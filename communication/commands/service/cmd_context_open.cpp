
#include "cmd_context_open.h"
#include "storage/storage_engine.h"
#include "video_server_common/system/system_environment.h"

using namespace std;
using namespace common_types;

CommandContextOpen::CommandContextOpen( SIncomingCommandServices * _services ) :
    ICommandExternal(_services)
{

}

bool CommandContextOpen::exec(){

    if( ! m_services->systemEnvironment->openContext( m_contextId ) ){
        string response = "context opening error: " + m_services->systemEnvironment->getLastError();

        sendResponse( response, false );
        return false;
    }

    sendResponse( Json::Value(), true );
    return true;
}
