
#include "storage/storage_engine_facade.h"
#include "system/system_environment_facade_vs.h"
#include "cmd_context_open.h"

using namespace std;
using namespace common_types;

CommandContextOpen::CommandContextOpen( SIncomingCommandServices * _services ) :
    ICommandExternal(_services)
{

}

#include <jsoncpp/json/writer.h>

bool CommandContextOpen::exec(){

    if( ! ((SIncomingCommandServices *)m_services)->systemEnvironment->serviceForContextControl()->openContext( m_contextId ) ){
        string response = "context opening error: " + ((SIncomingCommandServices *)m_services)->systemEnvironment->getState().lastError;

        // TODO: remove after protocol refactor (response/body)
        Json::FastWriter writer;
        Json::Value root;
        root["response"] = "fail";
        root["body"] = response;
        //

        sendResponse( writer.write(root) );
        return false;
    }

    // TODO: remove after protocol refactor (response/body)
    Json::FastWriter writer;
    Json::Value root;
    root["response"] = "success";
    root["body"] = "";
    //

    sendResponse( writer.write(root) );
    return true;
}
