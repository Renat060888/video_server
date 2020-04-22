
#include <microservice_common/system/logger.h>

#include "common/common_utils.h"
#include "pipeline_monitor.h"

using namespace std;

static constexpr const char * PRINT_HEADER = "PipelineMonitor:";

GstPadProbeReturn PipelineMonitor::callbackBufferListener( GstPad * pad, GstPadProbeInfo * info, gpointer user_data ){

    GstBuffer * buffer = GST_PAD_PROBE_INFO_BUFFER( info );

    VS_LOG_INFO << PRINT_HEADER << " buffer = "
             << " dts [" << buffer->dts << "]"
             << " pts [" << buffer->pts << "]"
             << " duration [" << buffer->duration << "]"
             << endl;





    return GST_PAD_PROBE_OK;
}

GstPadProbeReturn PipelineMonitor::callbackEventListener( GstPad * pad, GstPadProbeInfo * info, gpointer user_data ){

    GstEvent * event = GST_PAD_PROBE_INFO_EVENT( info );

    VS_LOG_INFO << PRINT_HEADER << " event = "
             << " seqnum [" << event->seqnum << "]"
             << " timestamp [" << event->timestamp << "]"
             << " type [" << gst_event_type_get_name( event->type ) << "]"
             << endl;







    return GST_PAD_PROBE_OK;
}

GstPadProbeReturn PipelineMonitor::callbackQueryListener( GstPad * pad, GstPadProbeInfo * info, gpointer user_data ){

    GstQuery * query = GST_PAD_PROBE_INFO_QUERY( info );

    VS_LOG_INFO << PRINT_HEADER << " query = "
             << " type [" << gst_query_type_get_name( query->type ) << "]"
             << endl;






    return GST_PAD_PROBE_OK;
}

/* called when we get a GstMessage from the source pipeline when we get EOS, we notify the appsrc of it. */
gboolean PipelineMonitor::callbackBusMessage( GstBus * _bus, GstMessage * _message, gpointer _userData ){

    switch( GST_MESSAGE_TYPE(_message) ){
    case GST_MESSAGE_EOS : {
        VS_LOG_INFO << PRINT_HEADER << " 'END OF STREAM' message"
                 << endl;
        break;
    }
    case GST_MESSAGE_ERROR : {
        GError * err;
        gchar * debug;
        gst_message_parse_error( _message, & err, & debug );

        VS_LOG_ERROR << PRINT_HEADER << " received error from GstObject = [" << _bus->object.name
                  << "] msg: [" << err->message
                  << "] Debug [" << debug
                  << "]"
                  << endl;

        g_error_free( err );
        g_free( debug );
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


PipelineMonitor::PipelineMonitor()
    : m_glibMainLoop(nullptr)
    , m_threadGLibMainLoop(nullptr)
    , m_gstPipeline(nullptr)
{

}

PipelineMonitor::~PipelineMonitor()
{
    if( m_settings.element || ! m_settings.pipeline.empty() ){
        stop();
    }
}

bool PipelineMonitor::launch( SInitSettings _settings ){

    // check
    if( (_settings.element && ! _settings.pipeline.empty()) || ( ! _settings.element && _settings.pipeline.empty()) ){
        VS_LOG_ERROR << PRINT_HEADER << "" << endl;
        return false;
    }

    // launch
    if( _settings.element ){
        return connectToElement( _settings, _settings.element );
    }
    else{
        return connectToPipeline( _settings, _settings.pipeline, _settings.elementsToListen );
    }

    m_settings = _settings;
    return true;
}


bool PipelineMonitor::connectToElement( const SInitSettings & _settings, GstElement * _element ){

    const string padName = "src";
    GstPad * padToListen = gst_element_get_static_pad( _element, padName.c_str() );
    if( ! padToListen ){
        VS_LOG_ERROR << PRINT_HEADER << " can't get pad [" << padName << "]" << endl;
        return false;
    }

    if( _settings.listenBuffers ){
        gst_pad_add_probe( padToListen, GST_PAD_PROBE_TYPE_BUFFER, (GstPadProbeCallback)callbackBufferListener, this, NULL );
    }
    if( _settings.listenEvents ){
        gst_pad_add_probe( padToListen, GST_PAD_PROBE_TYPE_EVENT_BOTH, (GstPadProbeCallback)callbackEventListener, this, NULL );
    }
    if( _settings.listenQueries ){
        gst_pad_add_probe( padToListen, GST_PAD_PROBE_TYPE_QUERY_BOTH, (GstPadProbeCallback)callbackQueryListener, this, NULL );
    }
    if( _settings.listenMessages ){
        // to be notified of messages from this pipeline, mostly EOS
        GstBus * bus = gst_element_get_bus( m_gstPipeline );
        gst_bus_add_watch( bus, callbackBusMessage, this );
        gst_object_unref( bus );
    }

    gst_object_unref( padToListen );

    return true;
}

bool PipelineMonitor::connectToPipeline( const SInitSettings & _settings, const std::string & _pipeline, const vector<string> & _elementsToListen ){

    // construct
    const string pipelineWithFakesink = _pipeline + " ! fakesink";
    m_gstPipeline = gst_parse_launch( pipelineWithFakesink.c_str(), NULL );
    if( ! m_gstPipeline ){
        VS_LOG_ERROR << PRINT_HEADER << "can't launch pipeline: [" << pipelineWithFakesink << "]" << endl;
        return false;
    }


    // connect to elements
    for( const string & elementName : _elementsToListen ){

        GstElement * streamerElement = gst_bin_get_by_name( GST_BIN(m_gstPipeline), elementName.c_str() );
        if( ! streamerElement ){
            VS_LOG_ERROR << PRINT_HEADER << " can't get element [" << elementName << "]" << endl;
            return false;
        }

        const string padName = "src";
        GstPad * padToListen = gst_element_get_static_pad( streamerElement, padName.c_str() );
        if( ! padToListen ){
            VS_LOG_ERROR << PRINT_HEADER << " can't get pad [" << padName << "]" << endl;
            return false;
        }

        if( _settings.listenBuffers ){
            gst_pad_add_probe( padToListen, GST_PAD_PROBE_TYPE_BUFFER, (GstPadProbeCallback)callbackBufferListener, this, NULL );
        }
        if( _settings.listenEvents ){
            gst_pad_add_probe( padToListen, GST_PAD_PROBE_TYPE_EVENT_BOTH, (GstPadProbeCallback)callbackEventListener, this, NULL );
        }
        if( _settings.listenQueries ){
            gst_pad_add_probe( padToListen, GST_PAD_PROBE_TYPE_QUERY_BOTH, (GstPadProbeCallback)callbackQueryListener, this, NULL );
        }
        if( _settings.listenMessages ){
            // to be notified of messages from this pipeline, mostly EOS
            GstBus * bus = gst_element_get_bus( m_gstPipeline );
            gst_bus_add_watch( bus, callbackBusMessage, this );
            gst_object_unref( bus );
        }

        gst_object_unref( padToListen );
    }


    // launch
    GstStateChangeReturn stateChanged = gst_element_set_state( m_gstPipeline, GST_STATE_READY );
    if( GST_STATE_CHANGE_FAILURE == stateChanged ){
        VS_LOG_ERROR << PRINT_HEADER << " pipeline GST_STATE_READY failed" << endl;
        return false;
    }

    stateChanged = gst_element_set_state( m_gstPipeline, GST_STATE_PLAYING );
    if( GST_STATE_CHANGE_FAILURE == stateChanged ){
        VS_LOG_ERROR << PRINT_HEADER << " pipeline GST_STATE_PLAYING failed" << endl;
        return false;
    }

    // let's run ! This loop will quit when the sink pipeline goes EOS or when an error occurs in the source or sink pipelines
    m_glibMainLoop = g_main_loop_new( NULL, FALSE );
    m_threadGLibMainLoop = new std::thread( g_main_loop_run, m_glibMainLoop );

    VS_LOG_INFO << PRINT_HEADER << " started on pipeline: [" << _pipeline << "]" << endl;

    return true;
}

void PipelineMonitor::stop(){

    // TODO: remove probs from element ?

    // stop pipeline
    if( m_gstPipeline ){
        gst_element_send_event( m_gstPipeline, gst_event_new_eos() );

        GstStateChangeReturn ret = gst_element_set_state( m_gstPipeline, GST_STATE_PAUSED );
        ret = gst_element_set_state( m_gstPipeline, GST_STATE_NULL );
        if( GST_STATE_CHANGE_FAILURE == ret ){
            VS_LOG_ERROR << PRINT_HEADER << " unable to set the pipeline to the null state"
                      << endl;
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

    // thread
    common_utils::threadShutdown( m_threadGLibMainLoop );

    VS_LOG_INFO << PRINT_HEADER << " stopped"
             << endl;
}








