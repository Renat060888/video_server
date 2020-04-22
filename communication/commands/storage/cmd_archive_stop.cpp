
#include "system/config_reader.h"
#include "system/system_environment_facade_vs.h"
#include "storage/video_recorder.h"
#include "datasource/source_manager_facade.h"
#include "cmd_archive_stop.h"

using namespace std;
using namespace common_types;

CommandArchiveStop::CommandArchiveStop( SIncomingCommandServices * _services )
    : ICommandExternal( _services )
    , m_destroy(false)
{

}

#include <jsoncpp/json/writer.h>

bool CommandArchiveStop::exec(){

    if( CONFIG_PARAMS.RECORD_ENABLE ){
        if( 0 == (CONFIG_PARAMS.baseParams.SYSTEM_SERVER_FEATURES & (int16_t)EServerFeatures::ARCHIVING) ){
            Json::Value response;
            response["cmd_type"] = "storage";
            response["cmd_name"] = "stop";
            response["archiving_id"] = m_archivingId;
            response["archiving_state"] = common_utils::convertArchivingStateToStr( EArchivingState::UNAVAILABLE );
            response["error_msg"] = "recording is not available on this video server";

            // TODO: remove after protocol refactor (response/body)
            Json::FastWriter writer;
            Json::Value root;
            root["response"] = "fail";
            root["body"] = response;
            //

            sendResponse( writer.write(root) );
            return false;
        }

        //
        if( CONFIG_PARAMS.RECORD_ENABLE_RETRANSLATION ){
            SVideoSource source;
            source.sensorId = ((SIncomingCommandServices *)m_services)->videoRecorder->getArchiverState( m_archivingId ).initSettings->sensorId;
            ((SIncomingCommandServices *)m_services)->sourceManager->getServiceOfSourceRetranslation()->stopRetranslation( source );
        }

        //
        ((SIncomingCommandServices *)m_services)->systemEnvironment->serviceForWriteAheadLogging()->closeClientOperation( m_archivingId );

        //
        if( ! ((SIncomingCommandServices *)m_services)->videoRecorder->stopRecording( m_archivingId, m_destroy ) ){
            Json::Value response;
            response["error_msg"] = ((SIncomingCommandServices *)m_services)->videoRecorder->getLastError();

            // TODO: remove after protocol refactor (response/body)
            Json::FastWriter writer;
            Json::Value root;
            root["response"] = "fail";
            root["body"] = response;
            //

            sendResponse( writer.write(root) );
            return false;
        }

        //
        Json::Value response;
        response["cmd_type"] = "storage";
        response["cmd_name"] = "stop";        
        response["archiving_id"] = m_archivingId;
        response["archiving_state"] = common_utils::convertArchivingStateToStr( EArchivingState::READY );

        // TODO: remove after protocol refactor (response/body)
        Json::FastWriter writer;
        Json::Value root;
        root["response"] = "success";
        root["body"] = response;
        //

        sendResponse( writer.write(root) );

        return true;
    }
    else{
        Json::Value response;
        response["error_msg"] = "recording is not available on this video server";

        // TODO: remove after protocol refactor (response/body)
        Json::FastWriter writer;
        Json::Value root;
        root["response"] = "fail";
        root["body"] = response;
        //

        sendResponse( writer.write(root) );
        return false;
    }
}
