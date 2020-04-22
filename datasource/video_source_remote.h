#ifndef VIDEO_SOURCE_REMOTE_H
#define VIDEO_SOURCE_REMOTE_H

#include <thread>
#include <condition_variable>

#include <microservice_common/communication/network_interface.h>
#include <microservice_common/system/process_launcher.h>

#include "common/common_types.h"
#include "video_source_proxy.h"
#include "system/wal.h"

class VideoSourceRemote : public IVideoSource, public INetworkObserver, public IProcessObserver
{
public:        
    VideoSourceRemote( common_types::IInternalCommunication * _internalCommunicationService, const std::string & _clientName );
    ~VideoSourceRemote();

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

    bool launchSourceProcess( const SInitSettings & _settings );
    bool establishCommunicationWithProcess( const std::string & _sourceUrl );
    bool waitForStreamStartEvent();

    bool makeConnectAttempts();
    void attemptToConnect();

    void getArchiveParameters();

    // data
    int32_t m_referenceCounter;
    SInitSettings m_settings;
    std::string m_lastError;
    std::string m_callbackMsgArchiveData;
    std::string m_callbackMsgStreamData;
    std::vector<common_types::ISourceObserver *> m_observers;
    bool m_signalLosted;
    bool m_correlatedProcessCrashed;
    bool m_connectCallFinished;
    bool m_disconnectCalled;
    common_types::ISourcesProvider::SSourceParameters m_sourceParametersFromStreamEvent;
    std::atomic<bool> m_requestInited;
    bool m_streamStartEventArrived = false;
    ProcessHandle * m_processHandle;
    bool m_shutdown;

    // service    
    common_types::IInternalCommunication * m_internalCommunicationService;
    PNetworkClient m_remoteSourceCommunicator;
    PNetworkClient m_networkForConnectedConsumers;
    std::condition_variable m_cvResponseWait;
    std::condition_variable m_cvArchiveSessionParametersWait;
    WriteAheadLogger m_walForDump;
    std::mutex m_muConnectAttempts;
    std::mutex m_muPublicThreadSafety;
};
using PVideoSourceRemote = std::shared_ptr<VideoSourceRemote>;

#endif // VIDEO_SOURCE_REMOTE_H
