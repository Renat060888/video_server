
#include "video_server_common/common/common_utils_specific.h"
#include "video_server_common/system/config_reader.h"
#include "storage/video_recorder.h"
#include "cmd_archive_stop.h"
#include "system/wal.h"
#include "video_source/source_manager.h"

using namespace std;
using namespace common_types;

CommandArchiveStop::CommandArchiveStop( SIncomingCommandServices * _services )
    : ICommandExternal( _services )
    , m_destroy(false)
{

}

bool CommandArchiveStop::exec(){

    if( CONFIG_PARAMS.RECORD_ENABLE ){
        if( 0 == (CONFIG_PARAMS.SYSTEM_SERVER_FEATURES & (int16_t)EServerFeatures::ARCHIVING) ){
            Json::Value response;
            response["cmd_type"] = "storage";
            response["cmd_name"] = "stop";
            response["archiving_id"] = m_archivingId;
            response["archiving_state"] = common_utils::convertArchivingStateToStr( EArchivingState::UNAVAILABLE );
            response["error_msg"] = "recording is not available on this video server";
            sendResponse( response, false );
            return false;
        }

        //
        if( CONFIG_PARAMS.RECORD_ENABLE_RETRANSLATION ){
            SVideoSource source;
            source.sensorId = m_services->videoRecorder->getArchiverState( m_archivingId ).initSettings->sensorId;
            m_services->sourceManager->getServiceOfSourceRetranslation()->stopRetranslation( source );
        }

        //
        WriteAheadLogger walForEndDump;
        walForEndDump.closeClientOperation( m_archivingId );

        //
        if( ! m_services->videoRecorder->stopRecording( m_archivingId, m_destroy ) ){
            Json::Value response;
            response["error_msg"] = m_services->videoRecorder->getLastError();
            sendResponse( response, false );
            return false;
        }

        //
        Json::Value response;
        response["cmd_type"] = "storage";
        response["cmd_name"] = "stop";        
        response["archiving_id"] = m_archivingId;
        response["archiving_state"] = common_utils::convertArchivingStateToStr( EArchivingState::READY );
        sendResponse( response, true );

        return true;
    }
    else{
        Json::Value response;
        response["error_msg"] = "recording is not available on this video server";
        sendResponse( response, false );
        return false;
    }
}
