
#include <objrepr/reprServer.h>
#include <microservice_common/system/system_monitor.h>
#include <microservice_common/system/logger.h>

#include "common/common_utils.h"
#include "system/config_reader.h"
#include "system/objrepr_bus_vs.h"
#include "system/path_locator.h"
#include "analyzer.h"
#include "analyzer_remote.h"
#include "analytic_manager_facade.h"
#include "event_retranslator.h"

using namespace std;
using namespace common_types;

//#define REMOTE_ANALYZER
static constexpr const char * PRINT_HEADER = "AnalyticMgr:";
static constexpr int DEFAULT_FPS = 24;

AnalyticManagerFacade::AnalyticManagerFacade()
    : m_shutdownCalled(false)
    , m_threadEventsRetranslate(nullptr)
    , m_player(nullptr)
{

}

AnalyticManagerFacade::~AnalyticManagerFacade()
{
    if( ! m_shutdownCalled ){
        shutdown();
    }
}

bool AnalyticManagerFacade::init( SInitSettings _settings ){

    m_settings = _settings;        

    //
    VideocardBalancer::SInitSettings settings;
    settings.analyzers = & m_analyzers;
    settings.mutexAnalyzersLock = & m_muAnalyzers;
    if( ! m_videocardBalancer.init(settings) ){
        m_lastError = m_videocardBalancer.getLastError();
        return false;
    }

    //
    m_database = DatabaseManager::getInstance();

    DatabaseManager::SInitSettings settings3;
    settings3.host = CONFIG_PARAMS.MONGO_DB_ADDRESS;
    settings3.databaseName = CONFIG_PARAMS.MONGO_DB_NAME;

    if( ! m_database->init(settings3) ){
        return false;
    }

    // NOTE: only for live mode switching ( start/stop ) & update state commands
    IObjectsPlayer * newPlayer = m_playerDispatcher.getInstance( OBJREPR_BUS.getCurrentContextId() );
    if( ! newPlayer ){
        VS_LOG_ERROR << PRINT_HEADER << " TODO: print" << m_playerDispatcher.getLastError() << endl;
        // TODO: player only created by user ( without him it's pointless )
//        return false;
    }
    m_player = newPlayer;

    // TODO: may be not so good solution
    m_settings.serviceLocator.eventRetranslator = new EventRetranslator();

    //
    m_threadEventsRetranslate = new std::thread( & AnalyticManagerFacade::threadAnalyzersMaintenance, this );

    VS_LOG_INFO << PRINT_HEADER << " init success" << endl;
    return true;
}

void AnalyticManagerFacade::shutdown(){

    VS_LOG_INFO << PRINT_HEADER << " begin shutdown" << endl;

    delete m_settings.serviceLocator.eventRetranslator;
    m_settings.serviceLocator.eventRetranslator = nullptr;

    m_playerDispatcher.releaseInstance( m_player );

    m_shutdownCalled = true;
    common_utils::threadShutdown( m_threadEventsRetranslate );

    VS_LOG_INFO << PRINT_HEADER << " shutdown success" << endl;
}

void AnalyticManagerFacade::callbackVideoSourceConnection( bool _estab, const std::string & _sourceUrl ){

    // find all analyzers with same SourceURL & notify about CRASH
    if( ! _estab ){

        m_muAnalyzers.lock();
        for( auto iter = m_analyzers.begin(); iter != m_analyzers.end(); ){
            PAnalyzerProxy analyzer = ( * iter );

            if( analyzer->getSettings().sourceUrl == _sourceUrl ){

                const AnalyzerProxy::SAnalyzeStatus analyzerStatus = getAnalyzerStatus( analyzer->getSettings().processingId );

                // send State event
                SAnalyzeStateEvent event;
                event.analyzeState = EAnalyzeState::CRUSHED;
                event.sensorId = analyzerStatus.sensorId;
                event.message = "connection with camera related to this processing is losted";
                event.processingId = analyzerStatus.processingId;
                m_settings.serviceLocator.eventNotifier->sendEvent( event );

                iter = m_analyzers.erase( iter );
            }
            else{
                ++iter;
            }
        }
        m_muAnalyzers.unlock();
    }
}

PAnalyzerProxy AnalyticManagerFacade::createAnalyzer( const AnalyzerProxy::SInitSettings & _settings ){

    // TODO: who will be destroy a real analyzer ? ( now this doing by AnalyzerProxy )

    PAnalyzerProxy out;
#ifdef REMOTE_ANALYZER
    AnalyzerRemote * remote = new AnalyzerRemote( m_settings.serviceLocator.internalCommunication, _settings.sourceUrl );
    out = std::make_shared<AnalyzerProxy>( remote );
#else
    AnalyzerLocal * local = new AnalyzerLocal();
    out = std::make_shared<AnalyzerProxy>( local );
#endif
    return out;
}

bool AnalyticManagerFacade::startAnalyze( AnalyzerProxy::SInitSettings _settings ){

    if( ! isAnalyzerSettingsUnique(_settings) ){
        return false;
    }

    if( isAnotherAnalyzerStarting() ){
        return false;
    }

    // session number update
    _settings.sourceUrl = OBJREPR_BUS.getSensorUrl( _settings.sensorId );
    const std::unordered_map<TSensorId, SProcessedSensor> sensors = m_database->getProcessedSensors( OBJREPR_BUS.getCurrentContextId() );
    auto iter = sensors.find( _settings.sensorId );
    if( iter != sensors.end() ){
        SProcessedSensor sensorCopy = iter->second;
        sensorCopy.lastSession++;
        const bool rt = m_database->writeProcessedSensor( OBJREPR_BUS.getCurrentContextId(), sensorCopy );
        _settings.currentSession = sensorCopy.lastSession;
    }
    else{
        SProcessedSensor sensor;
        sensor.id = _settings.sensorId;
        sensor.lastSession = 0;
        sensor.updateStepMillisec = 1000 / DEFAULT_FPS;
        const bool rt = m_database->writeProcessedSensor( OBJREPR_BUS.getCurrentContextId(), sensor );
        _settings.currentSession = sensor.lastSession;
    }

    // async starting for immediate response
    std::future<TFutureStartResult> analyzeStartInFuture = std::async( std::launch::async, & AnalyticManagerFacade::asyncAnalyzeStart, this, _settings );
    m_muStartFutures.lock();
    m_analyzeStartFutures.push_back( std::move(analyzeStartInFuture) );

    //
    m_analyzersFailedInFuture.insert( {_settings.processingId, _settings} );
    m_muStartFutures.unlock();

    return true;
}

AnalyticManagerFacade::TFutureStartResult AnalyticManagerFacade::asyncAnalyzeStart( AnalyzerProxy::SInitSettings _settings ){

    // init source
    SVideoSource source;
    source.sourceUrl = OBJREPR_BUS.getSensorUrl( _settings.sensorId );

    _settings.VideoReceiverRtpPayloadSocket = PATH_LOCATOR.getVideoReceiverRtpPayloadSocket( source.sourceUrl );

    if( ! m_settings.serviceLocator.videoSources->connectSource(source) ){
        m_lastError = m_settings.serviceLocator.videoSources->getLastError();
        return TFutureStartResult(_settings.processingId, false);
    }
    const ISourcesProvider::SSourceParameters sourceParameters = m_settings.serviceLocator.videoSources->getShmRtpStreamParameters( source.sourceUrl );

    // videocard
    void * dpfProcessorPtr = nullptr;
    unsigned int videocardIdx = 0;
    m_videocardBalancer.getVideocard( _settings.processingId,
                                      SVideoResolution(sourceParameters.frameWidth, sourceParameters.frameHeight),
                                      _settings.profileId,
                                      videocardIdx,
                                      dpfProcessorPtr );
    if( videocardIdx == SYSTEM_MONITOR.NO_AVAILABLE_CARDS ){
        m_lastError = m_videocardBalancer.getLastError();

        // deinit source
        m_settings.serviceLocator.videoSources->disconnectSource( source.sourceUrl );

        return TFutureStartResult(_settings.processingId, false);
    }

    // create analyzer
    SPluginParameter pluginParam;
    pluginParam.key = "profile_path";
    pluginParam.value = getSensorProfiles( _settings.sensorId )[ _settings.profileId ].profilePath;

    // TODO: duplicates in Settings & Plugin struct

    // TODO: hardcode
    _settings.plugins[ 0 ].params.push_back( pluginParam );
    _settings.sourceUrl = source.sourceUrl;
    _settings.intervideosinkChannelName = source.sourceUrl;
    _settings.shmRtpStreamCaps = sourceParameters.rtpStreamCaps;
    _settings.shmRtpStreamEncoding = sourceParameters.rtpStreamEnconding;
    _settings.usedVideocardIdx = videocardIdx;
    _settings.processingFlags = setProcessingFlags();
    _settings.dpfProcessorPtr = dpfProcessorPtr;
    _settings.allowedDpfLabelObjreprClassinfo = getSensorProfiles( _settings.sensorId )[ _settings.profileId ].processingNameToObjreprClassinfo;
    _settings.maxObjectsForBestImageTracking = CONFIG_PARAMS.PROCESSING_MAX_OBJECTS_FOR_BEST_IMAGE_TRACKING;
//    _settings.allowedDpfLabelObjreprClassinfo = getDpfObjreprMapping( _settings.profileId );

    PAnalyzerProxy analyzer = createAnalyzer( _settings );
    if( ! analyzer ){
        VS_LOG_ERROR << PRINT_HEADER << " failed to create analyzer with sensor id [" << _settings.sensorId << "]" << endl;
        return TFutureStartResult(_settings.processingId, false);
    }

    if( ! analyzer->start(_settings) ){
        m_lastError = analyzer->getLastError();
        return TFutureStartResult(_settings.processingId, false);
    }

    m_muAnalyzers.lock();
    m_analyzers.push_back( analyzer );
    m_analyzersByProcessingId.insert( {_settings.processingId, analyzer} );
    m_muAnalyzers.unlock();

    // watch for the source status
    m_settings.serviceLocator.videoSources->addSourceObserver( source.sourceUrl, this );

    return TFutureStartResult(_settings.processingId, true);
}

bool AnalyticManagerFacade::resumeAnalyze( TProcessingId _procId ){

    string errMsg;
    PAnalyzerProxy analyzer = getAnalyzer( _procId, errMsg );
    if( analyzer ){

        // resume
        analyzer->resume();

        SProcessedSensor sensor;
        sensor.id = analyzer->getSettings().sensorId;
        sensor.lastSession = analyzer->getSettings().currentSession;
        const bool rt = m_database->writeProcessedSensor( OBJREPR_BUS.getCurrentContextId(), sensor );

        // async notify about resume ( after 1 second )
        // TODO: very doubtful place
        std::future<TFutureStartResult> analyzeStartInFuture = std::async( std::launch::async, [analyzer]() -> TFutureStartResult {
            this_thread::sleep_for( chrono::seconds(1) );
            return TFutureStartResult(analyzer->getSettings().processingId, true);
        } );

        m_muStartFutures.lock();
        m_analyzeStartFutures.push_back( std::move(analyzeStartInFuture) );
        m_muStartFutures.unlock();
    }
    else{
        m_lastError = errMsg;
        VS_LOG_ERROR << PRINT_HEADER << errMsg << endl;
        return false;
    }

    return true;
}

bool AnalyticManagerFacade::pauseAnalyze( TProcessingId _procId ){

    string errMsg;
    PAnalyzerProxy analyzer = getAnalyzer( _procId, errMsg );
    if( analyzer ){
        analyzer->pause();
    }
    else{
        m_lastError = errMsg;
        VS_LOG_ERROR << PRINT_HEADER << errMsg << endl;
        return false;
    }

    return true;
}

#include "video_server.h"

bool AnalyticManagerFacade::stopAnalyze( TProcessingId _procId ){

    string errMsg;
    PAnalyzerProxy analyzer = getAnalyzer( _procId, errMsg );
    // stop analyzer that runned success
    if( analyzer ){
        analyzer->stop();
        m_videocardBalancer.returnVideocard( analyzer->getSettings().processingId );
        removeAnalyzer( analyzer->getSettings().processingId );

        // deinit source
        SVideoSource source;
        source.sourceUrl = OBJREPR_BUS.getSensorUrl( analyzer->getSettings().sensorId );
        if( ! m_settings.serviceLocator.videoSources->disconnectSource(source.sourceUrl) ){
            return false;
        }

        // TODO: temporary ?
//        m_player->updatePlayingData();

        // TODO: походу receiver перехватывает подписку на сигнал от ядра
        VS_LOG_DBG << "============================ test signal connecting check >>" << endl;
        common_utils::connectInterruptSignalHandler( ( common_utils::TSignalHandlerFunction ) & VideoServer::callbackUnixInterruptSignal );
        VS_LOG_DBG << "============================ test signal connecting check <<" << endl;
    }
    // stop analyzer that failed while starting
    else{
        // check in start-futures
        m_muStartFutures.lock();
        auto iter2 = m_analyzersFailedInFuture.find( _procId );
        if( iter2 != m_analyzersFailedInFuture.end() ){
            const AnalyzerProxy::SInitSettings settingsForStart = iter2->second;
            m_analyzersFailedInFuture.erase( iter2 );
            m_muStartFutures.unlock();
            VS_LOG_WARN << PRINT_HEADER << " processing id [" << _procId << "] found in start-futures. Disconnect from appropriate source" << endl;
            m_settings.serviceLocator.videoSources->disconnectSource( settingsForStart.sourceUrl );
        }
        m_muStartFutures.unlock();

        m_lastError = errMsg;
        VS_LOG_ERROR << PRINT_HEADER << errMsg << endl;
        return false;
    }

    return true;
}

void AnalyticManagerFacade::setLivePlaying( bool _live ){

    if( _live ){
        // switch on analyzers playing
        m_muAnalyzers.lock();
        for( PAnalyzerProxy & analyzer : m_analyzers ){
            analyzer->setLivePlaying( _live );
        }
        m_muAnalyzers.unlock();

        // switch off ( pause ) player
        m_muPlayer.lock();
//        m_player->pause();
        m_muPlayer.unlock();
    }
    else{
        // switch off analyzers playing
        m_muAnalyzers.lock();
        for( PAnalyzerProxy & analyzer : m_analyzers ){
            analyzer->setLivePlaying( _live );
        }
        m_muAnalyzers.unlock();

        // switch on ( start ) player
        m_muPlayer.lock();
//        m_player->start();
        m_muPlayer.unlock();
    }
}

AnalyzerProxy::SAnalyzeStatus AnalyticManagerFacade::getAnalyzerStatus( TProcessingId _procId ){

    AnalyzerProxy::SAnalyzeStatus out;

    string errMsg;
    PAnalyzerProxy analyzer = getAnalyzer( _procId, errMsg );
    if( analyzer ){
        out = analyzer->getStatus();
    }
    else{
        m_lastError = errMsg;
        VS_LOG_ERROR << PRINT_HEADER << errMsg << endl;
    }

    return out;
}

std::vector<AnalyzerProxy::SAnalyzeStatus> AnalyticManagerFacade::getAnalyzersStatuses(){

    std::vector<AnalyzerProxy::SAnalyzeStatus> out;
    out.reserve( m_analyzers.size() );

    // active analyzers
    m_muAnalyzers.lock();
    for( PAnalyzerProxy analyzer : m_analyzers ){
        out.push_back( analyzer->getStatus() );
    }
    m_muAnalyzers.unlock();

    // in starting progress
    m_muStartFutures.lock();
    for( auto & valuePair : m_analyzersFailedInFuture ){
        const AnalyzerProxy::SInitSettings & analyzerSettings = valuePair.second;

        AnalyzerProxy::SAnalyzeStatus toOut;
        toOut.analyzeState = EAnalyzeState::PREPARING;
        toOut.sensorId = analyzerSettings.sensorId;
        toOut.processingId = analyzerSettings.processingId;
        toOut.processingName = analyzerSettings.processingName;
        toOut.profileId = analyzerSettings.profileId;

        out.push_back( toOut );
    }
    m_muStartFutures.unlock();

    return out;
}

PAnalyzerProxy AnalyticManagerFacade::getAnalyzer( const TProcessingId _processingId, std::string & _msg ){

    if( _processingId.empty() ){
        stringstream ss;
        ss << PRINT_HEADER << " empty processing id";
        _msg = ss.str();
        return nullptr;
    }

    m_muAnalyzers.lock();
    auto iter = m_analyzersByProcessingId.find( _processingId );
    if( iter != m_analyzersByProcessingId.end() ){
        m_muAnalyzers.unlock();
        return iter->second;
    }
    else{
        stringstream ss;

        ss << " analyzer with such processing id [" << _processingId << "] not found. Available: [";
        for( auto iter = m_analyzersByProcessingId.begin(); iter != m_analyzersByProcessingId.end(); ++iter ){
            PAnalyzerProxy analyzer = iter->second;
            ss << analyzer->getSettings().processingId << " ";
        }
        ss << "]";

        m_muAnalyzers.unlock();
        _msg = ss.str();
        return nullptr;
    }
}

void AnalyticManagerFacade::removeAnalyzer( const TProcessingId _processingId ){

    m_muAnalyzers.lock();

    auto iter = m_analyzersByProcessingId.find( _processingId );
    if( iter != m_analyzersByProcessingId.end() ){
        m_analyzersByProcessingId.erase( iter );
    }

    for( auto iter = m_analyzers.begin(); iter != m_analyzers.end();  ){
        PAnalyzerProxy analyzer = ( * iter );

        if( analyzer->getSettings().processingId == _processingId ){
            m_analyzers.erase( iter );
            break;
        }
        else{
            ++iter;
        }
    }

    m_muAnalyzers.unlock();
}

bool AnalyticManagerFacade::isAnalyzerSettingsUnique( const AnalyzerProxy::SInitSettings & _settings ){

    m_muAnalyzers.lock();
    for( auto iter = m_analyzers.begin(); iter != m_analyzers.end(); ++iter ){
        PAnalyzerProxy analyzer = ( * iter );

        if( analyzer->getSettings() == _settings ){
            m_muAnalyzers.unlock();

            stringstream ss;
            ss << PRINT_HEADER << " analyzer with same settings already exist, sensor id [" << _settings.sensorId << "]";
            VS_LOG_WARN << ss.str() << endl;
            m_lastError = ss.str();

            return false;
        }
    }
    m_muAnalyzers.unlock();

    return true;
}

bool AnalyticManagerFacade::isAnotherAnalyzerStarting(){

    std::lock_guard<std::mutex> lock( m_muStartFutures );
    if( ! m_analyzeStartFutures.empty() ){

        stringstream ss;
        ss << PRINT_HEADER << " another analyzer in starting process. Please wait for it's finish";
        VS_LOG_WARN << ss.str() << endl;
        m_lastError = ss.str();

        return true;
    }

    return false;
}

void AnalyticManagerFacade::callbackFromAnalyzer( const SAnalyticEvent & _event ){

    // TODO: ?
    m_muAnalyzers.lock();

    m_settings.serviceLocator.eventStore->writeEvent( _event );
    m_settings.serviceLocator.eventNotifier->sendEvent( _event );
    m_statisticsClient.statEvent( _event );
    m_lastAnalyzerEvent[ _event.pluginName ] = _event;

    m_muAnalyzers.unlock();
}

void AnalyticManagerFacade::threadAnalyzersMaintenance(){

    VS_LOG_INFO << PRINT_HEADER << " start a maintenance THREAD" << endl;

    constexpr int listenIntervalMillisec = 10;

    while( ! m_shutdownCalled ){

        processEvents();
        checkStartResults();
        updatePlayer();

        this_thread::sleep_for( chrono::milliseconds(listenIntervalMillisec) );
    }

    VS_LOG_INFO << PRINT_HEADER << " maintenance THREAD is stopped" << endl;
}

void AnalyticManagerFacade::processEvents(){

    m_muAnalyzers.lock();
    for( const PAnalyzerProxy & analyzer : m_analyzers ){

        for( const SAnalyticEvent & event : analyzer->getAccumulatedEvents() ){

            if( CONFIG_PARAMS.PROCESSING_STORE_EVENTS ){
                m_settings.serviceLocator.eventStore->writeEvent( event );
            }

            m_settings.serviceLocator.eventNotifier->sendEvent( event );

            // NOTE: at this moment only for video-attr
            if( CONFIG_PARAMS.PROCESSING_ENABLE_VIDEO_ATTRS ){
                m_settings.serviceLocator.eventProcessor->processEvent( pullProcessingPart(analyzer->getSettings().processingId, event) );
            }

            m_statisticsClient.statEvent( event );
            m_lastAnalyzerEvent[ event.pluginName ] = event;
        }

        for( const SAnalyticEvent & event : analyzer->getAccumulatedInstantEvents() ){

            if( CONFIG_PARAMS.PROCESSING_RETRANSLATE_EVENTS ){
                m_settings.serviceLocator.eventRetranslator->retranslateEvent( event );
            }
        }
    }
    m_muAnalyzers.unlock();
}

void AnalyticManagerFacade::checkStartResults(){

    vector<TProcessingId> crashedAnalyzers;
    m_muStartFutures.lock();
    for( auto iter = m_analyzeStartFutures.begin(); iter != m_analyzeStartFutures.end(); ){
        std::future<TFutureStartResult> & analyzeFuture = ( * iter );

        const std::future_status futureStatus = analyzeFuture.wait_for( std::chrono::milliseconds(10) );
        if( std::future_status::ready == futureStatus ){

            // check Start state and get message if it failed
            const TFutureStartResult startFromFuture = analyzeFuture.get();

            string errMsg;
            PAnalyzerProxy analyzer = getAnalyzer( startFromFuture.first, errMsg );

            // start failed
            if( ! startFromFuture.second ){
                if( analyzer ){
                    // error inside analyzer
                    crashedAnalyzers.push_back( analyzer->getSettings().processingId );

                    const AnalyzerProxy::SAnalyzeStatus analyzerStatus = analyzer->getStatus();

                    SAnalyzeStateEvent event;
                    event.analyzeState = analyzerStatus.analyzeState;
                    event.sensorId = analyzerStatus.sensorId;
                    event.message = analyzer->getLastError();
                    event.processingId = analyzerStatus.processingId;
                    m_settings.serviceLocator.eventNotifier->sendEvent( event );
                }
                else{
                    // error outside analyzer
                    VS_LOG_WARN << PRINT_HEADER << " analyzer's return from std::future was failed: [" << errMsg << "]" << endl;

                    const AnalyzerProxy::SInitSettings & startSettings = m_analyzersFailedInFuture.find( startFromFuture.first )->second;

                    SAnalyzeStateEvent event;
                    event.analyzeState = EAnalyzeState::CRUSHED;
                    event.sensorId = startSettings.sensorId;
                    event.message = getLastError();
                    event.processingId = startFromFuture.first;
                    m_settings.serviceLocator.eventNotifier->sendEvent( event );
                }

                WriteAheadLogger walForEndDump;
                walForEndDump.closeClientOperation( startFromFuture.first );
            }
            // start success
            else{
                const AnalyzerProxy::SAnalyzeStatus analyzerStatus = analyzer->getStatus();

                SAnalyzeStateEvent event;
                event.analyzeState = analyzerStatus.analyzeState;
                event.sensorId = analyzerStatus.sensorId;
                event.message = analyzer->getLastError();
                event.processingId = analyzerStatus.processingId;
                m_settings.serviceLocator.eventNotifier->sendEvent( event );

                m_settings.serviceLocator.eventProcessor->addEventProcessor( analyzer->getSettings().sourceUrl, analyzer->getSettings().processingId );

                m_analyzersFailedInFuture.erase( startFromFuture.first );
            }

            // clear future
            iter = m_analyzeStartFutures.erase( iter );
        }
        else{
            ++iter;
        }
    }
    m_muStartFutures.unlock();

    // clean crashed analyzers
    // TODO: what to do with mirror on a client side ? ( stop not sended )
//        for( const TProcessingId & procId : crashedAnalyzers ){
//            removeAnalyzer( procId );
//        }
}

void AnalyticManagerFacade::updatePlayer(){

    if( ! CONFIG_PARAMS.PROCESSING_STORE_EVENTS ){
        return;
    }

    static constexpr int64_t playerUpdateIntervalSec = 10;
    static int64_t lastPlayerUpdateSec = 0;
    if( (common_utils::getCurrentTimeSec() - lastPlayerUpdateSec) <= playerUpdateIntervalSec ){
        return;
    }
    lastPlayerUpdateSec = common_utils::getCurrentTimeSec();

    //
    m_muAnalyzers.lock();
    if( m_analyzers.empty() ){
        m_muAnalyzers.unlock();
        return;
    }

    //
    bool activeProcessingExist = false;
    for( const PAnalyzerProxy & analyzer : m_analyzers ){
        if( EAnalyzeState::ACTIVE == analyzer->getStatus().analyzeState ){
            activeProcessingExist = true;
            break;
        }
    }
    m_muAnalyzers.unlock();

    //
    if( activeProcessingExist ){
        m_muPlayer.lock();
//        m_player->updatePlayingData();
        m_muPlayer.unlock();
    }
}

int32_t AnalyticManagerFacade::setProcessingFlags(){

    int32_t flags = 0;
    if( CONFIG_PARAMS.PROCESSING_ENABLE_IMAGE_ATTRS ){
        flags |= (int32_t)common_types_with_plugins::EAnalyzerFlags::ENABLE_IMAGE_ATTR;
    }
    if( CONFIG_PARAMS.PROCESSING_ENABLE_VIDEO_ATTRS ){
        flags |= (int32_t)common_types_with_plugins::EAnalyzerFlags::ENABLE_VIDEO_ATTR;
    }
    if( CONFIG_PARAMS.PROCESSING_ENABLE_RECTS_ON_TEST_SCREEN ){
        flags |= (int32_t)common_types_with_plugins::EAnalyzerFlags::ENABLE_RECTS_ON_TEST_SCREEN;
    }
    if( CONFIG_PARAMS.PROCESSING_ENABLE_OPTIC_STREAM ){
        flags |= (int32_t)common_types_with_plugins::EAnalyzerFlags::ENABLE_OPTIC_STREAM;
    }
    if( CONFIG_PARAMS.PROCESSING_ENABLE_ABSENT_STATE_AT_OBJECT_DISAPPEAR ){
        flags |= (int32_t)common_types_with_plugins::EAnalyzerFlags::ENABLE_ABSENT_STATE_AT_OBJECT_DISAPPEAR;
    }


    return flags;
}

SAnalyzedObjectEvent AnalyticManagerFacade::pullProcessingPart( const TProcessingId & _procId, const SAnalyticEvent & _event ){

    SAnalyzedObjectEvent out;

    //
    Json::Value parsedRecord;
    if( ! m_jsonReader.parse( _event.eventMessage.c_str(), parsedRecord, false ) ){
        VS_LOG_ERROR << PRINT_HEADER
                  << " parse failed of [1] Reason: [2] ["
                  << _event.eventMessage << "] [" << m_jsonReader.getFormattedErrorMessages() << "]"
                  << endl;
        return out;
    }

    const int totalCount = parsedRecord["total_count"].asInt();
    if( 0 == totalCount ){
        return out;
    }

    //
    if( ! parsedRecord["disappeared_objects"].isNull() ){
        const Json::Value & disappearedObjects = parsedRecord["disappeared_objects"];

        for( Json::ArrayIndex i = 0; i < disappearedObjects.size(); i++ ){
            out.objreprObjectId.push_back( disappearedObjects[ i ]["objrepr_id"].asInt64() );
        }
    }

    out.processingId = _procId;
    return out;
}


// TODO: temp
const std::map<std::string, SAnalyticEvent> & AnalyticManagerFacade::getLastAnalyzersEvent(){
    return m_lastAnalyzerEvent;
}


// TODO: temp
#ifdef OBJREPR_LIBRARY_EXIST
bool isThisGroupOrParentGroupHasThisNameRecursive(objrepr::ClassgroupPtr groupPtr, const string nameOfGroup )
{
    bool resVal = false;
    if( ! groupPtr || nameOfGroup.empty() )
        return resVal;

    string currentGroupName = groupPtr->name();
    if( currentGroupName == nameOfGroup )
        resVal = true;
    else
        resVal = isThisGroupOrParentGroupHasThisNameRecursive( groupPtr->parent(),nameOfGroup );

    return resVal;
}
#endif

std::map<uint64_t, SAnalyzeProfile> AnalyticManagerFacade::getSensorProfiles( TSensorId _sensorId ){
#ifdef OBJREPR_LIBRARY_EXIST
    objrepr::GlobalDataManager * globalManager = objrepr::RepresentationServer::instance()->globalManager();
    objrepr::SpatialObjectManager * objManager = objrepr::RepresentationServer::instance()->objectManager();

    vector<objrepr::GlobalDataManager::ClassRelation> childRels = globalManager->classinfoRelationByOrigin( _sensorId );
    if( childRels.empty() ){
        childRels = objManager->getObjectRelations( _sensorId );
    }

    // ищем профили у этого сенсора
    std::map<uint64_t, SAnalyzeProfile> profiles;
    for( objrepr::GlobalDataManager::ClassRelation clRel : childRels ){
        objrepr::ClassinfoPtr target = clRel.target;

        if( ! target ){
            continue;
        }

        if( ! target->group() ){
            continue;
        }

        if( target->group()->name() != string("Профили трекинга видео") ){
            continue;
        }

        // это профиль обработки видео
        objrepr::AttributeMapPtr attrs = target->attrMap();
        if( ! attrs ){
            continue;
        }

        // это путь к папке профиля, то что нужно Никите
        objrepr::AttributePtr profilePathAttr = attrs->getAttr("Папка профиля");

        SAnalyzeProfile profile;
        profile.name = target->name();
        profile.id = target->id();

        const std::string MEDIA_PATH = globalManager->mediaPath();
        if( profilePathAttr ){
            objrepr::StringAttributePtr profilePath = boost::dynamic_pointer_cast<objrepr::StringAttribute>(profilePathAttr);
            profile.profilePath = MEDIA_PATH + "/" + profilePath->valueAsString();
        }

        objrepr::ClassinfoPtr profileClassinfo = objManager->getClassinfo( profile.id );
        if( profileClassinfo ){
            vector<objrepr::RelationClassinfoPtr> relationsFromProfile = globalManager->relationListByOrigin( profileClassinfo->id() );
            // смотрим что распознает этот профиль
            for( objrepr::RelationClassinfoPtr relation : relationsFromProfile ){

                if( ! relation || ! relation->abstract() ){
                    continue;
                }

                // класс объекта в обжрепре
                objrepr::ClassinfoPtr relationTarget = objManager->getClassinfo( relation->targetClassinfo() );
                if( ! relationTarget ){
                    continue;
                }

                if( ! isThisGroupOrParentGroupHasThisNameRecursive(relation->group(), "Распознаваемые профилями объекты") ){
                    continue;
                }

                // имя объекта у Никиты
                objrepr::AttributePtr attrPtr = relation->attrMap()->getAttr("Идентификатор объекта");
                if( attrPtr ){
                    profile.processingNameToObjreprClassinfo.insert( {attrPtr->valueAsString(), relationTarget->id()} );

                    objrepr::ClassinfoPtr classinfo = globalManager->findClassinfo( relationTarget->id() );
                    int a = 0;
                }
            }
        }

        profiles.insert( { profile.id, profile} );
    }

    return profiles;
#endif
}

IObjectsPlayer * AnalyticManagerFacade::getPlayingService(){
    // TODO: mutex protect ?
    return m_player;
}

std::map<std::string, uint64_t> AnalyticManagerFacade::getDpfObjreprMapping( TProfileId _profileId ){



}

struct ObjreprClassinfFindFunctor {
    ObjreprClassinfFindFunctor( uint64_t _classinfoId )
        : classinfoId(_classinfoId)
    {}

    bool operator()( uint64_t _classinfoId ){
        return ( _classinfoId == classinfoId );
    }

    uint64_t classinfoId;
};

std::map<std::string, uint64_t> AnalyticManagerFacade::getDpfObjreprMapping( const std::vector<uint64_t> & _filterObjreprClassinfoId ){
#ifdef OBJREPR_LIBRARY_EXIST
    std::map<std::string, uint64_t> out;
    objrepr::GlobalDataManager * globalManager = objrepr::RepresentationServer::instance()->globalManager();

    for( auto & valuePair : CONFIG_PARAMS.DPF_LABEL_TO_OBJREPR_CLASSINFO ){
        const string & dpfLabel = valuePair.first;
        const string & objreprClassinfoName = valuePair.second;

        // TODO: find solution for multiple classinfos of one name
        objrepr::ClassinfoPtr classinfo;
//        if( "Человек" == objreprClassinfoName ){
            std::vector<objrepr::ClassinfoPtr> classinfos = globalManager->classinfoListByCat( "ground" );
            for( objrepr::ClassinfoPtr & classinfoFromContainer : classinfos ){

                if( objreprClassinfoName == classinfoFromContainer->name() ){
                    classinfo = classinfoFromContainer;
                }
            }
//        }
//        else{
//            classinfo = globalManager->findClassinfo( objreprClassinfoName.c_str() );
//        }

        if( classinfo ){

            // filter
            auto iterClassinfoFound = std::find_if( _filterObjreprClassinfoId.begin(), _filterObjreprClassinfoId.end(), ObjreprClassinfFindFunctor(classinfo->id()) );
            if( ! _filterObjreprClassinfoId.empty() && iterClassinfoFound == _filterObjreprClassinfoId.end() ){
                continue;
            }

            out.insert( {dpfLabel, classinfo->id()} );
        }
        else{
            VS_LOG_ERROR << "objrepr classinfo not found: " << objreprClassinfoName
                      << endl;
        }
    }

    return out;
#endif
}


