
#include "system/config_reader.h"
#include "system/system_environment_facade_vs.h"
#include "storage/video_recorder.h"
#include "datasource/source_manager_facade.h"
#include "cmd_archive_start.h"

using namespace std;
using namespace common_types;

CommandArchiveStart::CommandArchiveStart( SIncomingCommandServices * _services )
    : ICommandExternal( _services )
{

}

#include <jsoncpp/json/writer.h>

bool CommandArchiveStart::exec(){

    if( CONFIG_PARAMS.RECORD_ENABLE ){
        if( 0 == (CONFIG_PARAMS.baseParams.SYSTEM_SERVER_FEATURES & (int16_t)EServerFeatures::ARCHIVING) ){
            Json::Value response;
            response["cmd_type"] = "storage";
            response["cmd_name"] = "start";
            response["archiving_id"] = "0";
            response["sensor_id"] = (unsigned long long)m_sensorId;
            response["analyze_state"] = common_utils::convertArchivingStateToStr( EArchivingState::UNAVAILABLE );
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

        SVideoSource source;
        source.sensorId = m_sensorId;
        source.archivingName = m_archivingName;
        const string archivingId = ( ! m_archivingId.empty() ? m_archivingId : common_utils::generateUniqueId() );

        if( ! ((SIncomingCommandServices *)m_services)->videoRecorder->startRecording( archivingId, source ) ){
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
        SWALClientOperation operation;
        operation.commandFullText = m_requestFullText;
        operation.uniqueKey = archivingId;
        operation.begin = true;        
        ((SIncomingCommandServices *)m_services)->systemEnvironment->serviceForWriteAheadLogging()->openOperation( operation );

        //
        Json::Value response;
        response["cmd_type"] = "storage";
        response["cmd_name"] = "start";
        response["sensor_id"] = (unsigned long long)m_sensorId;
        response["archiving_state"] = common_utils::convertArchivingStateToStr( EArchivingState::PREPARING );
        response["archiving_name"] = m_archivingName;
        response["archiving_id"] = archivingId;

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
        response["error_msg"] = "recording disabled on this video server";

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






