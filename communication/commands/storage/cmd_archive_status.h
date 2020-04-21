#ifndef CMD_ARCHIVE_STATUS_H
#define CMD_ARCHIVE_STATUS_H

#include <microservice_common/communication/i_command_external.h>

#include "common/common_types.h"

class CommandArchiveStatus : public ICommandExternal
{
    static const std::string ALL_ARCHIVINGS;
public:
    CommandArchiveStatus( common_types::SIncomingCommandServices * _services );

    virtual bool exec() override;

    common_types::TArchivingId m_archivingId;
};
using PCommandArchiveStatus = std::shared_ptr<CommandArchiveStatus>;

#endif // CMD_ARCHIVE_STATUS_H
