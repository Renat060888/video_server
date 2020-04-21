#ifndef COMMUNICATION_GATEWAY_FACADE_VS_H
#define COMMUNICATION_GATEWAY_FACADE_VS_H

#include <microservice_common/communication/communication_gateway_facade.h>

#include "common/common_types.h"

class CommunicationGatewayFacadeVS : public CommunicationGatewayFacade
                                   , public common_types::IInternalCommunication
                                   , public common_types::IExternalCommunication
{
public:
    struct SInitSettings : CommunicationGatewayFacade::SInitSettings {
        common_types::SIncomingCommandServices services;
    };

    CommunicationGatewayFacadeVS();
    ~CommunicationGatewayFacadeVS();

    bool init( const SInitSettings & _settings );

    // outcoming commands ( for internal sub-systems )
    common_types::IExternalCommunication * serviceForExternalCommunication();
    common_types::IInternalCommunication * serviceForInternalCommunication();

    // outcoming commands ( for external clients )
    common_types::IEventNotifier * getServiceOfEventNotifier();

private:
    // internal communications
    virtual PNetworkClient getAnalyticCommunicator( std::string _clientName ) override;
    virtual PNetworkClient getSourceCommunicator( std::string _clientName ) override;
    virtual PNetworkClient getArchiveCommunicator( std::string _clientName ) override;
    virtual PNetworkClient getServiceCommunicator() override;

    virtual PNetworkClient getEventRetranslateCommunicator( std::string _clientName ) override;
    virtual PNetworkClient getPlayerCommunicator( std::string _clientName ) override;

    // external communications ( async )
    PNetworkClient createCommunicatorWithRecevier( const std::string & _clientName );

    // data
    PNetworkClient m_asyncNotifierNetworkClient;
    SInitSettings m_settings;

    // service
    common_types::IEventNotifier * m_asyncNotifier;

};

#endif // COMMUNICATION_GATEWAY_FACADE_VS_H
