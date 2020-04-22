#ifndef CMD_PING_H
#define CMD_PING_H

#include <microservice_common/communication/i_command_external.h>

#include "common/common_types.h"

class CommandPing : public ICommandExternal
{
public:
    CommandPing( common_types::SIncomingCommandServices * _services );

    virtual bool exec() override;

    void clear(){

    }


private:


};
using PCommandPing = std::shared_ptr<CommandPing>;

#endif // CMD_PING_H
