#ifndef CMD_CONTEXT_CLOSE_H
#define CMD_CONTEXT_CLOSE_H

#include <microservice_common/communication/i_command_external.h>

#include "common/common_types.h"

class CommandContextClose : public ICommandExternal
{
public:
    CommandContextClose( common_types::SIncomingCommandServices * _services );

    virtual bool exec() override;
};
using PCommandContextClose = std::shared_ptr<CommandContextClose>;

#endif // CMD_CONTEXT_CLOSE_H
