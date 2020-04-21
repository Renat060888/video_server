#ifndef CMD_VIDEO_BOOKMARK_H
#define CMD_VIDEO_BOOKMARK_H

#include <microservice_common/communication/i_command_external.h>

#include "common/common_types.h"

class CommandVideoBookmark : public ICommandExternal
{
public:
    CommandVideoBookmark( common_types::SIncomingCommandServices * _services );

    virtual bool exec() override;

    bool m_write;
    common_types::SBookmark m_bookmark;
};
using PCommandVideoBookmark = std::shared_ptr<CommandVideoBookmark>;

#endif // CMD_VIDEO_BOOKMARK_H
