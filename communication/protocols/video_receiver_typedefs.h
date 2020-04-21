#ifndef VIDEO_RECEIVER_TYPEDEFS_H
#define VIDEO_RECEIVER_TYPEDEFS_H

#include <string>
#include <vector>

#include <boost/variant.hpp>

//----------- COMMANDS ----------------------

enum class CommandType
{
    CT_Undef = -1,
    CT_PLAY = 0,
    CT_PAUSE,
    CT_STOP,
    CT_SEEK,
    CT_NEXT_FRAME,
    CT_SET_VALUES,
    CT_SAVE_VIDEO,
    CT_ADD_FILTER,
    CT_DEL_FILTER,
    CT_ADD_ANALYSER,
    CT_DEL_ANALYSER,
    CT_CHANGE_URL,
    CT_GET_STREAM_INFO,
    CT_GET_PLUGINS_INFO,
    CT_QUICK_FILTER,
    CT_SET_RATE,
    CT_GET_ARCHIVE_INFO
};

static std::vector< std::string > getNames()
{
    std::vector< std::string > names;
    names = { "play",
              "pause",
              "stop",
              "seek",
              "next_frame",
              "set_values",
              "save_video",
              "add_filer",
              "del_filter",
              "add_analyser",
              "del_analyser",
              "change_url",
              "get_stream_info",
              "get_plugins_info",
              "set_quick_filter",
              "set_rate",
              "get_archive_info"
            };

    return names;
}

static std::string commandTypeToString( CommandType type )
{
    auto names = getNames();
    if( names.size() > (uint) type )
        return names[ (uint) type ];
    return "undefined";
}

static CommandType commandTypeByString( std::string type )
{
    auto names = getNames();
    for( uint i = 0; i < names.size(); ++i )
        if( names[ i ] == type )
            return (CommandType) i;

    return CommandType::CT_Undef;
}

//-------------- ANSWERS -------------------

enum class AnswerType
{
    AT_Undef = -1,
    AT_DURATION,
    AT_RECORD,
    AT_RESPONSE,
    AT_EVENT,
    AT_INFO,
    AT_ANALYTIC,
    AT_STREAMDATA,
    AT_PLUGINSDATA,
    AT_ARCHIVEDATA,
};

static const std::vector<std::string> & getAnswerNames()
{
    static const std::vector<std::string> names = {"duration",
                                                   "record",
                                                   "response",
                                                   "event",
                                                   "info",
                                                   "analytic",
                                                   "streamdata",                                                   
                                                   "plugindata",
                                                   "archivedata"};
    return names;
}

static std::string answerTypeToString( AnswerType type )
{
    auto names = getAnswerNames();
    if( names.size() > (uint) type )
        return names[ (uint) type ];
    return "undefine";
}

static AnswerType answerTypeByString( std::string type )
{
    auto names = getAnswerNames();
    for( uint i = 0; i < names.size(); ++i )
        if( names[ i ] == type )
            return (AnswerType) i;

    return AnswerType::AT_Undef;
}

//------------- EVENTS & MESSAGES --------------------

enum class EventType
{
    ET_UNDEF = -1,
    ET_STREAM_START,
    ET_STREAM_STOP
};

static std::vector<std::string> getEventNames()
{
    std::vector<std::string> names;
    names = {"stream-start", "stream-stop"};

    return names;
}

static std::string eventTypeToString(EventType type)
{
    auto names = getEventNames();
    if (names.size() > (uint)type)
        return names[(uint) type];
    return "undefined";
}

static EventType eventTypeByString(std::string type)
{
    auto names = getEventNames();
    for(uint i = 0; i < names.size(); ++i)
        if (names[i] == type)
            return (EventType)i;

    return EventType::ET_UNDEF;
}

enum class Severity
{
    SV_UNDEF = -1,
    SV_DEBUG,
    SV_INFO,
    SV_WARNING,
    SV_ERROR,
    SV_CRIT
};

static std::vector<std::string> getSeverityNames()
{
    std::vector<std::string> names;
    names = {"debug", "info", "warning", "error", "crit"};

    return names;
}

static std::string severityToString(Severity type)
{
    auto names = getSeverityNames();
    if (names.size() > (uint)type)
        return names[(uint) type];
    return "undefined";
}

static Severity severityByString(std::string type)
{
    auto names = getSeverityNames();
    for(uint i = 0; i < names.size(); ++i)
        if (names[i] == type)
            return (Severity)i;

    return Severity::SV_UNDEF;
}

//------------- STRUCTS --------------------

enum CommandOptionType
{
    COT_STRING = 0,
    COT_INT,
    COT_DOUBLE
};
typedef boost::variant< std::string, int, double > CommandOptionValue;

struct CommandOption
{
    std::string name = "";
    CommandOptionType type;
    CommandOptionValue value;
};
typedef std::vector< CommandOption > CommandOptions;

struct FrameOptions
{
    int width;
    int height;
    double fps;
    std::string format;

    int current_frame;
    int total_frame;

    uint64_t cur_timestamp;
    uint64_t total_timestamp;
};

struct ModuleDesc
{
    std::string code = "";
    std::string name = "";
};

struct RecordData
{
    std::string recordPath;
    std::string previewPath;
    int64_t startTime = 0;
    int64_t duration = 0;
    std::string tag;
};

struct ArchiveSession
{
    int64_t timeBeginMillisec = 0;
    std::string sessionName;
    std::string playlistName;
};

struct EventData
{
    EventType type;
    std::string description;
    FrameOptions options;
    ArchiveSession archiveSession;
};

struct InfoData
{
    Severity level;
    std::string text;
};

struct AnalyticData
{
    std::string source;
    std::string data;
};


#endif // VIDEO_RECEIVER_TYPEDEFS_H
