
#include "cmd_shutdown_server.h"

using namespace std;
using namespace common_types;

CommandShutdownServer::CommandShutdownServer( SIncomingCommandServices * _services ) :
    ICommandExternal( _services )
{

}

bool CommandShutdownServer::exec(){

    Json::Value root;
    root["message"] = "I'am shutdown, bye bye!";

    sendResponse( root, true );

    m_services->signalShutdownServer();
    return true;
}
