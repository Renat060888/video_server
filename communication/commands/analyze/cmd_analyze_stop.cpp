
#include "analyze/analytic_manager_facade.h"
#include "storage/storage_engine_facade.h"
#include "common/common_utils.h"
#include "system/config_reader.h"
#include "system/system_environment_facade_vs.h"
#include "cmd_analyze_stop.h"

using namespace std;
using namespace common_types;

CommandAnalyzeStop::CommandAnalyzeStop( SIncomingCommandServices * _services )
    : ICommandExternal(_services)
    , m_sensorId(0)
    , destroy(false)
{

}

#include <jsoncpp/json/writer.h>

bool CommandAnalyzeStop::exec(){

    if( 0 == (CONFIG_PARAMS.baseParams.SYSTEM_SERVER_FEATURES & (int16_t)EServerFeatures::ANALYZE) ){
        Json::Value jsonResponse;
        jsonResponse["cmd_type"] = "analyze";
        jsonResponse["cmd_name"] = "stop";
        jsonResponse["sensor_id"] = (unsigned long long)m_sensorId;
        jsonResponse["analyze_status"] = common_utils::convertAnalyzeStateToStr( EAnalyzeState::UNAVAILABLE );
        jsonResponse["processing_id"] = m_processingId;
        jsonResponse["error_msg"] = "analyze is not available on this video server";

        // TODO: remove after protocol refactor (response/body)
        Json::FastWriter writer;
        Json::Value root;
        root["response"] = "fail";
        root["body"] = jsonResponse;
        //

        sendResponse( writer.write(root) );

        return false;
    }

    if( destroy ){
        if( ! ((SIncomingCommandServices *)m_services)->analyticManager->stopAnalyze(m_processingId) ){
            const string response = ((SIncomingCommandServices *)m_services)->analyticManager->getLastError();

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
        ((SIncomingCommandServices *)m_services)->systemEnvironment->serviceForWriteAheadLogging()->closeClientOperation( m_processingId );

        //
        ((SIncomingCommandServices *)m_services)->storageEngine->getServiceEventProcessor()->removeEventProcessor( m_processingId );
    }
    else{
        if( ! ((SIncomingCommandServices *)m_services)->analyticManager->pauseAnalyze(m_processingId) ){
            const string response = ((SIncomingCommandServices *)m_services)->analyticManager->getLastError();

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

    Json::Value response;
    response["cmd_type"] = "analyze";
    response["cmd_name"] = "stop";    
    response["destroy_current_session"] = destroy;
    response["sensor_id"] = (unsigned long long)m_sensorId;
    response["processing_id"] = m_processingId;
    response["analyze_status"] = common_utils::convertAnalyzeStateToStr( EAnalyzeState::READY );

    // TODO: remove after protocol refactor (response/body)
    Json::FastWriter writer;
    Json::Value root;
    root["response"] = "success";
    root["body"] = response;
    //

    sendResponse( writer.write(root) );

    return true;
}
