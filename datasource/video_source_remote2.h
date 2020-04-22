#ifndef VIDEO_SOURCE_REMOTE2_H
#define VIDEO_SOURCE_REMOTE2_H

#include <future>

#include <microservice_common/system/process_launcher.h>

#include "video_source_proxy.h"

class VideoSourceRemote2 : public IVideoSource, public INetworkObserver, public IProcessObserver
{
public:
    VideoSourceRemote2( common_types::IInternalCommunication * _internalCommunicationService );
    ~VideoSourceRemote2();

    virtual const SInitSettings & getSettings() override;
    virtual const std::string & getLastError() override;

    virtual bool connect( const SInitSettings & _settings ) override;
    virtual void disconnect() override;
    virtual void addObserver( common_types::ISourceObserver * _observer ) override;

    virtual PNetworkClient getCommunicator() override;
    virtual int32_t getReferenceCount() override;
    virtual common_types::ISourcesProvider::SSourceParameters getStreamParameters() override;


private:
    virtual void callbackNetworkRequest( PEnvironmentRequest _request ) override;
    virtual void callbackProcessCrashed( ProcessHandle * _handle ) override;

    bool isUrlValid( const std::string & _url );
    std::pair<int, int> getWidthHeight( const std::string & _caps );

    bool makeConnectAttempt( const SInitSettings & _settings );
    void attemptsCycle();

    bool launchSourceProcess( const SInitSettings & _settings );
    bool establishCommunicationWithProcess( const std::string & _sourceUrl );
    bool waitForStreamStartEvent();

    void shutdown();
    void destroyRecources();

    // data
    SInitSettings m_settings;
    int32_t m_referenceCounter;
    std::vector<common_types::ISourceObserver *> m_observers;
    std::string m_lastError;
    ProcessHandle * m_processHandle;
    bool m_streamStartEventArrived;
    bool m_signalExist;
    std::string m_callbackMsgArchiveData;
    std::string m_callbackMsgStreamData;
    bool m_shutdownCalled;
    bool m_attemptsExhausted;
    bool m_attemptsCycleActive;
    bool m_interruptAttempts;

    // service
    common_types::IInternalCommunication * m_internalCommunicationService;
    PNetworkClient m_remoteSourceCommunicator;
    PNetworkClient m_splittedNetworkForConnectedConsumers;
    std::condition_variable m_cvResponseEvent;
    std::future<bool> m_futureCallbackConnectAttempts;
    std::future<void> m_futureCycleConnectAttempts;
    std::mutex m_mutexAttemptArea;
};

#endif // VIDEO_SOURCE_REMOTE2_H
