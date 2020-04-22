
#include "datasource/source_manager_facade.h"
#include "cmd_outgoing_analyze_event.h"

using namespace std;
using namespace common_types;

CommandOutgoingAnalyzeEvent::CommandOutgoingAnalyzeEvent( SIncomingCommandServices * _services ) :
    ICommandExternal(_services){

    m_request = _services->networkClient->getRequestInstance();
}

CommandOutgoingAnalyzeEvent::~CommandOutgoingAnalyzeEvent(){

}

bool CommandOutgoingAnalyzeEvent::exec(){



    m_request->setOutcomingMessage( m_outcomingMsg );
    return true;
}
