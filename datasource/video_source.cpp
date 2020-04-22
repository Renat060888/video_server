
#include <boost/format.hpp>
#include <microservice_common/system/logger.h>

#include "common/common_utils.h"
#include "video_source.h"

using namespace std;
using namespace common_types;

VideoSourceLocal::VideoSourceLocal()
    : m_gstPipeline(nullptr)
    , m_glibMainLoop(nullptr)
    , m_threadGLibMainLoop(nullptr)
    , m_referenceCounter(0)
{

}

VideoSourceLocal::~VideoSourceLocal()
{
    if( m_gstPipeline ){
        disconnect();
    }
}

void VideoSourceLocal::addObserver( ISourceObserver * _observer ){
    m_observers.push_back( _observer );
}

bool VideoSourceLocal::connect( const SInitSettings & _settings ){

    if( m_gstPipeline ){
        VS_LOG_WARN << "video source [" << _settings.source.sourceUrl << "] already connected" << endl;
        m_referenceCounter++;
        return true;
    }

    const string pipelineDscr = definePipelineDescription( _settings );
    if( pipelineDscr.empty() ){
        return false;
    }

    // launch
    GError * gerr = nullptr;
    m_gstPipeline = gst_parse_launch( pipelineDscr.c_str(), & gerr );
    if( gerr ){
        VS_LOG_ERROR << "VideoSource - couldn't construct pipeline: [1], gst message: [2] " << pipelineDscr << " " << gerr->message << endl;
        g_clear_error( & gerr );
        return false;
    }

    // to be notified of messages from this pipeline, mostly EOS
    GstBus * bus = gst_element_get_bus( m_gstPipeline );
    gst_bus_add_watch( bus, callback_gst_source_message, this );
    gst_object_unref( bus );

    // state
    gst_element_set_state( GST_ELEMENT( m_gstPipeline ), GST_STATE_PLAYING );

    // run loop
    m_glibMainLoop = g_main_loop_new( nullptr, FALSE );
    m_threadGLibMainLoop = new std::thread( g_main_loop_run, m_glibMainLoop );

    const string msg = ( boost::format( "Video Source started with follow pipeline [%1%]" )
                         % pipelineDscr
                         ).str();
    VS_LOG_INFO << msg << endl;

    m_settings = _settings;
    m_referenceCounter++;
    return true;
}

void VideoSourceLocal::disconnect(){

    if( --m_referenceCounter > 0 ){
        VS_LOG_WARN << "video source [" << m_settings.source.sourceUrl << "] still in use. Disconnect aborted" << endl;
        return;
    }

    // TODO: find out memory leak on delete Hls Wrapper
    if( ! m_gstPipeline ){
        return;
    }

    // stop pipeline
    gst_element_send_event( m_gstPipeline, gst_event_new_eos() );

    GstStateChangeReturn ret = gst_element_set_state( m_gstPipeline, GST_STATE_PAUSED );
    ret = gst_element_set_state( m_gstPipeline, GST_STATE_NULL );
    if( GST_STATE_CHANGE_FAILURE == ret ){
        VS_LOG_ERROR << "Unable to set the pipeline to the null state." << endl;
    }

    // destroy pipeline
    g_object_run_dispose( G_OBJECT( m_gstPipeline ) );
    gst_object_unref( m_gstPipeline );
    m_gstPipeline = nullptr;

    // destroy loop
    if( m_glibMainLoop ){
        g_main_loop_quit( m_glibMainLoop );
        g_main_loop_unref( m_glibMainLoop );
    }

    common_utils::threadShutdown( m_threadGLibMainLoop );

    VS_LOG_INFO << "Video Source - " << m_settings.source.sourceUrl << " is disconnected"
             << endl;
}

/* called when we get a GstMessage from the source pipeline when we get EOS, we notify the appsrc of it. */
gboolean VideoSourceLocal::callback_gst_source_message( GstBus * _bus, GstMessage * _message, gpointer _userData ){

    switch( GST_MESSAGE_TYPE(_message) ){
    case GST_MESSAGE_EOS : {
        g_print("The source got dry ('END OF STREAM' message)");
        break;
    }
    case GST_MESSAGE_ERROR : {

        GError * err;
        gchar * debug;
        gst_message_parse_error( _message, & err, & debug );

        const string messageFromDebug = debug;
        const string message = err->message;

        VS_LOG_ERROR << "Video Source received error from GstObject: [" << _bus->object.name
                  << "] msg: [" << err->message
                  << "] Debug [" << debug
                  << "]"
                  << endl;

        g_error_free( err );
        g_free( debug );

        if( messageFromDebug.find("Could not connect to server") != string::npos ||
            messageFromDebug.find("Failed to connect") != string::npos ||
            message.find("Could not read from resource") != string::npos
          ){

//            if( (( HLSCreator * )_userData)->getCurrentState().connectRetries < CONFIG_PARAMS.SYSTEM_CONNECT_RETRIES ){
//                (( HLSCreator * )_userData)->tryToReconnect();
//                (( HLSCreator * )_userData)->m_currentState.connectRetries++;
//            }
//            else{
//                LOG_ERROR << "connect retries count exhausted, stop stream recording" << endl;
//                g_main_loop_quit( (( HLSCreator * )_userData)->m_glibMainLoop );
//                (( HLSCreator * )_userData)->m_connectionLosted.store( true );
//            }
        }

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

ISourcesProvider::SSourceParameters VideoSourceLocal::getStreamParameters(){

    assert( false && "local version of VideoSource has no VideoReceiver" );
    return ISourcesProvider::SSourceParameters();
}

std::string VideoSourceLocal::definePipelineDescription( const SInitSettings & _settings ){

    const string channelName = _settings.source.sourceUrl;

    string source;
    // RTSP ( shared memory - video/x-raw )
    if( _settings.source.sourceUrl.find("rtsp://") != string::npos ){
        source = ( boost::format( "rtspsrc latency=%1% location=%2%"
                                  " ! decodebin"
                                  " ! videorate ! videoconvert ! video/x-raw,format=YV12,width=1920,height=1080,framerate=30/1"
                                  " ! shmsink socket-path=/tmp/socket_video_server_shm_test shm-size=300000000 wait-for-connection=false"
                                  )
                   % _settings.source.latencyMillisec
                   % _settings.source.sourceUrl
                   ).str();
    }
//    // RTSP ( shared memory - application/x-rtp )
//    if( _settings.sourceUrl.find("rtsp://") != string::npos ){
//        source = ( boost::format( "rtspsrc latency=%1% location=%2%"
//                                  " ! application/x-rtp"
//                                  " ! shmsink socket-path=/tmp/socket_video_server_rtpshm_test shm-size=300000000 wait-for-connection=false"
//                                  )
//                   % _settings.latencyMillisec
//                   % _settings.sourceUrl
//                   ).str();
//    }
//    // RTSP ( intervideo )
//    if( _settings.sourceUrl.find("rtsp://") != string::npos ){
//        source = ( boost::format( "rtspsrc latency=%1% location=%2%"
//                                  " ! decodebin"
//                                  " ! videorate ! videoconvert ! video/x-raw,format=YV12,width=1920,height=1080,framerate=30/1"
//                                  " ! intervideosink channel=%3%"
//                                  )
//                   % _settings.latencyMillisec
//                   % _settings.sourceUrl
//                   % channelName
//                   ).str();
//    }
    // HTTP
    else if( _settings.source.sourceUrl.find("http://") != string::npos ){

        // with id & password
        if( ! _settings.source.userId.empty() && ! _settings.source.userPassword.empty() ){
            source = ( boost::format( "souphttpsrc name=source is-live=true do-timestamp=true location=%1% user-id=%2% user-pw=%3% "
                                      "! decodebin "
                                      "! intervideosink channel=%4% "
                                      )
                       % _settings.source.sourceUrl
                       % _settings.source.userId
                       % _settings.source.userPassword
                       % channelName
                       ).str();
        }
        // without authentication
        else{
            source = ( boost::format( "souphttpsrc name=source location=%1% "
                                      "! decodebin "
                                      "! intervideosink channel=%2% "
                                      )
                       % _settings.source.sourceUrl
                       % channelName
                       ).str();
        }
    }
    else{
        VS_LOG_ERROR << "unsupported protocol:"
                  << " " << _settings.source.sourceUrl
                  << endl;
        return string();
    }

    return source;
}
