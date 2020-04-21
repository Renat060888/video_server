#ifndef CMD_DISCONNECT_SOURCE_H
#define CMD_DISCONNECT_SOURCE_H

#include <microservice_common/communication/i_command_external.h>

#include "common/common_types.h"

class CommandSourceStopRetranslate : public ICommandExternal
{
public:
    CommandSourceStopRetranslate( common_types::SIncomingCommandServices * _services );
    ~CommandSourceStopRetranslate();

    virtual bool exec() override;

    common_types::SVideoSource m_sourceToDisconnect;
};
using PCommandSourceStopRetranslate = std::shared_ptr<CommandSourceStopRetranslate>;

#endif // CMD_DISCONNECT_SOURCE_H
