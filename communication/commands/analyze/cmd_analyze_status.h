#ifndef CMD_ANALYZE_STATUS_H
#define CMD_ANALYZE_STATUS_H

#include <microservice_common/communication/i_command_external.h>

#include "common/common_types.h"

class CommandAnalyzeStatus : public ICommandExternal
{
    static constexpr uint64_t ALL_SENSORS = 0;
    static const std::string ALL_PROCESSING;
public:
    CommandAnalyzeStatus( common_types::SIncomingCommandServices * _services );


    virtual bool exec() override;

    common_types::TSensorId m_sensorId;
    common_types::TProcessingId m_processingId;

private:

};
using PCommandAnalyzeStatus = std::shared_ptr<CommandAnalyzeStatus>;

#endif // CMD_ANALYZE_STATUS_H
