#ifndef ASYNC_NOTIFIER_H
#define ASYNC_NOTIFIER_H

#include "visitor_notify_event.h"
#include "common/common_types.h"

class AsyncNotifier : public common_types::IEventNotifier
{
public:
    struct SInitSettings {
        PNetworkClient networkClient;
    };

    AsyncNotifier();

    bool init( SInitSettings _settings );

    virtual void sendEvent( const common_types::SEvent & _event ) override;
    void sendAnotherSignal();

private:
    // data
    SInitSettings m_settings;

    // service
    EventSendVisitor m_eventSendVisitor;


};

#endif // ASYNC_NOTIFIER_H
