#ifndef COMMAND_SHUTDOWN_SERVER_H
#define COMMAND_SHUTDOWN_SERVER_H

#include <microservice_common/communication/i_command_external.h>

#include "common/common_types.h"

class CommandShutdownServer : public ICommandExternal
{
public:
    CommandShutdownServer( common_types::SIncomingCommandServices * _services );
    ~CommandShutdownServer(){}

    virtual bool exec() override;
};
using PCommandShutdownServer = std::shared_ptr<CommandShutdownServer>;

#endif // COMMAND_SHUTDOWN_SERVER_H
