#ifndef ANALYZER_REMOTE_H
#define ANALYZER_REMOTE_H

#include <mutex>

#include <microservice_common/system/process_launcher.h>

#include "video_server_common/communication/protocols/internal_communication_analyzer.pb.h"
#include "analyzer_proxy.h"

class AnalyzerRemote : public IAnalyzer, public INetworkObserver, public IProcessObserver
{
public:
    AnalyzerRemote( common_types::IInternalCommunication * _internalCommunicationService, const std::string & _clientName );
    ~AnalyzerRemote();

    bool start( SInitSettings _settings ) override;
    void resume() override;
    void pause() override;
    void stop() override;

    const SAnalyzeStatus & getStatus() override;
    const SInitSettings & getSettings() override { return m_settings; }
    const std::string & getLastError() override { return m_lastError; }

    std::vector<SAnalyticEvent> getAccumulatedEvents() override;

    void setLivePlaying( bool _live ) override;

private:
    virtual void callbackNetworkRequest( PEnvironmentRequest _request ) override;
    virtual void callbackProcessCrashed( ProcessHandle * _handle ) override;

    void createConstRequests();

    std::string createStartMessage( const SInitSettings & _settings );
    std::string createPauseMessage();
    std::string createResumeMessage();
    std::string createStopMessage();

    // data
    std::string m_lastError;
    SInitSettings m_settings;
    SAnalyzeStatus m_status;
    ProcessHandle * m_processHandle;
    std::string m_constRequestGetAccumEvents;
    std::string m_moduleName;

    // service
    std::mutex m_networkLock;
    common_types::IInternalCommunication * m_internalCommunicationService;
    PNetworkClient m_remoteAnalyzerCommunicator;
    video_server_protocol::ProtobufInternalCommunicateAnalyzer m_protobufAnalyzerOut;
    video_server_protocol::ProtobufInternalCommunicateAnalyzer m_protobufAnalyzerIn;
};
using PAnalyzerRemote = std::shared_ptr<AnalyzerRemote>;

#endif // ANALYZER_REMOTE_H
