#ifndef COMMON_VARS_H
#define COMMON_VARS_H

#include <microservice_common/common/ms_common_vars.h>

namespace common_vars {

namespace cmd {

    // source
    const std::string COMMAND_TYPE_SOURCE = "source";
    const std::string CMD_NAME_CONNECT = "connect";
    const std::string CMD_NAME_DISCONNECT = "disconnect";
    const std::string CMD_NAME_GET = "get";

    // service
    const std::string CMD_NAME_PING = "ping";
    const std::string SHUTDOWN_SERVER = "shutdown";
    const std::string CMD_NAME_STATUS = "status";
    const std::string CMD_NAME_CTX_OPEN = "context_open";
    const std::string CMD_NAME_CTX_CLOSE = "context_close";

    // analyze
    const std::string COMMAND_TYPE_ANALYZE = "analyze";
    const std::string CMD_NAME_ANALYZE_START = "start";
    const std::string CMD_NAME_ANALYZE_STOP = "stop";
    const std::string CMD_NAME_ANALYZE_STATUS = "analyze_status";
    const std::string CMD_NAME_PLAY_START = "play_start";
    const std::string CMD_NAME_PLAY_PAUSE = "play_pause";
    const std::string CMD_NAME_PLAY_STOP = "play_stop";
    const std::string CMD_NAME_PLAY_FROM_POS = "play_from_pos";
    const std::string CMD_NAME_PLAY_LOOP = "play_loop";
    const std::string CMD_NAME_PLAY_LIVE = "play_live";
    const std::string CMD_NAME_PLAY_REVERSE = "play_reverse";
    const std::string CMD_NAME_PLAY_WITH_SPEED = "play_with_speed";
    const std::string CMD_NAME_PLAY_STEP = "play_step";

    // storage
    const std::string COMMAND_TYPE_STORAGE = "storage";
    const std::string CMD_NAME_ARCHIVE_START = "start";
    const std::string CMD_NAME_ARCHIVE_STOP = "stop";
    const std::string CMD_NAME_ARCHIVE_STATUS = "status";
    const std::string CMD_NAME_ARCHIVE_INFO = "archive_info";
    const std::string CMD_NAME_DOWNLOAD = "download";
    const std::string CMD_NAME_BOOKMARK = "bookmark";




}

namespace mongo_fields {

namespace service {
    const std::string COLLECTION_NAME = "service";
    const std::string LAST_LOADED_CONTEXT_ID = "last_loaded_context_id";
}

namespace analytic {
namespace metadata {
    const std::string COLLECTION_NAME = "analytic_metadata";
    const std::string CTX_ID = "ctx_id";
    const std::string SENSOR_ID = "sensor_id";
    const std::string LAST_SESSION_ID = "last_session_id";
    const std::string UPDATE_STEP_MILLISEC = "update_step_millisec";
}
}

namespace archiving {
    const std::string COLLECTION_NAME = "hls_metadata";
    const std::string TAG = "tag";
    const std::string START_TIME = "time_start";
    const std::string END_TIME = "time_end";
    const std::string SOURCE = "source";
    const std::string PLAYLIST_PATH = "playlist_path";
}

}


}

#endif // COMMON_VARS_H
