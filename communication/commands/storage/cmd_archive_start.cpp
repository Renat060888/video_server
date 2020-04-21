
#include "video_server_common/common/common_utils_specific.h"
#include "video_server_common/system/config_reader.h"
#include "system/wal.h"
#include "storage/video_recorder.h"
#include "cmd_archive_start.h"
#include "video_source/source_manager.h"

using namespace std;
using namespace common_types;

CommandArchiveStart::CommandArchiveStart( SIncomingCommandServices * _services )
    : ICommandExternal( _services )
{

}

bool CommandArchiveStart::exec(){

    if( CONFIG_PARAMS.RECORD_ENABLE ){
        if( 0 == (CONFIG_PARAMS.SYSTEM_SERVER_FEATURES & (int16_t)EServerFeatures::ARCHIVING) ){
            Json::Value response;
            response["cmd_type"] = "storage";
            response["cmd_name"] = "start";
            response["archiving_id"] = "0";
            response["sensor_id"] = (unsigned long long)m_sensorId;
            response["analyze_state"] = common_utils::convertArchivingStateToStr( EArchivingState::UNAVAILABLE );
            response["error_msg"] = "recording is not available on this video server";
            sendResponse( response, false );
            return false;
        }

        SVideoSource source;
        source.sensorId = m_sensorId;
        source.archivingName = m_archivingName;
        const string archivingId = ( ! m_archivingId.empty() ? m_archivingId : common_utils::generateUniqueId() );

        if( ! m_services->videoRecorder->startRecording( archivingId, source ) ){
            Json::Value response;
            response["error_msg"] = m_services->videoRecorder->getLastError();
            sendResponse( response, false );
            return false;
        }        

        //
        SWALClientOperation operation;
        operation.fullText = m_requestFullText;
        operation.uniqueKey = archivingId;
        operation.begin = true;
        operation.type = SWALClientOperation::EOperationType::ARCHIVE;

        WriteAheadLogger walForBeginDump;
        walForBeginDump.openArchiveOperation( operation );

        //
        Json::Value response;
        response["cmd_type"] = "storage";
        response["cmd_name"] = "start";
        response["sensor_id"] = (unsigned long long)m_sensorId;
        response["archiving_state"] = common_utils::convertArchivingStateToStr( EArchivingState::PREPARING );
        response["archiving_name"] = m_archivingName;
        response["archiving_id"] = archivingId;

        sendResponse( response, true );
        return true;
    }
    else{
        Json::Value response;
        response["error_msg"] = "recording disabled on this video server";
        sendResponse( response, false );
        return false;
    }    
}






