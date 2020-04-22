
#include <boost/filesystem.hpp>
#include <microservice_common/system/logger.h>

#include "common/common_utils.h"
#include "system/config_reader.h"
#include "system/objrepr_bus_vs.h"
#include "system/system_environment_facade_vs.h"
#include "archive_creator_remote.h"
#include "archive_creator.h"
#include "video_recorder.h"

using namespace std;
using namespace common_types;

static constexpr const char * PRINT_HEADER = "VideoRec:";

VideoRecorder::VideoRecorder()
    : m_shutdownCalled(false)
    , m_threadHlsMaintenance(nullptr)
    , m_newDataBlockCreation(false)
{

}

VideoRecorder::~VideoRecorder(){

    shutdown();
}

bool VideoRecorder::init( SInitSettings _settings ){

    m_settings = _settings;

    m_threadHlsMaintenance = new std::thread( & VideoRecorder::threadArchivesMaintenance, this );


    VS_LOG_INFO << PRINT_HEADER << " init success" << endl;
    return true;
}

void VideoRecorder::shutdown(){

    if( ! m_shutdownCalled ){
        VS_LOG_INFO << PRINT_HEADER << " begin shutdown" << endl;

        m_shutdownCalled = true;
        common_utils::threadShutdown( m_threadHlsMaintenance );

        m_muArchiverLock.lock();
        for( PArchiveCreatorProxy & creator : m_archiveCreators ){

            creator->stop();
            SVideoSource source;
            source.sourceUrl = creator->getSettings().sourceUrl;

            // 'dereference' source consumers
            // TODO: may be replace into archiver ?
            m_settings.videoSourcesProvider->disconnectSource(source.sourceUrl);
        }
        m_muArchiverLock.unlock();

        VS_LOG_INFO << PRINT_HEADER << " shutdown success" << endl;
    }
}

std::string VideoRecorder::getLastError(){
    return string(PRINT_HEADER) + " ERROR - [" + m_lastError + "]";
}

void VideoRecorder::threadArchivesMaintenance(){

    VS_LOG_INFO << PRINT_HEADER << " start archives maintenance THREAD" << endl;

    constexpr int listenIntervalMillisec = 10;

    while( ! m_shutdownCalled ){

        // influence on actual data
        if( CONFIG_PARAMS.RECORD_ENABLE_STORE_POLICY ){
            checkForArchiveNewBlock();
            checkForOutdatedArchives();
        }

        // supply infrastructure
        updateArchivesMetadata();
        checkForDiedArchiveCreators();

        // async recording start
        recordingStartFuturesMaintenance();

        // tick
        archiversTick();

        this_thread::sleep_for( chrono::milliseconds(listenIntervalMillisec) );
    }

    VS_LOG_INFO << PRINT_HEADER << " archives maintenance THREAD is stopped" << endl;
}

bool VideoRecorder::startRecording( const TArchivingId & _archId, const SVideoSource & _source ){

    // TODO: for future with various params
    // 1 перезаписывается тот же обжрепр-архив ( потому что ресивер уже запущен - start_time есть )
    // 2 ресивер уже пишет этот стрим ( для других параметров нужно будет запустить другой )
    if( isSensorAlreadyRecording(_source.sensorId) ){
        return false;
    }

    SVideoSource sourceCopy = _source;
    checkSourceUrl( sourceCopy );

    std::future<TFutureStartResult> startInFuture = std::async( std::launch::async, & VideoRecorder::asyncRecordingStart, this, _archId, sourceCopy );
    m_mutexFuturesLock.lock();
    m_recordingStartFutures.push_back( std::move(startInFuture) );
    m_mutexFuturesLock.unlock();

    m_mutexRecordingsInfoInFutures.lock();
    m_recordingsFailedInFuture.insert( {_archId, sourceCopy} );
    m_mutexRecordingsInfoInFutures.unlock();

    return true;
}

VideoRecorder::TFutureStartResult VideoRecorder::asyncRecordingStart( const TArchivingId & _archId, SVideoSource sourceCopy ){

    // init source
    sourceCopy.runArchiving = true;
    if( ! m_settings.videoSourcesProvider->connectSource(sourceCopy) ){
        return TFutureStartResult( _archId, false );
    }

    // archiving
    setCrutchesForArchiver( sourceCopy );
    if( ! launchRecord(_archId, sourceCopy) ){
        return TFutureStartResult( _archId, false );
    }

    // watch for the source status
    m_settings.videoSourcesProvider->addSourceObserver( sourceCopy.sourceUrl, this );

    return TFutureStartResult( _archId, true );
}

void VideoRecorder::recordingStartFuturesMaintenance(){

    m_mutexFuturesLock.lock();
    for( auto iter = m_recordingStartFutures.begin(); iter != m_recordingStartFutures.end(); ){
        std::future<TFutureStartResult> & archiveFuture = ( * iter );

        const std::future_status futureStatus = archiveFuture.wait_for( std::chrono::milliseconds(10) );
        if( std::future_status::ready == futureStatus ){
            const TFutureStartResult startFromFuture = archiveFuture.get();

            // check Start state and get message if it failed
            string errMsg;
            PArchiveCreatorProxy archiver = getArchiver( startFromFuture.first, errMsg );

            // start failed
            if( ! startFromFuture.second ){
                // problem inside archiver
                if( archiver ){
                    SArchiverStateEvent event;
                    event.archivingState = archiver->getCurrentState().state;
                    event.sensorId = archiver->getSettings().sensorId;
                    event.message = archiver->getLastError();
                    event.archivingId = archiver->getSettings().archivingId;
                    m_settings.eventNotifier->sendEvent( event );
                }
                // problem outside archiver ( source connection for example )
                else{
                    VS_LOG_WARN << PRINT_HEADER
                                << " no archiver in std::future"
                                << " err msg [" << errMsg << "]"
                                << " problem outside archiver ( source connection for example )"
                                << endl;

                    m_mutexRecordingsInfoInFutures.lock();
                    const SVideoSource startSettings = m_recordingsFailedInFuture.find( startFromFuture.first )->second;
                    m_mutexRecordingsInfoInFutures.unlock();

                    SArchiverStateEvent event;
                    event.archivingState = EArchivingState::CRUSHED;
                    event.sensorId = startSettings.sensorId;
                    event.message = getLastError();
                    event.archivingId = startFromFuture.first;
                    m_settings.eventNotifier->sendEvent( event );
                }

                m_settings.systemEnvironment->serviceForWriteAheadLogging()->closeClientOperation( startFromFuture.first );
            }
            // start success
            else{
                SArchiverStateEvent event;
                event.archivingState = archiver->getCurrentState().state;
                event.sensorId = archiver->getSettings().sensorId;
                event.message = archiver->getLastError();
                event.archivingId = archiver->getSettings().archivingId;
                m_settings.eventNotifier->sendEvent( event );

                //
                if( CONFIG_PARAMS.RECORD_ENABLE_RETRANSLATION ){
                    SVideoSource source;
                    source.sensorId = archiver->getSettings().sensorId;
                    if( ! m_settings.videoRetranslator->launchRetranslation(source) ){
                        VS_LOG_WARN << PRINT_HEADER << " retranslation after archive success start failed" << endl;
                    }
                }

                m_mutexRecordingsInfoInFutures.lock();
                m_recordingsFailedInFuture.erase( startFromFuture.first );
                m_mutexRecordingsInfoInFutures.unlock();
            }

            // clear future
            iter = m_recordingStartFutures.erase( iter );
        }
        else{
            ++iter;
        }
    }
    m_mutexFuturesLock.unlock();
}

void VideoRecorder::archiversTick(){

    static const int64_t TICK_INTERVAL_MILLISEC = 2000;
    static int64_t lastTickAtMillisec = 0;
    const int64_t diff = common_utils::getCurrentTimeMillisec() - lastTickAtMillisec;

    if( diff > TICK_INTERVAL_MILLISEC ){

        m_muArchiverLock.lock();
        for( PArchiveCreatorProxy & creator : m_archiveCreators ){
            creator->tick();
        }
        m_muArchiverLock.unlock();

        lastTickAtMillisec = common_utils::getCurrentTimeMillisec();
    }
}

void VideoRecorder::setCrutchesForArchiver( SVideoSource & _source ){

    // TODO: эти параметры выдает video-receiver при запуске во время подключения источника, т.к. запись он начинает сразу

    const CrutchForCommonResources::Crutches & crutches = CrutchForCommonResources::singleton().get( _source.sourceUrl );

    _source.tempCrutchStartRecordTimeMillisec = crutches.tempCrutchStartRecordTimeMillisec;
    _source.tempCrutchSessionName = crutches.tempCrutchSessionName;
    _source.tempCrutchPlaylistName = crutches.tempCrutchPlaylistName;
}

bool VideoRecorder::stopRecording( const TArchivingId & _archId, bool _destroySession ){

    PArchiveCreatorProxy archiver;

    // search archive
    m_muArchiverLock.lock();
    auto iter = m_archiveCreatorsByArchivingId.find( _archId );
    if( iter != m_archiveCreatorsByArchivingId.end() ){
        archiver = iter->second;
    }
    else{
        stringstream ss;
        ss << PRINT_HEADER << " nothing to stop, such archiving id not found [" << _archId << "]. May be in start-futures...";
        VS_LOG_ERROR << ss.str() << endl;
        m_lastError = ss.str();

        // check in start-futures
        m_mutexRecordingsInfoInFutures.lock();
        auto iter2 = m_recordingsFailedInFuture.find( _archId );
        if( iter2 != m_recordingsFailedInFuture.end() ){
            const SVideoSource settingsForStart = iter2->second;

            VS_LOG_WARN << PRINT_HEADER << " archiving id [" << _archId << "] found in start-futures. Disconnect from appropriate source" << endl;
            m_settings.videoSourcesProvider->disconnectSource( settingsForStart.sourceUrl );

            m_mutexRecordingsInfoInFutures.unlock();
            m_muArchiverLock.unlock();
            return true;
        }
        m_mutexRecordingsInfoInFutures.unlock();

        m_muArchiverLock.unlock();
        return false;
    }
    m_muArchiverLock.unlock();

    // close him
    archiver->stop();
    removeArchiver( _archId );

    // deinit source
    if( ! m_settings.videoSourcesProvider->disconnectSource(archiver->getSettings().sourceUrl) ){
        return false;
    }

    return true;
}

ArchiveCreatorProxy::SCurrentState VideoRecorder::getArchiverState( const TArchivingId & _archId ){

    std::lock_guard<std::mutex> lock( m_muArchiverLock );

    auto iter = m_archiveCreatorsByArchivingId.find( _archId );
    if( iter != m_archiveCreatorsByArchivingId.end() ){
        PArchiveCreatorProxy archiver = iter->second;

        return archiver->getCurrentState();
    }
    else{
        stringstream ss;
        ss << "get archiver status failed, such archiving-id not found [" << _archId << "]";
        VS_LOG_ERROR << ss.str() << endl;
        m_lastError = ss.str();

        return ArchiveCreatorProxy::SCurrentState();
    }
}

std::vector<ArchiveCreatorLocal::SCurrentState> VideoRecorder::getArchiverStates(){

    std::vector<ArchiveCreatorLocal::SCurrentState> out;
    out.reserve( m_archiveCreators.size() );

    // active archivers
    m_muArchiverLock.lock();
    for( PArchiveCreatorProxy archiver : m_archiveCreators ){
        out.push_back( archiver->getCurrentState() );
    }
    m_muArchiverLock.unlock();

    // in starting progress
    m_mutexRecordingsInfoInFutures.lock();
    for( auto & valuePair : m_recordingsFailedInFuture ){
        const SVideoSource & videoSrc = valuePair.second;

        ArchiveCreatorLocal::SCurrentState toOut;
        toOut.state = EArchivingState::ACTIVE;

        // TODO: memory leak
        toOut.initSettings = new ArchiveCreatorProxy::SInitSettings();
        toOut.initSettings->archivingName = videoSrc.archivingName;
        toOut.initSettings->archivingId = valuePair.first;
        toOut.initSettings->sensorId = videoSrc.sensorId;

        out.push_back( toOut );
    }
    m_mutexRecordingsInfoInFutures.unlock();

    return out;
}

void VideoRecorder::callbackVideoSourceConnection( bool _estab, const std::string & _sourceUrl ){

    if( m_newDataBlockCreation ){
        return;
    }

    // close all archivers with such URL
    if( ! _estab ){
        // TODO: cyclic video source destroy
//        SVideoSource source;
//        source.sourceUrl = _sourceUrl;
//        stopRecording( source );
        if( ! m_shutdownCalled ){
            m_muArchiverLock.lock();
        }
        for( auto iter = m_archiveCreators.begin(); iter != m_archiveCreators.end(); ){
            PArchiveCreatorProxy & archiver = ( * iter );

            if( archiver->getSettings().sourceUrl == _sourceUrl ){
                m_archiveSettingsFromLostedSources.insert( {_sourceUrl, archiver->getSettings()} );

                // send State event
                SArchiverStateEvent event;
                event.archivingState = EArchivingState::CRUSHED; // TODO: dirty solution ( must be inside archiver )
                event.sensorId = archiver->getSettings().sensorId;
                event.message = "connection with camera related to this archiving is losted";
                event.archivingId = archiver->getSettings().archivingId;
                m_settings.eventNotifier->sendEvent( event );

                //
                iter = m_archiveCreators.erase( iter );
                m_archiveCreatorsByArchivingId.erase( archiver->getSettings().archivingId );
            }
            else{
                ++iter;
            }
        }
        if( ! m_shutdownCalled ){
            m_muArchiverLock.unlock();
        }
    }
    // restore archivers related with this URL, if they were
    else{
        //
        m_muArchiverLock.lock();
        for( auto iter = m_archiveCreators.begin(); iter != m_archiveCreators.end(); ++iter ){
            PArchiveCreatorProxy & creator = ( * iter );

            if( creator->getSettings().sourceUrl == _sourceUrl ){
                VS_LOG_WARN << PRINT_HEADER << " archiver with such source  [" << _sourceUrl << "] already in record."
                         << " Restore cancelled"
                         << endl;
                m_muArchiverLock.unlock();
                return;
            }
        }
        m_muArchiverLock.unlock();

        //
        auto iterRange = m_archiveSettingsFromLostedSources.equal_range( _sourceUrl );
        for( auto iter = iterRange.first; iter != iterRange.second; ++iter ){
            const ArchiveCreatorProxy::SInitSettings lostedSettings = iter->second;

            VS_LOG_WARN << PRINT_HEADER << " restore archiver with a source [" << _sourceUrl << "]"
                     << endl;

            SVideoSource temp;

            if( ! launchRecord(common_utils::generateUniqueId(), temp) ){
                // TODO: to warn
            }
        }
    }
}


bool VideoRecorder::launchRecord( const TArchivingId & _archId, const SVideoSource & _source ){

    const string & sessionName = _source.tempCrutchSessionName;

    // settings for local version
    ArchiveCreatorProxy::SInitSettings settings;
    settings.playlistDirLocationFullPath = CONFIG_PARAMS.HLS_WEBSERVER_DIR_ROOT + "/" + sessionName + "/" + _source.tempCrutchPlaylistName;
    settings.playlistLength = CONFIG_PARAMS.HLS_MAX_FILE_COUNT;
    settings.maxFiles = CONFIG_PARAMS.HLS_MAX_FILE_COUNT;    

    // common
    settings.archivingId = _archId;
    settings.archivingName = _source.archivingName;
    settings.sourceUrl = _source.sourceUrl;
    settings.sensorId = _source.sensorId;
    settings.communicationWithVideoSource = m_settings.videoSourcesProvider->getCommunicatorWithSource( _source.sourceUrl );
    settings.serviceOfSourceProvider = m_settings.videoSourcesProvider;

    PArchiveCreatorProxy archiveCreator;
    if( CONFIG_PARAMS.RECORD_ENABLE_REMOTE_ARCHIVER ){
        ArchiveCreatorRemote * remote = new ArchiveCreatorRemote( m_settings.internalCommunication, _source.sourceUrl );
        archiveCreator = std::make_shared<ArchiveCreatorProxy>( remote );
    }
    else{
        ArchiveCreatorLocal * local = new ArchiveCreatorLocal();
        archiveCreator = std::make_shared<ArchiveCreatorProxy>( local );
    }

    if( ! archiveCreator->start(settings) ){
        m_lastError = archiveCreator->getLastError();
        return false;
    }

    m_muArchiverLock.lock();
    m_archiveCreatorsByArchivingId.insert( {settings.archivingId, archiveCreator} );
    m_archiveCreators.push_back( archiveCreator );
    m_muArchiverLock.unlock();

    // write to database/objrepr
    SHlsMetadata meta;
    meta.tag = archiveCreator->getCurrentState().sessionName;
    meta.timeStartMillisec = archiveCreator->getCurrentState().timeStartMillisec;
    meta.timeEndMillisec = archiveCreator->getCurrentState().timeEndMillisec;
    meta.sourceUrl = settings.sourceUrl;
    meta.playlistDirLocationFullPath = settings.playlistDirLocationFullPath;

    m_settings.recordingMetadataStore->writeMetadata( meta );

    return true;
}

class HLSCreatorSearchPredicate {
public:
    HLSCreatorSearchPredicate( std::string _tag ) : m_tag(_tag) {}
    bool operator()( const PArchiveCreatorProxy & _inst ){ return ( m_tag == _inst->getCurrentState().sessionName ); }
private:
    std::string m_tag;
};

void VideoRecorder::updateArchivesMetadata(){

    static int64_t lastUpdateSec = 0;
    if( (common_utils::getCurrentTimeSec() - lastUpdateSec) <= CONFIG_PARAMS.RECORD_METADATA_UPDATE_INTERVAL_SEC ){
        return;
    }
    lastUpdateSec = common_utils::getCurrentTimeSec();

    // check
    m_muArchiverLock.lock();
    if( m_archiveCreators.empty() ){
        m_muArchiverLock.unlock();
        return;
    }
    m_muArchiverLock.unlock();

    // update
    const std::vector<SHlsMetadata> dbMetadata = m_settings.recordingMetadataStore->readMetadatas();
    for( const SHlsMetadata & dbMeta : dbMetadata ){

        m_muArchiverLock.lock();
        auto iter = find_if( m_archiveCreators.begin(), m_archiveCreators.end(), HLSCreatorSearchPredicate(dbMeta.tag) );
        if( iter != m_archiveCreators.end() ){

            PArchiveCreatorProxy creator = ( * iter );

            SHlsMetadata currentMeta;
            currentMeta.sourceUrl = creator->getSettings().sourceUrl;
            currentMeta.playlistDirLocationFullPath = creator->getSettings().playlistDirLocationFullPath;
            currentMeta.tag = creator->getCurrentState().sessionName;
            currentMeta.timeStartMillisec = creator->getCurrentState().timeStartMillisec;
            currentMeta.timeEndMillisec = creator->getCurrentState().timeEndMillisec;

            m_settings.recordingMetadataStore->writeMetadata( currentMeta );
        }
        m_muArchiverLock.unlock();
    }
}

void VideoRecorder::checkForArchiveNewBlock(){

    // check
    m_muArchiverLock.lock();
    if( m_archiveCreators.empty() ){
        m_muArchiverLock.unlock();
        return;
    }
    m_muArchiverLock.unlock();

    const int64_t recordNewDataBlockIntervalSec = CONFIG_PARAMS.RECORD_NEW_DATA_BLOCK_INTERVAL_MIN * 60;
    const int64_t additionalForSegmentFinishSec = 4;

    // collect settings from ready archivers
    vector<ArchiveCreatorProxy::SInitSettings> archiversForRestartSettings;
    m_muArchiverLock.lock();
    for( auto iter = m_archiveCreators.begin(); iter != m_archiveCreators.end(); ){
        PArchiveCreatorProxy creator = ( * iter );

        const int64_t diffMillisec = common_utils::getCurrentTimeMillisec() - creator->getCurrentState().timeStartMillisec;
        const int64_t triggerMillisec = ( recordNewDataBlockIntervalSec + additionalForSegmentFinishSec ) * 1000;

        // archive is ready
        if( (creator->getCurrentState().timeStartMillisec != 0) && ((diffMillisec / 1000) > (triggerMillisec / 1000)) ){
            m_newDataBlockCreation = true;

            VS_LOG_INFO << PRINT_HEADER
                        << " ==========================="
                        << " restart archiver [" << creator->getSettings().sourceUrl << "] for the new data block"
                        << " diff " << diffMillisec << " > trigger " << triggerMillisec
                        << " ==========================="
                        << endl;

            archiversForRestartSettings.push_back( creator->getSettings() );

            creator->stop();
            iter = m_archiveCreators.erase( iter );
            m_archiveCreatorsByArchivingId.erase( creator->getSettings().archivingId );

            // 'dereference' source consumers
            // TODO: may be replace into archiver ?
            m_settings.videoSourcesProvider->disconnectSource( creator->getSettings().sourceUrl );

            m_newDataBlockCreation = false;
        }
        else{
            ++iter;
        }
    }
    m_muArchiverLock.unlock();

    // restart ones
    for( const ArchiveCreatorProxy::SInitSettings & setting : archiversForRestartSettings ){

        SVideoSource source;
        source.runArchiving = true;
        source.sensorId = setting.sensorId;
        source.sourceUrl = setting.sourceUrl;
        source.archivingName = setting.archivingName;

        if( ! m_settings.videoSourcesProvider->connectSource(source) ){
            continue;
        }

        launchRecord( setting.archivingId, source );
    }
}

void VideoRecorder::checkForOutdatedArchives(){

    // TODO: disabled for temp
    return;

    const std::vector<SHlsMetadata> dbMetadata = m_settings.recordingMetadataStore->readMetadatas();
    for( const SHlsMetadata & dbMeta : dbMetadata ){

        if(  common_utils::timeMillisecToHour((dbMeta.timeEndMillisec - dbMeta.timeStartMillisec)) >
             CONFIG_PARAMS.RECORD_DATA_OUTDATE_AFTER_HOUR ){

        // NOTE: for fast check ( in minutes )
//        if( ((common_utils::getCurrentTimeMillisec() - dbMeta.timeEndMillisec) / 1000 / 60) >
//             CONFIG_PARAMS.RECORD_DATA_OUTDATE_AFTER_HOUR ){

            const string videoLocation = CONFIG_PARAMS.HLS_WEBSERVER_DIR_ROOT + "/" + dbMeta.tag;
            VS_LOG_INFO << "remove video location: " << videoLocation
                     << " current time: " << common_utils::getCurrentDateTimeStr()
                     << " right range of data: " << common_utils::timeMillisecToStr( dbMeta.timeEndMillisec )
                     << endl;

            boost::filesystem::remove_all( videoLocation );
            m_settings.recordingMetadataStore->deleteMetadata( dbMeta );
        }
    }
}

void VideoRecorder::checkForDiedArchiveCreators(){

    m_muArchiverLock.lock();
    for( auto iter = m_archiveCreators.begin(); iter != m_archiveCreators.end(); ){
        PArchiveCreatorProxy & creator = ( * iter );
        if( creator->isConnectionLosted() ){
            VS_LOG_INFO << PRINT_HEADER << " remove archiver with losted connection [" << creator->getSettings().sourceUrl << "]"
                     << endl;
            iter = m_archiveCreators.erase( iter );
        }
        else{
            ++iter;
        }
    }
    m_muArchiverLock.unlock();
}

void VideoRecorder::checkSourceUrl( SVideoSource & _source ){

    if( _source.sourceUrl.empty() ){
        _source.sourceUrl = OBJREPR_BUS.getSensorUrl( _source.sensorId );
    }
}

bool VideoRecorder::isSensorAlreadyRecording( TSensorId _sensorId ){

    m_muArchiverLock.lock();
    for( auto iter = m_archiveCreators.begin(); iter != m_archiveCreators.end(); ++iter ){
        PArchiveCreatorProxy & creator = ( * iter );

        if( creator->getSettings().sensorId == _sensorId ){
            stringstream ss;
            ss << PRINT_HEADER << " archiver with such sensor [" << _sensorId << "] already in record."
                     << " Start aborted"
                     << endl;
            VS_LOG_WARN << ss.str() << endl;
            m_lastError = ss.str();

            m_muArchiverLock.unlock();
            return true;
        }
    }
    m_muArchiverLock.unlock();
    return false;
}

PArchiveCreatorProxy VideoRecorder::getArchiver( const TArchivingId & _archId, std::string & _msg ){

    if( _archId.empty() ){
        stringstream ss;
        ss << "empty archiving id";
        _msg = ss.str();
        return nullptr;
    }

    m_muArchiverLock.lock();
    auto iter = m_archiveCreatorsByArchivingId.find( _archId );
    if( iter != m_archiveCreatorsByArchivingId.end() ){
        m_muArchiverLock.unlock();
        return iter->second;
    }
    else{
        stringstream ss;

        ss << "archiver with such archiving id [" << _archId << "] not found. Available: [";
        for( auto iter = m_archiveCreatorsByArchivingId.begin(); iter != m_archiveCreatorsByArchivingId.end(); ++iter ){
            PArchiveCreatorProxy archiver = iter->second;
            ss << archiver->getSettings().archivingId << " ";
        }
        ss << "]";

        m_muArchiverLock.unlock();
        _msg = ss.str();
        return nullptr;
    }
}

void VideoRecorder::removeArchiver( const TArchivingId & _archId ){

    m_muArchiverLock.lock();
    auto iter = m_archiveCreatorsByArchivingId.find( _archId );
    if( iter != m_archiveCreatorsByArchivingId.end() ){

        m_archiveCreatorsByArchivingId.erase( iter );
    }

    for( auto iter = m_archiveCreators.begin(); iter != m_archiveCreators.end();  ){
        PArchiveCreatorProxy analyzer = ( * iter );

        if( analyzer->getSettings().archivingId == _archId ){
            m_archiveCreators.erase( iter );
            break;
        }
        else{
            ++iter;
        }
    }
    m_muArchiverLock.unlock();
}

std::vector<PArchiveCreatorProxyConst> VideoRecorder::getArchivers(){

    std::vector<PArchiveCreatorProxyConst> out;

    // TODO: do
//    std::copy(  );

    m_muArchiverLock.lock();
    out.reserve( m_archiveCreators.size() );
    for( PArchiveCreatorProxy hls : m_archiveCreators ){
        out.push_back( hls );
    }
    m_muArchiverLock.unlock();

    return out;
}

void VideoRecorder::collectGarbage(){

    // TODO: resolve this code
    std::map<TUrl, SLostedSource> m_lostedSources;


    std::vector<PArchiveCreatorProxyConst> archives = getArchivers();

    // destroy RTSP-server retranslation(s) for losted connection(s)
    for( PArchiveCreatorProxyConst & creator : archives ){
        if( creator->isConnectionLosted() ){

            VS_LOG_INFO << "destroy objects (RTSP/HLS) correlated with losted connection: " << creator->getSettings().sourceUrl
                     << endl;

            SVideoSource videoSource;
            videoSource.sourceUrl = creator->getSettings().sourceUrl;
//            m_sourceManager->stopRetranslation( videoSource );
        }
    }

    // destroy HLS-creator(s) for losted connection(s)
    for( PArchiveCreatorProxyConst & creator : archives ){
        if( creator->isConnectionLosted() ){
            SVideoSource videoSource;
            videoSource.sourceUrl = creator->getSettings().sourceUrl;
//            m_videoRecorder->stopRecording( videoSource );
        }
    }

    // losted sources list
    for( PArchiveCreatorProxyConst & creator : archives ){

        if( creator->isConnectionLosted() ){

            auto iter = m_lostedSources.find( creator->getSettings().sourceUrl );
            if( iter == m_lostedSources.end() ){

                SLostedSource lostedSource;
                lostedSource.url = creator->getSettings().sourceUrl;
                lostedSource.lostedAtTimeSec = common_utils::getCurrentTimeSec();

                m_lostedSources.insert( {creator->getSettings().sourceUrl, lostedSource} );
                VS_LOG_INFO << "add to list losted connection: " << creator->getSettings().sourceUrl
                         << endl;
            }
            else{
                SLostedSource & lostedSource = iter->second;
                lostedSource.lostedAtTimeSec = common_utils::getCurrentTimeSec();
            }
        }
    }
}

void VideoRecorder::reconnectLostedSources(){

    // TODO: resolve this code
    std::map<TUrl, SLostedSource> m_lostedSources;

    for( auto & valuePair : m_lostedSources ){

        SLostedSource & lostedSource = valuePair.second;

        if( lostedSource.connectRetries <= CONFIG_PARAMS.baseParams.SYSTEM_CONNECT_RETRIES &&
            (common_utils::getCurrentTimeSec() - lostedSource.lostedAtTimeSec) > CONFIG_PARAMS.baseParams.SYSTEM_CONNECT_RETRY_PERIOD_SEC ){

            VS_LOG_INFO << "try reconnect to source: " << lostedSource.url
                     << endl;

//            PCommandSourceStartRetranslate cmd = std::make_shared<CommandSourceStartRetranslate>( & m_commandServices );
//            cmd->m_sourceToConnect.sourceUrl = lostedSource.url;
//            cmd->exec();

            lostedSource.lostedAtTimeSec = 0;
            lostedSource.connectRetries++;
        }
    }
}
