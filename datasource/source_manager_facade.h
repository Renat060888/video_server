#ifndef SOURCE_MANAGER_FACADE_H
#define SOURCE_MANAGER_FACADE_H

#include <map>
#include <mutex>

#include "common/common_types.h"
#include "video_retranslator.h"
#include "video_source_proxy.h"

class SourceManagerFacade : public common_types::ISourcesProvider
                          , public common_types::ISourceObserver
                          , public common_types::ISourceRetranslation
{
public:
    struct SConsumerDescr {
        SConsumerDescr()
            : sensorId(0)
            , connectedSuccessful(false)
            , allowToConnect(false)
            , abortConnection(false)
        {}
        common_types::TSensorId sensorId;
        bool connectedSuccessful;
        bool allowToConnect;
        bool abortConnection;
    };

    struct SServiceLocator {
        SServiceLocator()
            : analyticStore(nullptr)
            , internalCommunication(nullptr)
        {}
        common_types::IEventStore * analyticStore;
        common_types::IInternalCommunication * internalCommunication;
        SystemEnvironmentFacadeVS * systemEnvironment;
    };

    struct SInitSettings {
        SServiceLocator serviceLocator;
    };

    SourceManagerFacade();
    ~SourceManagerFacade();

    // facade services
    ISourcesProvider * getServiceOfSourceProvider();
    ISourceRetranslation * getServiceOfSourceRetranslation();

    bool init( SInitSettings _settings );    
    void shutdown();
    virtual std::string getLastError() override;

    std::vector<VideoRetranslator::SCurrentState> getRTSPStates();
    std::vector<PConstVideoRetranslator> getRTSPSources() const;


private:
    void threadSourcesMaintenance();

    virtual std::vector<common_types::SVideoSource> getSourcesMedadata() override;
    virtual void callbackVideoSourceConnection( bool _estab, const std::string & _sourceUrl ) override;

    // ISourceRetranslation interface
    virtual bool launchRetranslation( common_types::SVideoSource _source ) override;
    virtual bool stopRetranslation( const common_types::SVideoSource & _source ) override;

    // ISourcesProvider interface
    bool connectSource( common_types::SVideoSource _source ) override;
    bool disconnectSource( const std::string & _sourceUrl  ) override;
    void addSourceObserver( const std::string & _sourceUrl, ISourceObserver * _observer ) override;
    virtual PNetworkClient getCommunicatorWithSource( const std::string & _sourceUrl ) override;
    virtual ISourcesProvider::SSourceParameters getShmRtpStreamParameters( const std::string & _sourceUrl ) override;

    bool createRetranslator( const common_types::SVideoSource & _source );
    int32_t getRTSPSourcePort();

    bool enterToConnectQueue( const common_types::SVideoSource & _source );
    void exitFromConnectQueue( const common_types::SVideoSource & _source, bool _success );

    // data
    SInitSettings m_settings;
    std::map<std::string, common_types::SVideoSource> m_sourceParametersFromDiedRetranslators;
    std::unordered_map<common_types::TSensorId, std::queue<SConsumerDescr *>> m_consumersWaitingForConnect;

    // service
    std::mutex m_sourcesLock;
    bool m_shutdownCalled;
    int32_t m_rtspSourcePortCounter;
    std::vector<PVideoRetranslator> m_retranslators;
    std::vector<PVideoSourceProxy> m_videoSources;
    std::map<std::string, PVideoSourceProxy> m_videoSourcesInTransitionState;
    std::string m_lastError;
    std::thread * m_threadSourcesMaintenance;
    std::mutex m_muConsumersQueue;
};

#endif // SOURCE_MANAGER_FACADE_H
