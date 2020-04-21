
#include "cmd_context_close.h"
#include "storage/storage_engine.h"
#include "video_server_common/system/system_environment.h"

using namespace std;
using namespace common_types;

CommandContextClose::CommandContextClose( SIncomingCommandServices * _services ) :
    ICommandExternal(_services)
{

}

bool CommandContextClose::exec(){

    if( ! m_services->systemEnvironment->closeContext() ){
        string response = "context closing error: " + m_services->systemEnvironment->getLastError();

        sendResponse( response, false );
        return false;
    }

    sendResponse( Json::Value(), true );
    return true;
}
