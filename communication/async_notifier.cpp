
#include "async_notifier.h"

using namespace std;
using namespace common_types;

AsyncNotifier::AsyncNotifier()
{

}

bool AsyncNotifier::init( SInitSettings _settings ){

    m_eventSendVisitor.m_services.networkClient = _settings.networkClient;



    return true;
}

void AsyncNotifier::sendEvent( const SEvent & _event ){

    _event.accept( & m_eventSendVisitor );

}

void AsyncNotifier::sendAnotherSignal(){

}
