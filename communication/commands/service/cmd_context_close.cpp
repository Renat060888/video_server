
#include "cmd_context_close.h"
#include "storage/storage_engine_facade.h"
#include "system/system_environment_facade_vs.h"

using namespace std;
using namespace common_types;

CommandContextClose::CommandContextClose( SIncomingCommandServices * _services ) :
    ICommandExternal(_services)
{

}

bool CommandContextClose::exec(){

    if( ! ((SIncomingCommandServices *)m_services)->systemEnvironment->serviceForContextControl()->closeContext() ){
        string response = "context closing error: " + ((SIncomingCommandServices *)m_services)->systemEnvironment->getState().lastError;

        sendResponse( response );
        return false;
    }

    sendResponse( string() );
    return true;
}
