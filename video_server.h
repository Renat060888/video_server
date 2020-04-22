#ifndef VIDEO_SERVER_H
#define VIDEO_SERVER_H

#include <string>
#include <future>

#include <boost/signals2.hpp>

#include "common/common_types.h"
#include "system/system_environment_facade_vs.h"
#include "communication/communication_gateway_facade_vs.h"
#include "storage/storage_engine_facade.h"
#include "storage/video_recorder.h"
#include "datasource/source_manager_facade.h"
#include "analyze/analytic_manager_facade.h"

class VideoServer
{
public:
    struct SInitSettings {

    };

    struct SState {
        SInitSettings settings;
        std::string lastError;
    };

    static void callbackUnixInterruptSignal();

    VideoServer();
    ~VideoServer();

    bool init( const SInitSettings & _settings );
    const SState & getState(){ return m_state; }

    void launch();


private:
    static boost::signals2::signal<void()> m_unixInterruptSignal;

    void shutdown();
    void shutdownByUnixInterruptSignal();
    void checkForSelfShutdown();

    // data
    SState m_state;
    common_types::SIncomingCommandServices m_commandServices;
    std::atomic<bool> m_shutdownCalled;
    std::future<void> m_selfShutdownFuture;

    // service
    SystemEnvironmentFacadeVS * m_systemEnvironment;
    CommunicationGatewayFacadeVS * m_communicateGateway;
    StorageEngineFacade * m_storageEngine;
    SourceManagerFacade * m_sourceManager;
    AnalyticManagerFacade * m_analyticManager;

    // TODO: put into StorageEngineFacade
    VideoRecorder * m_videoRecorder;
};

#endif // VIDEO_SERVER_H
