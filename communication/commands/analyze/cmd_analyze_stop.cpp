
#include "analyze/analytic_manager.h"
#include "cmd_analyze_stop.h"
#include "storage/storage_engine.h"
#include "video_server_common/common/common_utils.h"
#include "video_server_common/common/common_utils_specific.h"
#include "video_server_common/system/config_reader.h"
#include "system/wal.h"

using namespace std;
using namespace common_types;

CommandAnalyzeStop::CommandAnalyzeStop( SIncomingCommandServices * _services )
    : ICommandExternal(_services)
    , m_sensorId(0)
    , destroy(false)
{

}

bool CommandAnalyzeStop::exec(){

    if( 0 == (CONFIG_PARAMS.SYSTEM_SERVER_FEATURES & (int16_t)EServerFeatures::ANALYZE) ){
        Json::Value jsonResponse;
        jsonResponse["cmd_type"] = "analyze";
        jsonResponse["cmd_name"] = "stop";
        jsonResponse["sensor_id"] = (unsigned long long)m_sensorId;
        jsonResponse["analyze_state"] = common_utils::convertAnalyzeStateToStr( EAnalyzeState::UNAVAILABLE );
        jsonResponse["processing_id"] = m_processingId;
        jsonResponse["error_msg"] = "analyze is not available on this video server";

        sendResponse( jsonResponse, false );

        return false;
    }

    if( destroy ){
        if( ! m_services->analyticManager->stopAnalyze(m_processingId) ){
            const string response = m_services->analyticManager->getLastError();
            sendResponse( response, false );
            return false;
        }

        //
        WriteAheadLogger walForEndDump;
        walForEndDump.closeClientOperation( m_processingId );

        //
        m_services->storageEngine->getServiceEventProcessor()->removeEventProcessor( m_processingId );
    }
    else{
        if( ! m_services->analyticManager->pauseAnalyze(m_processingId) ){
            const string response = m_services->analyticManager->getLastError();
            sendResponse( response, false );
            return false;
        }
    }

    Json::Value response;
    response["cmd_type"] = "analyze";
    response["cmd_name"] = "stop";    
    response["destroy_current_session"] = destroy;
    response["sensor_id"] = (unsigned long long)m_sensorId;
    response["processing_id"] = m_processingId;
    response["analyze_state"] = common_utils::convertAnalyzeStateToStr( EAnalyzeState::READY );
    sendResponse( response, true );

    return true;
}
