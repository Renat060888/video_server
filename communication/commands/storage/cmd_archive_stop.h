#ifndef CMD_ARCHIVE_STOP_H
#define CMD_ARCHIVE_STOP_H

#include <microservice_common/communication/i_command_external.h>

#include "common/common_types.h"

class CommandArchiveStop : public ICommandExternal
{
public:
    CommandArchiveStop( common_types::SIncomingCommandServices * _services );

    virtual bool exec() override;

    common_types::TArchivingId m_archivingId;
    bool m_destroy;
};
using PCommandArchiveStop = std::shared_ptr<CommandArchiveStop>;

#endif // CMD_ARCHIVE_STOP_H
