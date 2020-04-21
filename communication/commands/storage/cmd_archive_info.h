#ifndef CMD_ARCHIVE_INFO_H
#define CMD_ARCHIVE_INFO_H

#include <microservice_common/communication/i_command_external.h>

#include "common/common_types.h"

class CommandArchiveInfo : public ICommandExternal
{
public:
    CommandArchiveInfo( common_types::SIncomingCommandServices * _services );

    virtual bool exec() override;
};
using PCommandArchiveInfo = std::shared_ptr<CommandArchiveInfo>;

#endif // CMD_ARCHIVE_INFO_H
