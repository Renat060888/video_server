#ifndef RECORDER_REMOTE_H
#define RECORDER_REMOTE_H

#include <thread>

#include <microservice_common/communication/network_interface.h>
#include <microservice_common/system/process_launcher.h>

#include "common/common_types.h"
#include "archive_creator_proxy.h"

class ArchiveCreatorRemote : public AArchiveCreator, public INetworkObserver, public IProcessObserver
{
public:
    ArchiveCreatorRemote( common_types::IInternalCommunication * _internalCommunicationService, const std::string & _clientName );
    ~ArchiveCreatorRemote();

    virtual bool start( const SInitSettings & _settings ) override;
    virtual void stop() override;

    virtual bool isConnectionLosted() const override;
    virtual void tick() override;

    virtual const SInitSettings & getSettings() const override;
    virtual const SCurrentState & getCurrentState() override;
    virtual const std::string & getLastError() override;

private:
    // TODO: is this callbacks work ?
    virtual void callbackNetworkRequest( PEnvironmentRequest _request ) override;
    virtual void callbackProcessCrashed( ProcessHandle * _handle ) override;
    virtual bool getArchiveSessionInfo( int64_t & _startRecordTimeMillisec,
                                        std::string & _sessionName,
                                        std::string & _playlistName ) override;

    // data
    SInitSettings m_settings;
    SCurrentState m_currentState;
    bool m_connectionLosted;
    std::string m_lastError;
    bool m_closed;    
    std::string m_liveStreamPlaylistDirFullPath;
    std::string m_originalPlaylistFullPath;
    std::string m_callbackMsgArchiveData;
    std::string m_playlistName;
    std::atomic<bool> m_requestInited;

    // service
    std::condition_variable m_cvResponseWait;    


};

#endif // RECORDER_REMOTE_H
