#ifndef CMD_START_RECORD_H
#define CMD_START_RECORD_H

#include <microservice_common/communication/i_command_external.h>

#include "common/common_types.h"

class CommandArchiveStart : public ICommandExternal
{
public:
    CommandArchiveStart( common_types::SIncomingCommandServices * _services );

    virtual bool exec() override;

    uint64_t m_sensorId;
    std::string m_archivingName;
    common_types::TArchivingId m_archivingId;
    std::string m_requestFullText;
};
using PCommandArchiveStart = std::shared_ptr<CommandArchiveStart>;

#endif // CMD_START_RECORD_H
