
#include <microservice_common/system/logger.h>
#include <microservice_common/system/system_monitor.h>

#include "system/objrepr_bus_vs.h"
#include "analyze/analytic_manager_facade.h"
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

bool CommandPing::exec(){

    // status
    Json::Value status;
    status["analyze_events"] = getAnalyzeEvents();
    status["load_status"] = getLoadStatus();
    status["player_state"] = getPlayerStatus();
    status["current_ctx_id"] = OBJREPR_BUS.getCurrentContextId();

    // main message
    Json::Value response;
    response["message"] = "pong";
    response["status"] = status;

    // send
    sendResponse( response, true );
    return true;
}

Json::Value CommandPing::getAnalyzeEvents(){

    // collect events
    Json::Value analyzeEvents;

//    for( auto & valuePair : m_services->analyticManager->getLastAnalyzersEvent() ){
//        const std::string & pluginName = valuePair.first;
//        const SAnalyticEvent & lastEvent = valuePair.second;

//        analyzeEvents[ pluginName.c_str() ] = lastEvent.eventMessage;
//    }

    return analyzeEvents;
}

Json::Value CommandPing::getLoadStatus(){

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

Json::Value CommandPing::getPlayerStatus(){

    // TODO: binary code of player in "PlayerAgent" bin

    Json::Value playerStatus;

    IObjectsPlayer * player = m_services->analyticManager->getPlayingService();
    playerStatus["player_status"] = "INITED";

#if 0
    // TODO: crutch 'char * -> Player::SState *'
    Player::SState * playerState = ( Player::SState * )player->getState();
    const DatasourceMixer::SState & mixerState = playerState->mixer->getState();

    Json::Value playerStatus;
    playerStatus["current_step_ms"] = (long long)mixerState.globalDataRangeMillisec.first + (playerState->playIterator->getCurrentStep() * mixerState.globalStepUpdateMillisec);
    playerStatus["total_range_left_ms"] = (long long)mixerState.globalDataRangeMillisec.first;
    playerStatus["total_range_right_ms"] = (long long)mixerState.globalDataRangeMillisec.second;
    playerStatus["player_status"] = common_utils::printPlayingStatus( playerState->playingStatus );

    // datasources ranges
    // NOTE: temp ( because number of empty areas is tremendous )
    Json::Value array;
    for( PlayingDatasource * datasrc : mixerState.settings->datasources ){
        Json::Value datasrcRanges;

        datasrcRanges["sensor_id"] = (long long)datasrc->getState().settings->sensorId;

        PlayingDatasource::SBeacon::SDataBlock * block1 = datasrc->getState().payloadDataRangesInfo.front();
        PlayingDatasource::SBeacon::SDataBlock * block2 = datasrc->getState().payloadDataRangesInfo.back();
        datasrcRanges["ranges"].append( (long long)block1->timestampRangeMillisec.first );
        datasrcRanges["ranges"].append( (long long)block2->timestampRangeMillisec.second );

        array.append( datasrcRanges );
    }
    playerStatus["datasrc_set"] = array;

#if 0
    Json::Value array;
    for( PlayingDatasource * datasrc : mixerState.settings->datasources ){
        Json::Value datasrcRanges;

        datasrcRanges["sensor_id"] = (long long)datasrc->getState().settings->sensorId;

        for( PlayingDatasource::SBeacon::SDataBlock * block : datasrc->getState().payloadDataRangesInfo ){
            datasrcRanges["ranges"].append( (long long)block->timestampRangeMillisec.first );
            datasrcRanges["ranges"].append( (long long)block->timestampRangeMillisec.second );
        }

        array.append( datasrcRanges );
    }
    playerStatus["datasrc_set"] = array;
#endif
#endif

    return playerStatus;
}













