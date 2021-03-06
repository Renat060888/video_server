
#include "common/common_utils.h"
#include "analyze/analytic_manager_facade.h"
#include "cmd_analyze_status.h"

using namespace std;
using namespace common_types;

const std::string CommandAnalyzeStatus::ALL_PROCESSING = "all_processings";

CommandAnalyzeStatus::CommandAnalyzeStatus( SIncomingCommandServices * _services )
    : ICommandExternal(_services)
{

}

#include <jsoncpp/json/writer.h>

bool CommandAnalyzeStatus::exec(){

    Json::Value response;
    Json::Value statusesRecords;

    // one analyzer
    if( m_processingId != ALL_PROCESSING ){

        AnalyzerProxy::SAnalyzeStatus status = ((SIncomingCommandServices *)m_services)->analyticManager->getAnalyzerStatus( m_processingId );

        Json::Value statusRecord;
        statusRecord["sensor_id"] = (unsigned long long)status.sensorId;
        statusRecord["state"] = common_utils::convertAnalyzeStateToStr( status.analyzeState );
        statusRecord["processing_id"] = status.processingId;
        statusRecord["processing_name"] = status.processingName;
        statusRecord["profile_id"] = (unsigned long long)status.profileId;

        statusesRecords.append( statusRecord );
    }
    // all analyzers
    else{
        for( const AnalyzerProxy::SAnalyzeStatus & status : ((SIncomingCommandServices *)m_services)->analyticManager->getAnalyzersStatuses() ){

            Json::Value statusRecord;
            statusRecord["sensor_id"] = (unsigned long long)status.sensorId;
            statusRecord["state"] = common_utils::convertAnalyzeStateToStr( status.analyzeState );
            statusRecord["processing_id"] = status.processingId;
            statusRecord["processing_name"] = status.processingName;
            statusRecord["profile_id"] = (unsigned long long)status.profileId;

            statusesRecords.append( statusRecord );
        }
    }

    response["analyzers_status"] = statusesRecords;

    // TODO: remove after protocol refactor (response/body)
    Json::FastWriter writer;
    Json::Value root;
    root["response"] = "success";
    root["body"] = response;
    //

    sendResponse( writer.write(root) );

    return true;
}
