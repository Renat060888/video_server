#ifndef CMD_STATUS_H
#define CMD_STATUS_H

#include <microservice_common/communication/i_command_external.h>

#include "common/common_types.h"

class CommandOutgoingAnalyzeEvent : public ICommandExternal
{
public:
    CommandOutgoingAnalyzeEvent( common_types::SIncomingCommandServices * _services );
    ~CommandOutgoingAnalyzeEvent();

    virtual bool exec() override;

    void clear(){
        m_outcomingMsg.clear();
    }

    std::string m_outcomingMsg;

private:
};
using PCommandOutgoingAnalyzeEvent = std::shared_ptr<CommandOutgoingAnalyzeEvent>;

#endif // CMD_STATUS_H
