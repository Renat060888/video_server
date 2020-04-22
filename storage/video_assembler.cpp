
#include <string>

#include <boost/format.hpp>
#include <objrepr/reprServer.h>
#include <microservice_common/system/logger.h>

#include "common/common_utils.h"
#include "communication/protocols/video_receiver_protocol.h"
#include "video_assembler.h"

using namespace std;
using namespace common_types;

static constexpr const char * PRINT_HEADER = "VideoAsm:";

VideoAssembler::VideoAssembler()
{

}

VideoAssembler::~VideoAssembler()
{
    PNetworkProvider provider = std::dynamic_pointer_cast<INetworkProvider>( m_settings.communicationWithVideoSource );
    provider->removeObserver( this );
}

void VideoAssembler::callbackNetworkRequest( PEnvironmentRequest _request ){

    if( _request->getIncomingMessage().empty() ){
        return;
    }

    if( _request->getIncomingMessage().find("\"answer\":\"duration\"") != std::string::npos ){
        return;
    }

    const string json = _request->getIncomingMessage();

    const AnswerType answer = VideoReceiverProtocol::singleton().getAnswerType( json );

    switch( answer ){
    case AnswerType::AT_STREAMDATA: {
        break;
    }
    case AnswerType::AT_PLUGINSDATA: {
        break;
    }
    case AnswerType::AT_DURATION : {
        FrameOptions fo = VideoReceiverProtocol::singleton().getFrameOptions( json );
        break;
    }
    case AnswerType::AT_RESPONSE : {
        break;
    }
    case AnswerType::AT_RECORD : {
        RecordData rd = VideoReceiverProtocol::singleton().getRecordData( json );
        const string & taskId = rd.tag;

        m_muAssembledVideos.lock();
        auto iter = m_assembledVideosByTaskId.find( taskId );
        if( iter != m_assembledVideosByTaskId.end() ){
            SAssembledVideo * video = iter->second;
            video->location = rd.recordPath;
            video->ready = true;
        }
        else{
            VS_LOG_ERROR << PRINT_HEADER << " such task id not found [" << taskId << "]" << endl;
        }
        m_muAssembledVideos.unlock();

        break;
    }
    case AnswerType::AT_EVENT : {
        break;
    }
    case AnswerType::AT_INFO : {
        break;
    }
    case AnswerType::AT_ANALYTIC : {
        AnalyticData ad = VideoReceiverProtocol::singleton().getAnalyticData( json );
        break;
    }
    default : {
        break;
    }
    }
}

bool VideoAssembler::init( SInitSettings _settings ){

    m_settings = _settings;

    PNetworkProvider provider = std::dynamic_pointer_cast<INetworkProvider>( m_settings.communicationWithVideoSource );
    provider->addObserver( this );


    VS_LOG_INFO << PRINT_HEADER << " init success for procId [" << _settings.correlatedProcessingId << "]" << endl;
    return true;
}


bool VideoAssembler::run( uint64_t _objreprObjectId ){

    objrepr::SpatialObjectManager * objManager = objrepr::RepresentationServer::instance()->objectManager();
    objrepr::SpatialObjectPtr object = objManager->getObject( _objreprObjectId );

    // time validation
    if( (object->lifetimeEnd() - object->lifetimeBegin()) <= 0 ){
        VS_LOG_ERROR << PRINT_HEADER
                     << " invalid object lifetime for video assembling!"
                     << " obj id: " << _objreprObjectId
                     << " lifetime range begin: " << object->lifetimeBegin()
                     << " end: " << object->lifetimeEnd()
                     << endl;
        return false;
    }

    // send command to video-receiver
    if( object ){
        const uint64_t backwardSec = object->lifetimeEnd() - object->lifetimeBegin();
        const uint64_t forwardSec = 0;
        const string path = "";
        const string taskId = common_utils::generateUniqueId();

//        LOG_INFO << PRINT_HEADER << " send command to save fragment"
//                 << " for object [" << object->id() << "]"
//                 << " with task id [" << taskId << "]"
//                 << " backward [" << backwardSec << "] sec,"
//                 << " forward [" << common_utils::timeMillisecToStr( forwardSec * 1000 ) << "] sec"
//                 << endl;

        const string msgToSend = VideoReceiverProtocol::singleton().createRecordCommand( path,
                                                                                         backwardSec,
                                                                                         forwardSec,
                                                                                         taskId );

        //
        VideoAssembler::SAssembledVideo * assembledVideo = new VideoAssembler::SAssembledVideo();
        assembledVideo->taskId = taskId;
        assembledVideo->forObjectId = object->id();

        m_muAssembledVideos.lock();
        m_assembledVideosByObjectId.insert( {object->id(), assembledVideo} );
        m_assembledVideosByTaskId.insert( {taskId, assembledVideo} );
        m_muAssembledVideos.unlock();

        //
        PEnvironmentRequest request = m_settings.communicationWithVideoSource->getRequestInstance();
        request->sendOutcomingMessage( msgToSend, true );
        return true;
    }
    else{
        VS_LOG_ERROR << PRINT_HEADER << " unknown objrepr id [" << _objreprObjectId << "] Assembly cancelled" << endl;
        return false;
    }
}

std::vector<VideoAssembler::SAssembledVideo *> VideoAssembler::getReadyVideos(){

    std::vector<SAssembledVideo *> out;

    m_muAssembledVideos.lock();
    for( auto iter = m_assembledVideosByObjectId.begin(); iter != m_assembledVideosByObjectId.end(); ){
        SAssembledVideo * video = iter->second;
        if( video->ready ){
            out.push_back( video );

            // NOTE: remove from local store, but NOT destroy
            iter = m_assembledVideosByObjectId.erase( iter );
            m_assembledVideosByTaskId.erase( video->taskId );
        }
        else{
            ++iter;
        }
    }
    m_muAssembledVideos.unlock();

    return out;
}

bool VideoAssembler::run(){

    std::string location;
    std::string chunk_template;
    std::string target;

    const string pipelineDscr = ( boost::format(  "splitmuxsrc location=%1%/%2% "
                            "! h264parse "
                            "! mp4mux "
                            "! filesink location=%3% "
                                                )
                                 % location
                                 % chunk_template
                                 % target
                                ).str();

    GError * gerr = nullptr;
    GstElement * pipeline = gst_parse_launch( pipelineDscr.c_str(), & gerr );
    if( gerr ){
        VS_LOG_ERROR << "VideoAssembler - couldn't construct pipeline: " << pipelineDscr
                  << "error message: " << gerr->message
                  << endl;
        g_clear_error( & gerr );
        return false;
    }

    gst_element_set_state( GST_ELEMENT( pipeline ), GST_STATE_PLAYING );

    GMainLoop * loop = g_main_loop_new( NULL, FALSE );
    if( loop ){
        GstBus * bus = gst_pipeline_get_bus( GST_PIPELINE( pipeline ) );
        gst_bus_add_watch( bus, VideoAssembler::myBusCallback, loop );
        g_main_loop_run( loop );
    }
    else{
        VS_LOG_ERROR << "can't start GSreamer loop thread"
                  << endl;
        return false;
    }

    gst_element_set_state( pipeline, GST_STATE_NULL );
    gst_object_unref( pipeline );
    g_main_loop_unref( loop );

    return true;
}

gboolean VideoAssembler::myBusCallback( GstBus * bus, GstMessage * message, gpointer uData ){

    VS_LOG_ERROR << "VideoAssembler - message from bus"
              << endl;

    return true;
}

