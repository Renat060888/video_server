#ifndef COMMAND_FACTORY_H
#define COMMAND_FACTORY_H

#include <microservice_common/communication/i_command_factory.h>
#include <microservice_common/system/object_pool.h>

#include "common/common_types.h"
#include "commands/service/cmd_ping.h"

class CommandFactory : public ICommandFactory
{
public:
    CommandFactory( common_types::SIncomingCommandServices & _commandServices );

    virtual PCommand createCommand( PEnvironmentRequest _request ) override;


private:
    void sendFailToExternal( PEnvironmentRequest _request, const std::string _msg );

    // service
    common_types::SIncomingCommandServices & m_commandServices;
    ObjectPool<CommandPing , common_types::SIncomingCommandServices *> m_poolOfPings;
};
#endif // COMMAND_FACTORY_H
