#ifndef CMD_DOWNLOAD_VIDEO_H
#define CMD_DOWNLOAD_VIDEO_H

#include <microservice_common/communication/i_command_external.h>

#include "common/common_types.h"

class CommandDownloadVideo : public ICommandExternal
{
public:
    CommandDownloadVideo( common_types::SIncomingCommandServices * _services );

    virtual bool exec() override;

    int32_t m_archiveId;
    common_types::TTimeRangeMillisec m_timeRangeSec;
};
using PCommandDownloadVideo = std::shared_ptr<CommandDownloadVideo>;

#endif // CMD_DOWNLOAD_VIDEO_H
