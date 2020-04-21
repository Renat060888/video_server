#ifndef CMD_GET_SOURCES_H
#define CMD_GET_SOURCES_H

#include <microservice_common/communication/i_command_external.h>

#include "common/common_types.h"

class CommandSourcesGet : public ICommandExternal
{
public:
    CommandSourcesGet( common_types::SIncomingCommandServices * _services );
    ~CommandSourcesGet();

    virtual bool exec() override;
};
using PCommandGetSources = std::shared_ptr<CommandSourcesGet>;

#endif // CMD_GET_SOURCES_H
