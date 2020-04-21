#ifndef ANALYTIC_MANAGER_FACADE_H
#define ANALYTIC_MANAGER_FACADE_H

#include <thread>
#include <future>
#include <map>

#include <video_server_common/storage/database_manager_astra.h>

#include "analyzer_proxy.h"
#include "common/common_types.h"
#include "videocard_balancer.h"
#include "analytics_statistics_client.h"
#include "player_dispatcher.h"

class AnalyticManagerFacade : public common_types::ISourceObserver
{
public:
    using TFutureStartResult = std::pair<common_types::TProcessingId, bool>;

    struct SServiceLocator {
        SServiceLocator()
            : eventStore(nullptr)
            , eventNotifier(nullptr)
            , eventProcessor(nullptr)
            , eventRetranslator(nullptr)
            , internalCommunication(nullptr)
            , videoSources(nullptr)            
        {}
        common_types::IEventStore * eventStore;
        common_types::IEventNotifier * eventNotifier;
        common_types::IEventProcessor * eventProcessor;
        common_types::IEventRetranslator * eventRetranslator;
        common_types::IInternalCommunication * internalCommunication;
        common_types::IExternalCommunication * externalCommunication;
        common_types::ISourcesProvider * videoSources;
    };

    struct SInitSettings {
        SServiceLocator serviceLocator;
    };

    AnalyticManagerFacade();
    ~AnalyticManagerFacade();

    bool init( SInitSettings _settings );
    void shutdown();
    const std::string & getLastError(){ return m_lastError; }

    bool startAnalyze( AnalyzerProxy::SInitSettings _settings );
    bool resumeAnalyze( common_types::TProcessingId _procId );
    bool pauseAnalyze( common_types::TProcessingId _procId );
    bool stopAnalyze( common_types::TProcessingId _procId );

    AnalyzerProxy::SAnalyzeStatus getAnalyzerStatus( common_types::TProcessingId _procId );
    std::vector<AnalyzerProxy::SAnalyzeStatus> getAnalyzersStatuses();
    const std::map<std::string, common_types::SAnalyticEvent> & getLastAnalyzersEvent();

    // facade services
    IObjectsPlayer * getPlayingService();    

    void setLivePlaying( bool _live );

private:
    virtual void callbackVideoSourceConnection( bool _estab, const std::string & _sourceUrl ) override;

    void threadAnalyzersMaintenance();
    TFutureStartResult asyncAnalyzeStart( AnalyzerProxy::SInitSettings _settings );
    void callbackFromAnalyzer( const common_types::SAnalyticEvent & _event );

    void processEvents();
    void checkStartResults();
    void updatePlayer();

    std::map<uint64_t, SAnalyzeProfile> getSensorProfiles( common_types::TSensorId _sensorId );
    std::map<std::string, uint64_t> getDpfObjreprMapping( common_types::TProfileId _profileId );
    std::map<std::string, uint64_t> getDpfObjreprMapping( const std::vector<uint64_t> & _filterObjreprClassinfoId );

    PAnalyzerProxy createAnalyzer( const AnalyzerProxy::SInitSettings & _settings );
    inline PAnalyzerProxy getAnalyzer( const common_types::TProcessingId _processingId, std::string & _msg );
    void removeAnalyzer( const common_types::TProcessingId _processingId );
    bool isAnalyzerSettingsUnique( const AnalyzerProxy::SInitSettings & _settings );
    bool isAnotherAnalyzerStarting();

    int32_t setProcessingFlags();
    common_types::SAnalyzedObjectEvent pullProcessingPart( const common_types::TProcessingId & _procId, const common_types::SAnalyticEvent & _event );

    // data
    SInitSettings m_settings;    
    bool m_shutdownCalled;
    std::vector<PAnalyzerProxy> m_analyzers;
    std::map<common_types::TProcessingId, PAnalyzerProxy> m_analyzersByProcessingId;
    std::map<std::string, common_types::SAnalyticEvent> m_lastAnalyzerEvent;
    std::string m_lastError;
    std::vector<std::future<TFutureStartResult>> m_analyzeStartFutures;
    std::map<common_types::TProcessingId, AnalyzerProxy::SInitSettings> m_analyzersFailedInFuture;

    // service
    std::thread * m_threadEventsRetranslate;
    std::mutex m_muAnalyzers;
    std::mutex m_muStartFutures;
    std::mutex m_muPlayer;
    AnalyticsStatisticsClient m_statisticsClient;
    Json::Reader m_jsonReader;
    VideocardBalancer m_videocardBalancer;
    IObjectsPlayer * m_player;
    DatabaseManager * m_database;
    PlayerDispatcher m_playerDispatcher;    
};

#endif // ANALYTIC_MANAGER_FACADE_H
