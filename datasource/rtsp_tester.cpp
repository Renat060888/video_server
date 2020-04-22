
#include <boost/format.hpp>
#include <microservice_common/system/logger.h>

#include "common/common_utils.h"
#include "rtsp_tester.h"

using namespace std;

RTSPTester::RTSPTester() :
    m_gstPipeline( nullptr ),
    m_gLoop( nullptr ),
    m_threadGstLoop( nullptr )
{

}

RTSPTester::~RTSPTester()
{
    if( ! m_gstPipeline ){
        return;
    }

    gst_element_send_event( m_gstPipeline, gst_event_new_eos() );

    GstStateChangeReturn ret = gst_element_set_state(m_gstPipeline, GST_STATE_PAUSED);
    ret = gst_element_set_state(m_gstPipeline, GST_STATE_NULL);
    if( GST_STATE_CHANGE_FAILURE == ret ){
        VS_LOG_ERROR << "Unable to set the pipeline to the null state." << endl;
    }

    // TODO: try find memory leak on delete Hls Wrapper

    g_object_run_dispose( G_OBJECT( m_gstPipeline ));
    gst_object_unref( m_gstPipeline );
    m_gstPipeline = NULL;

    g_main_loop_quit( m_gLoop );
    g_main_loop_unref( m_gLoop );

    common_utils::threadShutdown( m_threadGstLoop );
}

bool RTSPTester::test( const std::string & _sourceUrl ){

    const string pipelineDscr = definePipelineDescription( _sourceUrl );

    // launch
    GError * gerr = nullptr;
    m_gstPipeline = gst_parse_launch( pipelineDscr.c_str(), & gerr );
    if( gerr ){
        VS_LOG_ERROR << "gst_parse_launch() : couldn't construct pipeline: [1], gst message: [2] " << pipelineDscr << " " << gerr->message << endl;
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
    m_gLoop = g_main_loop_new( nullptr, FALSE );
    m_threadGstLoop = new std::thread( g_main_loop_run, m_gLoop );

    const string msg = ( boost::format( "GST HLS pipeline started as: url [%1%] with source [%2%]" )
                         % _sourceUrl
                         % pipelineDscr
                         ).str();
    VS_LOG_INFO << msg << endl;

    return true;
}

/* called when we get a GstMessage from the source pipeline when we get EOS, we notify the appsrc of it. */
gboolean RTSPTester::callback_gst_source_message( GstBus * bus, GstMessage * message, gpointer user_data ){

    switch( GST_MESSAGE_TYPE(message) ){
    case GST_MESSAGE_EOS:
        g_print("The source got dry ('END OF STREAM' message)");
        break;
    case GST_MESSAGE_ERROR:

        GError * err;
        gchar * debug;
        gst_message_parse_error( message, & err, & debug );

        VS_LOG_ERROR << "RTSP Tester received error from GstObject: " << bus->object.name
                  << ". Next msg: " << err->message
                  << ". Debug: " << debug
                  << endl;

        g_main_loop_quit( (( RTSPTester * )user_data)->m_gLoop );

        break;
    default:
        break;
    }

    return TRUE;
}

std::string RTSPTester::definePipelineDescription( const std::string & _sourceUrl ){

    string out;

    out = ( boost::format( "rtspsrc location=%1% ! fakesink" )
            % _sourceUrl
          ).str();

    return out;
}
