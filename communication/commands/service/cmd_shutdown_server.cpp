
#include "cmd_shutdown_server.h"

using namespace std;
using namespace common_types;

CommandShutdownServer::CommandShutdownServer( SIncomingCommandServices * _services ) :
    ICommandExternal( _services )
{

}

#include <jsoncpp/json/writer.h>

bool CommandShutdownServer::exec(){

    Json::Value root;
    root["message"] = "I'am shutdown, bye bye!";

    // TODO: remove after protocol refactor (response/body)
    Json::FastWriter writer;
    Json::Value root2;
    root2["response"] = "success";
    root2["body"] = root;
    //

    sendResponse( writer.write(root2) );

    ((SIncomingCommandServices *)m_services)->signalShutdownServer();
    return true;
}
