#ifndef COMMAND_CONNECT_SOURCE_H
#define COMMAND_CONNECT_SOURCE_H

#include <microservice_common/communication/i_command_external.h>

#include "common/common_types.h"

class CommandSourceStartRetranslate : public ICommandExternal
{
public:
    CommandSourceStartRetranslate( common_types::SIncomingCommandServices * _services );
    ~CommandSourceStartRetranslate(){}

    virtual bool exec() override;

    common_types::SVideoSource m_sourceToConnect;
    std::string m_requestFullText;
};
using PCommandSourceStartRetranslate = std::shared_ptr<CommandSourceStartRetranslate>;

#endif // COMMAND_CONNECT_SOURCE_H
