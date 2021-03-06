
#include <objrepr/reprServer.h>
#include <microservice_common/system/logger.h>

#include "common/common_utils.h"
#include "system/config_reader.h"
#include "system/objrepr_bus_vs.h"
#include "system/system_environment_facade_vs.h"
#include "storage/storage_engine_facade.h"
#include "analyze/analytic_manager_facade.h"
#include "cmd_analyze_start.h"

using namespace std;
using namespace common_types;

CommandAnalyzeStart::CommandAnalyzeStart( SIncomingCommandServices * _services )
    : ICommandExternal(_services)
    , m_sensorId(0)
    , m_profileId(0)
    , resume(false)
{

}

#include <jsoncpp/json/writer.h>

bool CommandAnalyzeStart::exec(){

    if( 0 == (CONFIG_PARAMS.baseParams.SYSTEM_SERVER_FEATURES & (int16_t)EServerFeatures::ANALYZE) ){
        Json::Value jsonResponse;
        jsonResponse["cmd_type"] = "analyze";
        jsonResponse["cmd_name"] = "start";
        jsonResponse["sensor_id"] = (unsigned long long)m_sensorId;
        jsonResponse["analyze_status"] = common_utils::convertAnalyzeStateToStr( EAnalyzeState::UNAVAILABLE );
        jsonResponse["processing_id"] = "0";
        jsonResponse["error_msg"] = "this server instance not allows analyze";

        // TODO: remove after protocol refactor (response/body)
        Json::FastWriter writer;
        Json::Value root;
        root["response"] = "fail";
        root["body"] = jsonResponse;
        //

        sendResponse( writer.write(root) );

        return false;
    }

    if( resume ){
        ((SIncomingCommandServices *)m_services)->analyticManager->resumeAnalyze( m_processingId );

        Json::Value response;
        response["cmd_type"] = "analyze";
        response["cmd_name"] = "start";
        response["sensor_id"] = (unsigned long long)m_sensorId;
        response["analyze_status"] = common_utils::convertAnalyzeStateToStr( EAnalyzeState::PREPARING );
        response["processing_id"] = m_processingId;
        response["resume_current_session"] = resume;

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
        SPluginParameter pluginParam2;
        pluginParam2.key = "zone_x";
        pluginParam2.value = to_string( zoneRect[ 0 ] );

        SPluginParameter pluginParam3;
        pluginParam3.key = "zone_y";
        pluginParam3.value = to_string( zoneRect[ 1 ] );

        SPluginParameter pluginParam4;
        pluginParam4.key = "zone_w";
        pluginParam4.value = to_string( zoneRect[ 2 ] );

        SPluginParameter pluginParam5;
        pluginParam5.key = "zone_h";
        pluginParam5.value = to_string( zoneRect[ 3 ] );

        SPluginMetadata processingPlugin;
        processingPlugin.pluginStaticName = "dpf_motion_detect";
        processingPlugin.GStreamerElementName = "dpf_motion_detection";
        processingPlugin.params.push_back( pluginParam2 );
        processingPlugin.params.push_back( pluginParam3 );
        processingPlugin.params.push_back( pluginParam4 );
        processingPlugin.params.push_back( pluginParam5 );

        AnalyzerProxy::SInitSettings settings;
        settings.sensorId = m_sensorId;
        settings.profileId = m_profileId;
        settings.processingId = common_utils::generateUniqueId();
        settings.processingName = m_processingName;
        settings.plugins.push_back( processingPlugin );

        if( ! ((SIncomingCommandServices *)m_services)->analyticManager->startAnalyze(settings) ){
            Json::Value jsonResponse;
            jsonResponse["cmd_type"] = "analyze";
            jsonResponse["cmd_name"] = "start";
            jsonResponse["sensor_id"] = (unsigned long long)m_sensorId;
            jsonResponse["analyze_status"] = common_utils::convertAnalyzeStateToStr( EAnalyzeState::CRUSHED );
            jsonResponse["processing_id"] = "0";
            jsonResponse["resume_current_session"] = resume;
            jsonResponse["error_msg"] = ((SIncomingCommandServices *)m_services)->analyticManager->getLastError();

            // TODO: remove after protocol refactor (response/body)
            Json::FastWriter writer;
            Json::Value root;
            root["response"] = "fail";
            root["body"] = jsonResponse;
            //

            sendResponse( writer.write(root) );

            return false;
        }

        //
        SWALClientOperation operation;
        operation.commandFullText = m_requestFullText;
        operation.uniqueKey = settings.processingId;
        operation.begin = true;        
        ((SIncomingCommandServices *)m_services)->systemEnvironment->serviceForWriteAheadLogging()->openOperation( operation );

        // TODO: не успевает инициализироваться
    //    const Analyzer::SAnalyzeStatus analyzerStatus = m_services->analyticManager->getAnalyzerStatus( m_sensorId );

        Json::Value response;
        response["cmd_type"] = "analyze";
        response["cmd_name"] = "start";
        response["sensor_id"] = (unsigned long long)m_sensorId;
        response["analyze_status"] = common_utils::convertAnalyzeStateToStr( EAnalyzeState::PREPARING );
        response["processing_id"] = settings.processingId;
        response["resume_current_session"] = resume;

        // TODO: remove after protocol refactor (response/body)
        Json::FastWriter writer;
        Json::Value root;
        root["response"] = "success";
        root["body"] = response;
        //

        sendResponse( writer.write(root) );

        return true;
    }
}





