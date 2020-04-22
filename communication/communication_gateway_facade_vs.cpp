
#include <microservice_common/common/ms_common_utils.h>
#include <microservice_common/communication/amqp_client_c.h>

#include "system/config_reader.h"
#include "system/objrepr_bus_vs.h"
#include "system/path_locator.h"
#include "command_factory.h"
#include "async_notifier.h"
#include "communication_gateway_facade_vs.h"

using namespace std;
using namespace common_types;

CommunicationGatewayFacadeVS::CommunicationGatewayFacadeVS()
    : CommunicationGatewayFacade()
{

}

CommunicationGatewayFacadeVS::~CommunicationGatewayFacadeVS(){

}

bool CommunicationGatewayFacadeVS::init( const SInitSettings & _settings ){

    // initial connections in base class
    SAmqpRouteParameters initialRoute;
    initialRoute.predatorExchangePointName = "video_server_dx";
    initialRoute.predatorQueueName = "video_server_q_mailbox";
    initialRoute.predatorRoutingKeyName = "video_server_rk";

    m_settings = _settings;
    m_settings.paramsForInitialAmqp.route = initialRoute;
    m_settings.paramsForInitialAmqp.enable = CONFIG_PARAMS.baseParams.COMMUNICATION_AMQP_ENABLE;
    m_settings.paramsForInitialAmqp.host = CONFIG_PARAMS.baseParams.COMMUNICATION_AMQP_SERVER_HOST;
    m_settings.paramsForInitialAmqp.virtHost = CONFIG_PARAMS.baseParams.COMMUNICATION_AMQP_VIRTUAL_HOST;
    m_settings.paramsForInitialAmqp.port = CONFIG_PARAMS.baseParams.COMMUNICATION_AMQP_SERVER_PORT;
    m_settings.paramsForInitialAmqp.login = CONFIG_PARAMS.baseParams.COMMUNICATION_AMQP_LOGIN;
    m_settings.paramsForInitialAmqp.pass = CONFIG_PARAMS.baseParams.COMMUNICATION_AMQP_PASS;
    m_settings.paramsForInitialObjrepr.enable = CONFIG_PARAMS.baseParams.COMMUNICATION_OBJREPR_SERVICE_BUS_ENABLE;
    m_settings.paramsForInitialObjrepr.serverMirrorIdInContext = OBJREPR_BUS.getVideoServerObjreprMirrorId();
    m_settings.specParams.factory = new CommandFactory( m_settings.services );

    if( ! CommunicationGatewayFacade::init(m_settings) ){
        return false;
    }

    // get some of conns
    m_asyncNotifierNetworkClient = CommunicationGatewayFacade::getInitialObjreprConnection();

    return true;
}

IInternalCommunication * CommunicationGatewayFacadeVS::serviceForInternalCommunication(){
    return this;
}

common_types::IEventNotifier * CommunicationGatewayFacadeVS::getServiceOfEventNotifier(){
    AsyncNotifier * asyncNotifier = new AsyncNotifier();

    AsyncNotifier::SInitSettings settings;
    settings.networkClient = m_asyncNotifierNetworkClient;
    if( ! asyncNotifier->init(settings) ){
        delete asyncNotifier;
        return nullptr;
    }

    m_asyncNotifier = asyncNotifier;
    return m_asyncNotifier;
}

PNetworkClient CommunicationGatewayFacadeVS::getAnalyticCommunicator( std::string _clientName ){
    assert( false && "remote analyzer is already deprecated" );
    return nullptr;
}

PNetworkClient CommunicationGatewayFacadeVS::getSourceCommunicator( std::string _clientName ){
    return createCommunicatorWithRecevier( _clientName );
}

PNetworkClient CommunicationGatewayFacadeVS::getArchiveCommunicator( std::string _clientName ){
    assert( false && "archiver communicate with video-receiver via 'NetworkSplitter'" );
    return nullptr;
}

PNetworkClient CommunicationGatewayFacadeVS::getServiceCommunicator(){
    assert( false && "TODO: what for ? (previous used for: message transmitter, along hearbeats)" );
    return nullptr;
}

PNetworkClient CommunicationGatewayFacadeVS::getEventRetranslateCommunicator( std::string _clientName ){
    PNetworkClient network;
    // TODO: create objrepr listener for connection with SMPC
    return network;
}

PNetworkClient CommunicationGatewayFacadeVS::getPlayerCommunicator( std::string _clientName ){
    PNetworkClient network;
    // TODO: create amqp client for connection with PlayerWorker
    return network;
}

PNetworkClient CommunicationGatewayFacadeVS::createCommunicatorWithRecevier( const std::string & _clientName ){
    SConnectParamsShell params;
    params.client = true;
    params.socketName = PATH_LOCATOR.getVideoReceiverControlSocket( _clientName );
    params.withSizeHeader = true;
    params.asyncClientModeRequests = true;
    params.asyncServerMode = false;

    return CommunicationGatewayFacade::getNewShellConnection( params );
}










