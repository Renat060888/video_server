#ifndef VIDEO_RECORDER_H
#define VIDEO_RECORDER_H

#include <map>
#include <unordered_map>
#include <thread>
#include <condition_variable>
#include <future>

#include "common/common_types.h"
#include "archive_creator_proxy.h"


// TODO: class-crutch
#include "common/common_utils.h"
#include "system/config_reader.h"

class CrutchForCommonResources {
public:
    static CrutchForCommonResources & singleton(){
        static CrutchForCommonResources instance;
        return instance;
    }

    struct Crutches {
        // crutches for archiver ( set by network callback from video-receiver )
        int64_t tempCrutchStartRecordTimeMillisec;
        std::string tempCrutchSessionName;
        std::string tempCrutchPlaylistName;


        // TODO: deprecarted
        std::string tempCrutchTag;
    };

    void recalculate( const std::string & _url ){

        // create dir
//        const std::string tagTemplate = "archive_session";
//        const std::string tagSource = common_utils::filterByDigitAndNumber( common_utils::cutBySimbol( '@', _url ).second );
//        const int64_t archiveCreateTimeMillisec = common_utils::getCurrentTimeMillisec();
//        const std::string tag = tagTemplate + "_" + tagSource + "_" + std::to_string( archiveCreateTimeMillisec / 1000 );

//        const std::string dirFullPath = CONFIG_PARAMS.HLS_WEBSERVER_DIR + "/" + tag;
//        const int res = ::mkdir( dirFullPath.c_str(), 0777 );
//        if( -1 == res ){
//            LOG_CRITICAL << "Can't create HLS directory for server: " << dirFullPath << std::endl;
//            return;
//        }

        // ----------------------------------------------------------------------------------------
        // TODO: horrible crutch ! Because video-receiver begin recording immediately at startup
        // ----------------------------------------------------------------------------------------

        Crutches crutches;
        m_crutches.erase( _url );
        m_crutches.insert( {_url, crutches} );
    }

    Crutches & getForModify( const std::string & _url ){
        auto iter = m_crutches.find( _url );
        if( iter != m_crutches.end() ){
            return iter->second;
        }
        else{
            Crutches crutches;
            m_crutches.erase( _url );
            auto pair = m_crutches.insert( {_url, crutches} );
            return pair.first->second;
        }
    }

    const Crutches & get( const std::string & _url ){
        auto iter = m_crutches.find( _url );
        if( iter != m_crutches.end() ){
            return iter->second;
        }
        else{
            recalculate( _url );
            return get( _url );
        }
    }

    std::map<std::string, Crutches> m_crutches;
};

// TODO: move to StorageEngine
class VideoRecorder : public ISourceObserver
{
    using TFutureStartResult = std::pair<TArchivingId, bool>;
public:
    struct SServiceLocator {
        SServiceLocator()
        {}
        // TODO: do
    };

    struct SInitSettings {
        SInitSettings()
            : recordingMetadataStore(nullptr)
            , videoSourcesProvider(nullptr)
            , videoRetranslator(nullptr)
            , internalCommunication(nullptr)
            , eventNotifier(nullptr)
        {}
        common_types::IRecordingMetadataStore * recordingMetadataStore;
        common_types::ISourcesProvider * videoSourcesProvider;
        common_types::ISourceRetranslation * videoRetranslator;
        common_types::IInternalCommunication * internalCommunication;
        common_types::IEventNotifier * eventNotifier;
    };

    VideoRecorder();
    ~VideoRecorder();

    bool init( SInitSettings _settings );
    void shutdown();
    std::string getLastError();

    bool startRecording( const common_types::TArchivingId & _archId, const common_types::SVideoSource & _source );
    bool stopRecording( const common_types::TArchivingId & _archId, bool _destroySession );

    std::vector<PArchiveCreatorProxyConst> getArchivers();
    ArchiveCreatorProxy::SCurrentState getArchiverState( const common_types::TArchivingId & _archId );
    std::vector<ArchiveCreatorProxy::SCurrentState> getArchiverStates();


private:
    virtual void callbackVideoSourceConnection( bool _estab, const std::string & _sourceUrl ) override;

    void threadArchivesMaintenance();

    void setCrutchesForArchiver( SVideoSource & _source );

    TFutureStartResult asyncRecordingStart( const common_types::TArchivingId & _archId, common_types::SVideoSource sourceCopy );
    bool launchRecord( const common_types::TArchivingId & _archId, const common_types::SVideoSource & _source );

    void updateArchivesMetadata();
    void checkForArchiveNewBlock();
    void checkForOutdatedArchives();
    void recordingStartFuturesMaintenance();
    void archiversTick();

    PArchiveCreatorProxy getArchiver( const common_types::TArchivingId & _archId, std::string & _msg );
    void removeArchiver( const common_types::TArchivingId & _archId );

    void checkSourceUrl( common_types::SVideoSource & _source );
    bool isSensorAlreadyRecording( common_types::TSensorId _sensorId );

    // TODO: deprecated ?
    void checkForDiedArchiveCreators();
    void reconnectLostedSources();
    void collectGarbage();

    // data
    SInitSettings m_settings;
    std::string m_lastError;
    bool m_shutdownCalled;
    std::vector<PArchiveCreatorProxy> m_archiveCreators;
    std::unordered_map<common_types::TArchivingId, PArchiveCreatorProxy> m_archiveCreatorsByArchivingId;
    std::multimap<std::string, ArchiveCreatorProxy::SInitSettings> m_archiveSettingsFromLostedSources;
    std::vector<std::future<TFutureStartResult>> m_recordingStartFutures; // TODO: future to map's value
    std::map<common_types::TArchivingId, common_types::SVideoSource> m_recordingsFailedInFuture;
    bool m_newDataBlockCreation;

    // service
    std::thread * m_threadHlsMaintenance;
    std::mutex m_muArchiverLock;
    std::mutex m_mutexFuturesLock;
    std::mutex m_mutexRecordingsInfoInFutures;

};

#endif // VIDEO_RECORDER_H
