#ifndef CMD_ANALYZE_START_H
#define CMD_ANALYZE_START_H

#include <map>

#include <microservice_common/communication/i_command_external.h>

#include "common/common_types.h"

class CommandAnalyzeStart : public ICommandExternal
{
public:
    CommandAnalyzeStart( common_types::SIncomingCommandServices * _services );

    virtual bool exec() override;

    uint64_t m_sensorId;
    uint64_t m_profileId;
    std::string m_processingName;
    int32_t zoneRect[ 4 ]; // x y w h
    bool resume;
    common_types::TProcessingId m_processingId;
    std::string m_requestFullText;

    // TODO: remove
    std::vector<uint64_t> m_allowedObjreprClassinfos;

    // TODO: may be to delete
    std::map<std::string, std::string> m_dpfLabelToObjreprClassinfo;

private:

};
using PCommandAnalyzeStart = std::shared_ptr<CommandAnalyzeStart>;

#endif // CMD_ANALYZE_START_H
