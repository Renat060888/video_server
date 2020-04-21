#ifndef CMD_ANALYZE_STOP_H
#define CMD_ANALYZE_STOP_H

#include <microservice_common/communication/i_command_external.h>

#include "common/common_types.h"

class CommandAnalyzeStop : public ICommandExternal
{
public:
    CommandAnalyzeStop( common_types::SIncomingCommandServices * _services );

    virtual bool exec() override;

    common_types::TProcessingId m_processingId;
    uint64_t m_sensorId;
    bool destroy;
};
using PCommandAnalyzeStop = std::shared_ptr<CommandAnalyzeStop>;

#endif // CMD_ANALYZE_STOP_H
