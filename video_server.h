#ifndef VIDEO_SERVER_H
#define VIDEO_SERVER_H

#include <string>
#include <future>

#include <boost/signals2.hpp>

#include "common/common_types.h"
#include "system/system_environment_facade_vs.h"
#include "communication/communication_gateway_facade_vs.h"
#include "storage/storage_engine_facade.h"
#include "datasource/source_manager_facade.h"

class VideoServer
{
    static void callbackUnixInterruptSignal();
    static boost::signals2::signal<void()> m_unixInterruptSignal;
public:
    struct SInitSettings {

    };

    struct SState {
        SInitSettings settings;
        std::string lastError;
    };

    VideoServer();
    ~VideoServer();

    bool init( const SInitSettings & _settings );
    const SState & getState(){ return m_state; }

    void launch();


private:
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
};

#endif // VIDEO_SERVER_H
