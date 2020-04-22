
#include <iostream>

#include <microservice_common/system/logger.h>

#include "system/config_reader.h"
#include "common/common_utils.h"
#include "system/objrepr_bus_vs.h"
#include "analyzer.h"

using namespace std;
using namespace common_types;

// NOTE: must be equal to global variables in top of [plugin_interface/ianalytic_plugin.h]
constexpr const char * GLIB_SIGNAL_NAME_EVENT_MESSAGE = "event-message";
constexpr const char * GLIB_SIGNAL_NAME_INSTANT_EVENT_MESSAGE = "instant-event-message";
constexpr const char * GLIB_SIGNAL_NAME_SERVICE_MESSAGE = "service-message";

static const std::map<string, string> OUTPUT_ENCODINGS_FROM_RTP = { // pt=96
    {"H264", "rtph264depay ! decodebin"},
    {"JPEG", "rtpjpegdepay ! decodebin"},
    {"MP4V-ES", "rtpmp4vdepay ! decodebin"}
                                                                  };

AnalyzerLocal::AnalyzerLocal()
    : m_glibMainLoop(nullptr)
    , m_gstPipeline(nullptr)
    , m_threadGLibMainLoop(nullptr)
    , m_lastError("no_errors")
    , m_pluginGetReadyFailed(false)
{
    m_accumulatedEvents.reserve( 1024 );

}

AnalyzerLocal::~AnalyzerLocal()    
{    
    stop();
}

bool AnalyzerLocal::start( SInitSettings _settings ){    

    m_settings = _settings;

    // TODO: very tiny place, easy to forget some field
    m_status.analyzeState = EAnalyzeState::PREPARING;
    m_status.sensorId = _settings.sensorId;
    m_status.processingId = _settings.processingId;
    m_status.processingName = _settings.processingName;
    m_status.profileId = _settings.profileId;

    if( _settings.plugins.empty() ){
        VS_LOG_ERROR << "no one processing plugin is given for sensor id [" << _settings.sensorId << "]" << endl;
        return false;
    }

    const string pipelineDscr = definePipelineDescription( _settings );
    if( pipelineDscr.empty() ){
        // TODO: set error
        return false;
    }

    m_gstPipeline = gst_parse_launch( pipelineDscr.c_str(), NULL );
    if( ! m_gstPipeline ){        
        // TODO: set error
        return false;
    }

    VS_LOG_INFO << "analyzer pipeline started [" << pipelineDscr << "]" << endl;

    // to be notified of messages from this pipeline, mostly EOS
    GstBus * bus = gst_element_get_bus( m_gstPipeline );
    gst_bus_add_watch( bus, callbackGstSourceMessage, this );
    gst_object_unref( bus );

    // subscribe signal from plugins
    // TODO: only ONE plugin
    for( const SPluginMetadata & plugMeta : _settings.plugins ){
        GstElement * pluginElement = gst_bin_get_by_name( GST_BIN( m_gstPipeline ), plugMeta.GStreamerElementName.c_str() );

        if( ! pluginElement ){
            VS_LOG_ERROR << "Analyzer: bin not found by plugin name " << plugMeta.GStreamerElementName << endl;
            m_status.analyzeState = EAnalyzeState::READY;
            return false;
        }

        g_signal_connect( pluginElement, GLIB_SIGNAL_NAME_EVENT_MESSAGE, G_CALLBACK(& callbackEventFromSink), this );
        g_signal_connect( pluginElement, GLIB_SIGNAL_NAME_INSTANT_EVENT_MESSAGE, G_CALLBACK(& callbackInstantEventFromSink), this );
        g_signal_connect( pluginElement, GLIB_SIGNAL_NAME_SERVICE_MESSAGE, G_CALLBACK(& callbackServiceFromSink), this );
    }

    // set initial properties
    m_settings.pointerContainer.sensorId = _settings.sensorId;
    m_settings.pointerContainer.processingFlags = _settings.processingFlags;
    m_settings.pointerContainer.allowedDpfLabelObjreprClassinfo = _settings.allowedDpfLabelObjreprClassinfo;
    m_settings.pointerContainer.dpfProcessorPtr = _settings.dpfProcessorPtr;
    m_settings.pointerContainer.usedVideocardIdx = _settings.usedVideocardIdx;
    m_settings.pointerContainer.maxObjectsForBestImageTracking = _settings.maxObjectsForBestImageTracking;
    m_settings.pointerContainer.sessionNum = _settings.currentSession;

    GValue customPointerVal = G_VALUE_INIT;
    g_value_init( & customPointerVal, G_TYPE_POINTER );
    g_value_set_pointer( & customPointerVal, (gpointer)(& m_settings.pointerContainer) );

    const SPluginMetadata & plugMeta = _settings.plugins.front();
    GstElement * pluginElement = gst_bin_get_by_name( GST_BIN( m_gstPipeline ), plugMeta.GStreamerElementName.c_str() );
    g_object_set_property( G_OBJECT(pluginElement), "custom_pointer", & customPointerVal );

    // controls plugin via state
    GstStateChangeReturn stateChanged = gst_element_set_state( m_gstPipeline, GST_STATE_READY );

    if( GST_STATE_CHANGE_FAILURE == stateChanged || m_pluginGetReadyFailed ){
        VS_LOG_ERROR << "Analyzer: plugin GET_READY failed" << endl;
        m_lastError = m_errorFromPlugin;
        m_status.analyzeState = EAnalyzeState::CRUSHED;
        return false;
    }

    stateChanged = gst_element_set_state( m_gstPipeline, GST_STATE_PLAYING );

    // let's run !, this loop will quit when the sink pipeline goes EOS or when an
    // error occurs in the source or sink pipelines.
//    m_glibMainLoop = g_main_loop_new( NULL, FALSE );
//    m_threadGLibMainLoop = new std::thread( g_main_loop_run, m_glibMainLoop );

    m_status.analyzeState = EAnalyzeState::ACTIVE;

    VS_LOG_INFO << "Analyzer - sensor [" << m_settings.sensorId << "] is playing."
             << " DdpProcessorPtr [" << std::hex << m_settings.pointerContainer.dpfProcessorPtr << "]"
             << endl;

    return true;
}

void AnalyzerLocal::resume(){

    m_settings.currentSession++;
    m_settings.pointerContainer.sessionNum = m_settings.currentSession;

    GstStateChangeReturn ret = gst_element_set_state( m_gstPipeline, GST_STATE_PLAYING );
    if( GST_STATE_CHANGE_FAILURE == ret ){
        VS_LOG_ERROR << "Unable to set the Plugin pipeline to the PLAYING (resume) state." << endl;
    }

    m_status.analyzeState = EAnalyzeState::ACTIVE;

    VS_LOG_INFO << "Analyzer - sensor [" << m_settings.sensorId << "] is resumed"
             << endl;
}

void AnalyzerLocal::pause(){

    GstStateChangeReturn ret = gst_element_set_state( m_gstPipeline, GST_STATE_PAUSED );
    if( GST_STATE_CHANGE_FAILURE == ret ){
        VS_LOG_ERROR << "Unable to set the Plugin pipeline to the PAUSE state." << endl;
    }

    m_status.analyzeState = EAnalyzeState::READY;

    VS_LOG_INFO << "Analyzer - sensor [" << m_settings.sensorId << "] is paused"
             << endl;
}

void AnalyzerLocal::stop(){

    if( m_gstPipeline ){
        // stop pipeline
        gst_element_send_event( m_gstPipeline, gst_event_new_eos() );

        GstStateChangeReturn ret = gst_element_set_state( m_gstPipeline, GST_STATE_PAUSED );
        ret = gst_element_set_state( m_gstPipeline, GST_STATE_NULL );
        if( GST_STATE_CHANGE_FAILURE == ret ){
            VS_LOG_ERROR << "Unable to set the Plugin pipeline to the null state." << endl;
        }

        // destroy pipeline
        g_object_run_dispose( G_OBJECT( m_gstPipeline ));
        gst_object_unref( m_gstPipeline );
        m_gstPipeline = nullptr;
    }

    // destroy loop
    if( m_glibMainLoop ){
        g_main_loop_quit( m_glibMainLoop );
        g_main_loop_unref( m_glibMainLoop );
        m_glibMainLoop = nullptr;
    }

    common_utils::threadShutdown( m_threadGLibMainLoop );

    VS_LOG_INFO << "Analyzer - sensor [" << m_settings.sensorId << "] is stopped"
             << endl;
}

void AnalyzerLocal::setLivePlaying( bool _live ){

    //
    m_settings.pointerContainer.livePlaying = _live;

    //
//    for( auto & valuePair : m_settings.pointerContainer.currentObjects ){
//        objrepr::SpatialObjectPtr & obj = valuePair.second;
//        obj->setTemporalState( objrepr::SpatialObject::TemporalState::TS_Absent );
//        obj->push();
//    }
}

std::vector<SAnalyticEvent> AnalyzerLocal::getAccumulatedEvents(){    

    std::vector<SAnalyticEvent> out;

    m_mutexEvent.lock();
    m_accumulatedEvents.swap( out );
    m_mutexEvent.unlock();

    return out;
}

std::vector<SAnalyticEvent> AnalyzerLocal::getAccumulatedInstantEvents(){

    std::vector<SAnalyticEvent> out;

    m_mutexEvent.lock();
    m_accumulatedInstantEvents.swap( out );
    m_mutexEvent.unlock();

    return out;
}

/* called when the appsink notifies us that there is a new buffer ready for processing */
GstFlowReturn AnalyzerLocal::callbackEventFromSink( GstElement * _element, char * _elementMessage, gpointer _data ){

    const string elementName = GST_ELEMENT_NAME( _element );    

    assert( _elementMessage && "empty _elementMessage" );

    // TODO: move to callback_gst_service_from_sink()
    if( std::string(_elementMessage).find("give me a properties!") != std::string::npos ){
        return GstFlowReturn();
    }

//    VS_LOG_DBG << "event from plugin [" << _elementMessage << "]" << endl;

    SAnalyticEvent event;
    event.ctxId = OBJREPR_BUS.getCurrentContextId();
    event.sensorId = ((AnalyzerLocal *)_data )->m_status.sensorId;
    event.eventMessage = _elementMessage;
    event.pluginName = elementName;
    event.processingId = ((AnalyzerLocal *)_data )->m_status.processingId;

    ((AnalyzerLocal *)_data )->m_mutexEvent.lock();
    ((AnalyzerLocal *)_data )->m_accumulatedEvents.push_back( event );
    ((AnalyzerLocal *)_data )->m_mutexEvent.unlock();

    return GstFlowReturn();
}

GstFlowReturn AnalyzerLocal::callbackInstantEventFromSink( GstElement * _element, char * _elementMessage, gpointer _data ){

    const string elementName = GST_ELEMENT_NAME( _element );

    assert( _elementMessage && "empty _elementMessage" );

    // TODO: move to callback_gst_service_from_sink()
    if( std::string(_elementMessage).find("give me a properties!") != std::string::npos ){
        return GstFlowReturn();
    }

    SAnalyticEvent event;
    event.ctxId = OBJREPR_BUS.getCurrentContextId();
    event.sensorId = ((AnalyzerLocal *)_data )->m_status.sensorId;
    event.eventMessage = _elementMessage;
    event.pluginName = elementName;
    event.processingId = ((AnalyzerLocal *)_data )->m_status.processingId;

    ((AnalyzerLocal *)_data )->m_mutexEvent.lock();
    ((AnalyzerLocal *)_data )->m_accumulatedInstantEvents.push_back( event );
    ((AnalyzerLocal *)_data )->m_mutexEvent.unlock();

    return GstFlowReturn();
}

GstFlowReturn AnalyzerLocal::callbackServiceFromSink( GstElement * _element, char * _elementMessage, gpointer _data ){

    const string elementName = GST_ELEMENT_NAME( _element );

    VS_LOG_ERROR << "get service signal from plugin [" << elementName << "]"
              << " message [" << _elementMessage << "]"
              << std::endl;

    // TODO: parse json message from Streamer Plugin

    ((AnalyzerLocal *)_data )->m_pluginGetReadyFailed = true;
    ((AnalyzerLocal *)_data )->m_errorFromPlugin = _elementMessage;

    return GstFlowReturn();
}

/* called when we get a GstMessage from the source pipeline when we get EOS, we notify the appsrc of it. */
gboolean AnalyzerLocal::callbackGstSourceMessage( GstBus * _bus, GstMessage * _message, gpointer _userData ){

    switch( GST_MESSAGE_TYPE(_message) ){
    case GST_MESSAGE_EOS : {
        g_print("The source got dry ('END OF STREAM' message)");
        break;
    }
    case GST_MESSAGE_ERROR : {
        GError * err;
        gchar * debug;
        gst_message_parse_error( _message, & err, & debug );

        VS_LOG_ERROR << "Analyzer received error from GstObject: [" << _bus->object.name
                  << "] msg: [" << err->message
                  << "] Debug [" << debug
                  << "]"
                  << endl;

        g_error_free( err );
        g_free( debug );

        g_main_loop_quit( (( AnalyzerLocal * )_userData)->m_glibMainLoop );

        (( AnalyzerLocal * )_userData)->m_status.analyzeState = EAnalyzeState::CRUSHED;
        break;
    }
    case GST_MESSAGE_STATE_CHANGED : {

        break;
    }
    case GST_MESSAGE_BUFFERING : {

        break;
    }
    case GST_MESSAGE_ELEMENT : {

        break;
    }
    default:
        break;
    }

    return TRUE;
}

std::string AnalyzerLocal::definePipelineDescription( const SInitSettings & _settings ){

#ifdef SEPARATE_SINGLE_SOURCE
#ifdef REMOTE_PROCESSING
    string pipeline = "shmsrc socket-path=%1% do-timestamp=true is-live=true name=shmsrc_raw ! %2% ! %3%";
    pipeline += " ! videoconvert";

    vector<string> pluginsLaunchString = getCustomPluginsLaunchString( _settings.plugins );
    for( const string & pluginLaunchStr : pluginsLaunchString ){
        pipeline += " ! " + pluginLaunchStr;
    }

    if( CONFIG_PARAMS.PROCESSING_ENABLE_RECTS_ON_TEST_SCREEN ){
        pipeline += " ! videoconvert ! autovideosink";
    }
    else{
        pipeline += " ! videoconvert ! fakesink";
    }

    string encodingFromRtp;
    auto iter = OUTPUT_ENCODINGS_FROM_RTP.find( _settings.shmRtpStreamEncoding );
    if( iter != OUTPUT_ENCODINGS_FROM_RTP.end() ){
        encodingFromRtp = iter->second;
    }
    else{
        VS_LOG_ERROR << "payloader element not found for encoding: [" << _settings.shmRtpStreamEncoding << "]"
                  << endl;
        return string();
    }

    string out = ( boost::format( pipeline.c_str() )
//               % PATH_LOCATOR.getVideoReceiverRtpPayloadSocket( _settings.sourceUrl )
               % _settings.VideoReceiverRtpPayloadSocket
               % _settings.shmRtpStreamCaps
               % encodingFromRtp
               ).str();
#else
    string pipeline = "intervideosrc channel=" + _settings.sourceUrl;
    pipeline += " ! videoconvert";

    vector<string> pluginsLaunchString = getCustomPluginsLaunchString( _settings.plugins );
    for( const string & pluginLaunchStr : pluginsLaunchString ){
        pipeline += " ! " + pluginLaunchStr;
    }

    pipeline += " ! videoconvert ! fakesink";
#endif
#else
    // setting up source pipeline
    string pipeline = "rtspsrc latency=0 location=" + _settings.sourceUrl;
    // TODO: not only real-time via RTSP, but also via HLS reading & HTTP
//    string pipeline2 = createHeadOfLaunchString( _settings );

    pipeline += " ! decodebin ! videoconvert";

    // insert plugins into pipeline
    vector<string> pluginsLaunchString = getCustomPluginsLaunchString( _settings.plugins );
    for( const string & pluginLaunchStr : pluginsLaunchString ){
        pipeline += " ! " + pluginLaunchStr;
    }

    // fakesink for clear frames after plugins
    pipeline += " ! videoconvert ! fakesink";
#endif

    return out;
}

string AnalyzerLocal::createHeadOfLaunchString( const SInitSettings & _settings ){

    string out;



    return out;
}

vector<string> AnalyzerLocal::getCustomPluginsLaunchString( const vector<SPluginMetadata> & _pluginsMetadata ){

    vector<string> out;

    for( const SPluginMetadata & plugMeta : _pluginsMetadata ){

        string pluginLaunchString = plugMeta.pluginStaticName +
                                    " name=" + plugMeta.GStreamerElementName;

        for( const SPluginParameter & param : plugMeta.params ){
            pluginLaunchString += " " + param.key + "=" + param.value;
        }

        out.push_back( pluginLaunchString );
    }

    return out;
}
