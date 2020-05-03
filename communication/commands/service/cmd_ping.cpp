
#include <microservice_common/system/logger.h>
#include <microservice_common/system/system_monitor.h>

#include "system/objrepr_bus_vs.h"
#include "analyze/analytic_manager_facade.h"
#include "storage/video_recorder.h"
#include "common/common_utils.h"
#include "cmd_ping.h"

// TODO: remove later
//#include "player_agent/analyze/player.h"

using namespace std;
using namespace common_types;

CommandPing::CommandPing( SIncomingCommandServices * _services ) :
    ICommandExternal(_services)
{

}

static Json::Value getEvents(){

    // collect events
    Json::Value analyzeEvents;

//    for( auto & valuePair : m_services->analyticManager->getLastAnalyzersEvent() ){
//        const std::string & pluginName = valuePair.first;
//        const SAnalyticEvent & lastEvent = valuePair.second;

//        analyzeEvents[ pluginName.c_str() ] = lastEvent.eventMessage;
//    }

    return analyzeEvents;
}

static Json::Value getSystemState(){

    const SystemMonitor::STotalInfo total = SYSTEM_MONITOR.getTotalSnapshot();
    assert( (total.memory.ramTotalMb - total.memory.ramFreeMb) < total.memory.ramTotalMb );

    Json::Value coreStatus;
    coreStatus["cpu_cores"] = (int)total.cpu.cores.size();
    coreStatus["cpu_load_percent"] = total.cpu.totalLoadByAvgFromCoresPercent;
    coreStatus["ram_total_mb"] = total.memory.ramTotalMb;
    coreStatus["ram_free_mb"] = total.memory.ramFreeMb;
    coreStatus["ram_available_mb"] = total.memory.ramAvailableMb;

    unsigned int avgLoadSum = 0;
    for( const SystemMonitor::SVideoStatus & video : total.videocards ){
        avgLoadSum += video.gpuLoadPercent;
    }
    const unsigned int avgGpuLoadPercent = avgLoadSum / total.videocards.size();

    unsigned int avgMemSum = 0;
    for( const SystemMonitor::SVideoStatus & video : total.videocards ){
        avgMemSum += video.memoryUsedPercent;
    }
    const unsigned int avgGpuMemoryUsePercent = avgMemSum / total.videocards.size();

    Json::Value videoStatus;
    videoStatus["gpu_load_percent"] = avgGpuLoadPercent;
    videoStatus["memory_used_percent"] = avgGpuMemoryUsePercent;

    Json::Value loadStatus;
    loadStatus["core"] = coreStatus;
    loadStatus["video"] = videoStatus;

    return loadStatus;
}

static Json::Value getServerState(){

    Json::Value serverState;
    serverState["ctx_id"] = OBJREPR_BUS.getCurrentContextId();
    serverState["server_objrepr_id"] = (long long)OBJREPR_BUS.getVideoServerObjreprMirrorId();

    return serverState;
}

static Json::Value getArchiversState( SIncomingCommandServices * _services ){

    Json::Value archiversStates;
    for( const ArchiveCreatorProxy::SCurrentState & state : _services->videoRecorder->getArchiverStates() ){
        Json::Value archiverState;
        archiverState["sensor_id"] = (long long)state.initSettings->sensorId;
        archiverState["archiving_id"] = state.initSettings->archivingId;
        archiverState["archiver_status"] = common_utils::convertArchivingStateToStr( state.state );

        archiversStates.append( archiverState );
    }

    return archiversStates;
}

static Json::Value getAnalyzersState( SIncomingCommandServices * _services ){

    Json::Value analyzersStates;
    for( const AnalyzerProxy::SAnalyzeStatus & state : _services->analyticManager->getAnalyzersStatuses() ){
        Json::Value analyzerState;
        analyzerState["sensor_id"] = (long long)state.sensorId;
        analyzerState["profile_id"] = (long long)state.profileId;
        analyzerState["processing_id"] = state.processingId;
        analyzerState["analyzer_status"] = common_utils::convertAnalyzeStateToStr( state.analyzeState );

        analyzersStates.append( analyzerState );
    }

    return analyzersStates;
}

#include <jsoncpp/json/writer.h>

bool CommandPing::exec(){

    // status
    Json::Value responseRecord;
    responseRecord["cmd_name"] = "pong";
    responseRecord["server_state"] = getServerState();
    responseRecord["system_state"] = getSystemState();
    responseRecord["archivers_state"] = getArchiversState( (SIncomingCommandServices *)m_services );
    responseRecord["analyzers_state"] = getAnalyzersState( (SIncomingCommandServices *)m_services );
    responseRecord["events"] = getEvents();

    // send
    Json::FastWriter writer;
    sendResponse( writer.write(responseRecord) );
    return true;
}
















