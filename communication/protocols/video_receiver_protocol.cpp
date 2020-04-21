
#include <iostream>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "video_receiver_protocol.h"

using namespace std;

#define CHECK_JSON_STRING( doc, json, def_val ) \
doc.Parse( json.c_str() ); \
if( doc.HasParseError() ) \
{ \
    std::cerr << "Parce JSON error: " << doc.GetParseError() \
              << ", [" << json << "]" << std::endl; \
    return def_val; \
} \
if( !doc.IsObject() ) \
{ \
    std::cerr << "No JSON object" << std::endl; \
    return def_val; \
}

#define GET_UINT_64( doc, name, value ) \
    if( doc.HasMember( name )) \
    { \
        uint64_t dur = doc[ name ].GetUint64(); \
        value = dur; \
    }

#define GET_INT( doc, name, value ) \
    if( doc.HasMember( name )) \
    { \
        int dur = doc[ name ].GetInt(); \
        value = dur; \
    }

#define GET_DOUBLE( doc, name, value ) \
    if( doc.HasMember( name )) \
    { \
        double dur = doc[ name ].GetDouble(); \
        value = dur; \
    }

#define GET_STRING( doc, name, value ) \
    if( doc.HasMember( name )) \
    { \
        std::string dur = doc[ name ].GetString(); \
        value = dur; \
    }

VideoReceiverProtocol::VideoReceiverProtocol()
{

}

std::string VideoReceiverProtocol::createSimpleCommand(CommandType type)
{
    using namespace rapidjson;

    // make document
    Document doc;
    doc.SetObject();
    Document::AllocatorType& alloc = doc.GetAllocator();

    std::string name = commandTypeToString( type );

    Value cmnd;
    cmnd.SetObject();
    cmnd.SetString( name.data(), name.size() );

    doc.AddMember( "command", cmnd, alloc );

    // create string
    StringBuffer buffer;
    Writer< StringBuffer > writer( buffer );
    doc.Accept( writer );

    std::string res = buffer.GetString();
    return res;
}

std::string VideoReceiverProtocol::createOptionsCommand( std::string name, CommandOptions options )
{
    using namespace rapidjson;

    // make document
    Document doc;
    doc.SetObject();
    Document::AllocatorType& alloc = doc.GetAllocator();

    Value modul;
    modul.SetString( name.data(), name.size(), alloc );
    doc.AddMember( "module", modul, alloc );

    Value options_json( kArrayType );
    for( CommandOption option: options )
    {
        Value element;
        element.SetObject();

        Value name_el;
        name_el.SetString( option.name.data(), option.name.size(), alloc );
        element.AddMember( "name", name_el, alloc );

        Value value;
        if( option.type == COT_STRING )
        {
            std::string str = boost::get< std::string >( option.value );
            value.SetString( str.data(), str.size(), alloc );
        }
        else if( option.type == COT_INT )
            value.SetInt( boost::get< int >( option.value ));
        else if( option.type == COT_DOUBLE )
            value.SetDouble( boost::get< double >( option.value ));
        element.AddMember( "value", value, alloc );

        options_json.PushBack( element, alloc );
    }
    doc.AddMember( "options", options_json, alloc );

    std::string cmnd_name = commandTypeToString( CommandType::CT_SET_VALUES );
    Value cmnd;
    cmnd.SetObject();
    cmnd.SetString( cmnd_name.data(), cmnd_name.size() );

    doc.AddMember( "command", cmnd, alloc );

    // create string
    StringBuffer buffer;
    Writer< StringBuffer > writer( buffer );
    doc.Accept( writer );

    std::string res = buffer.GetString();
    return res;
}

std::string VideoReceiverProtocol::createSeekCommand(uint64_t msec_time)
{
    using namespace rapidjson;

    // make document
    Document doc;
    doc.SetObject();
    Document::AllocatorType& alloc = doc.GetAllocator();

    std::string name = commandTypeToString( CommandType::CT_SEEK );

    Value cmnd;
    cmnd.SetObject();
    cmnd.SetString( name.data(), name.size() );

    doc.AddMember( "command", cmnd, alloc );

    Value time;
    time.SetUint64( msec_time );
    doc.AddMember( "msec_time", time, alloc );

    // create string
    StringBuffer buffer;
    Writer< StringBuffer > writer( buffer );
    doc.Accept( writer );

    std::string res = buffer.GetString();
    return res;
}

AnswerType VideoReceiverProtocol::getAnswerType( const std::string & json )
{
    using namespace std;
    using namespace rapidjson;

    Document doc;
    CHECK_JSON_STRING( doc, json, AnswerType::AT_Undef )

            if( !doc.HasMember( "answer" ))
    {
        std::cerr << "No answer in JSON" << std::endl;
        return AnswerType::AT_Undef;
    }
    std::string ans = doc[ "answer" ].GetString();
    AnswerType type = answerTypeByString( ans );

    return type;
}

FrameOptions VideoReceiverProtocol::getFrameOptions(std::string json)
{
    using namespace std;
    using namespace rapidjson;

    Document doc;
    CHECK_JSON_STRING( doc, json, FrameOptions() )

            // get options
            FrameOptions opt;

    GET_UINT_64( doc, "position", opt.cur_timestamp )
            GET_UINT_64( doc, "duration", opt.total_timestamp )
            GET_INT( doc, "width", opt.width )
            GET_INT( doc, "height", opt.height )
            GET_INT( doc, "current_frame", opt.current_frame )
            GET_INT( doc, "total_frame", opt.total_frame )
            GET_DOUBLE( doc, "fps", opt.fps )
            GET_STRING( doc, "format", opt.format )

            return opt;
}

std::string VideoReceiverProtocol::createRecordCommand(const std::string &path, uint32_t backward, uint32_t forward, std::string tag )
{
    using namespace rapidjson;

    // make document
    Document doc;
    doc.SetObject();
    Document::AllocatorType& alloc = doc.GetAllocator();

    Value val;
    val.SetObject();
    val.SetUint( backward );
    doc.AddMember( "backward", val, alloc );

    val.SetObject();
    val.SetUint( forward );
    doc.AddMember( "forward", val, alloc );

    val.SetObject();
    val.SetString( path.c_str(), path.size() );
    doc.AddMember( "path", val, alloc );

    if (!tag.empty()) {
        val.SetObject();
        val.SetString( tag.c_str(), tag.size() );
        doc.AddMember( "tag", val, alloc );
    }

    std::string name = commandTypeToString( CommandType::CT_SAVE_VIDEO );
    Value cmnd;
    cmnd.SetObject();
    cmnd.SetString( name.data(), name.size() );

    doc.AddMember( "command", cmnd, alloc );

    // create string
    StringBuffer buffer;
    Writer< StringBuffer > writer( buffer );
    doc.Accept( writer );

    std::string res = buffer.GetString();
    return res;
}

std::string VideoReceiverProtocol::createAddFilterCommand( const std::string &code, const std::string &name )
{
    using namespace rapidjson;

    // make document
    Document doc;
    doc.SetObject();
    Document::AllocatorType& alloc = doc.GetAllocator();

    Value val;

    val.SetObject();
    val.SetString( code.c_str(), code.size() );
    doc.AddMember( "code", val, alloc );

    val.SetObject();
    val.SetString( name.c_str(), name.size() );
    doc.AddMember( "name", val, alloc );

    std::string cname = commandTypeToString( CommandType::CT_ADD_FILTER );
    Value cmnd;
    cmnd.SetObject();
    cmnd.SetString( cname.data(), cname.size() );
    doc.AddMember( "command", cmnd, alloc );

    // create string
    StringBuffer buffer;
    Writer< StringBuffer > writer( buffer );
    doc.Accept( writer );

    std::string res = buffer.GetString();
    return res;
}

std::string VideoReceiverProtocol::createDelFilterCommand( const std::string &name )
{
    using namespace rapidjson;

    // make document
    Document doc;
    doc.SetObject();
    Document::AllocatorType& alloc = doc.GetAllocator();

    Value val;

    val.SetObject();
    val.SetString( name.c_str(), name.size() );
    doc.AddMember( "name", val, alloc );

    std::string cname = commandTypeToString( CommandType::CT_DEL_FILTER );
    Value cmnd;
    cmnd.SetObject();
    cmnd.SetString( cname.data(), cname.size() );
    doc.AddMember( "command", cmnd, alloc );

    // create string
    StringBuffer buffer;
    Writer< StringBuffer > writer( buffer );
    doc.Accept( writer );

    std::string res = buffer.GetString();
    return res;
}

std::string VideoReceiverProtocol::createAddAnalyserCommand( const std::string &code, const std::string &name )
{
    using namespace rapidjson;

    // make document
    Document doc;
    doc.SetObject();
    Document::AllocatorType& alloc = doc.GetAllocator();

    Value val;

    val.SetObject();
    val.SetString( code.c_str(), code.size() );
    doc.AddMember( "code", val, alloc );

    val.SetObject();
    val.SetString( name.c_str(), name.size() );
    doc.AddMember( "name", val, alloc );

    /*if( options.size() )
    {
        Value options_json( kArrayType );
        for( CommandOption option: options )
        {
            Value element;
            element.SetObject();

            Value name_el;
            name_el.SetString( option.name.data(), option.name.size(), alloc );
            element.AddMember( "name", name_el, alloc );

            Value value;
            value.SetString( option.value.data(), option.value.size(), alloc );
            element.AddMember( "value", value, alloc );

            options_json.PushBack( element, alloc );
        }

        doc.AddMember( "options", options_json, alloc );
    }*/

    std::string cname = commandTypeToString( CommandType::CT_ADD_ANALYSER );
    Value cmnd;
    cmnd.SetObject();
    cmnd.SetString( cname.data(), cname.size() );
    doc.AddMember( "command", cmnd, alloc );

    // create string
    StringBuffer buffer;
    Writer< StringBuffer > writer( buffer );
    doc.Accept( writer );

    std::string res = buffer.GetString();
    return res;
}

std::string VideoReceiverProtocol::createDelAnalyserCommand( const std::string &name )
{
    using namespace rapidjson;

    // make document
    Document doc;
    doc.SetObject();
    Document::AllocatorType& alloc = doc.GetAllocator();

    Value val;

    val.SetObject();
    val.SetString( name.c_str(), name.size() );
    doc.AddMember( "name", val, alloc );

    std::string cname = commandTypeToString( CommandType::CT_DEL_ANALYSER );
    Value cmnd;
    cmnd.SetObject();
    cmnd.SetString( cname.data(), cname.size() );
    doc.AddMember( "command", cmnd, alloc );

    // create string
    StringBuffer buffer;
    Writer< StringBuffer > writer( buffer );
    doc.Accept( writer );

    std::string res = buffer.GetString();
    return res;
}

RecordData VideoReceiverProtocol::getRecordData( std::string json )
{
    using namespace rapidjson;

    Document doc;
    CHECK_JSON_STRING( doc, json, RecordData() )

            RecordData ret;
    if( doc.HasMember( "path" ))
    {
        ret.recordPath = doc[ "path" ].GetString();
    }
    if( doc.HasMember( "preview_path" ))
    {
        ret.previewPath = doc[ "preview_path" ].GetString();
    }
    if( doc.HasMember( "tag" ))
    {
        ret.tag = doc[ "tag" ].GetString();
    }
    if( doc.HasMember( "start_time" ))
    {
        ret.startTime = doc[ "start_time" ].GetInt64();
    } else {
        ret.startTime = 0;
    }
    if( doc.HasMember( "duration" ))
    {
        ret.duration = doc[ "duration" ].GetInt64();
    } else {
        ret.duration = 0;
    }
    return ret;
}

EventData VideoReceiverProtocol::getEventData( std::string json )
{
    using namespace rapidjson;

    Document doc;
    CHECK_JSON_STRING( doc, json, EventData() )

    EventData ret;
    if( doc.HasMember( "type" ))
    {
        ret.type = eventTypeByString(doc[ "type" ].GetString());
    }
    if( doc.HasMember( "description" ))
    {
        ret.description = doc[ "description" ].GetString();
    }
    if( doc.HasMember( "archive_session" ))
    {
        ret.archiveSession.timeBeginMillisec = doc[ "archive_session" ][ "time_begin_millisec" ].GetInt64();
        ret.archiveSession.sessionName = doc[ "archive_session" ][ "session_name" ].GetString();
        ret.archiveSession.playlistName = doc[ "archive_session" ][ "playlist_name" ].GetString();
    }
    if( doc.HasMember( "width" ))
    {
        ret.options.width = doc[ "width" ].GetInt();
    }
    if( doc.HasMember( "height" ))
    {
        ret.options.height = doc[ "height" ].GetInt();
    }

    return ret;
}

ArchiveSession VideoReceiverProtocol::getArchiveData( std::string json ){

    using namespace rapidjson;

    Document doc;
    CHECK_JSON_STRING( doc, json, ArchiveSession() )

    ArchiveSession out;
    if( doc.HasMember( "archive_session" ))
    {
        out.timeBeginMillisec = doc[ "archive_session" ][ "time_begin_millisec" ].GetInt64();
        out.sessionName = doc[ "archive_session" ][ "session_name" ].GetString();
        out.playlistName = doc[ "archive_session" ][ "playlist_name" ].GetString();
    }

    return out;
}

InfoData VideoReceiverProtocol::getInfoData( std::string json )
{
    using namespace rapidjson;

    Document doc;
    CHECK_JSON_STRING( doc, json, InfoData() )

    InfoData ret;
    if( doc.HasMember( "severity" ))
    {
        ret.level = severityByString(doc[ "severity" ].GetString());
    }
    if( doc.HasMember( "text" ))
    {
        ret.text = doc[ "text" ].GetString();
    }

    return ret;
}

AnalyticData VideoReceiverProtocol::getAnalyticData( std::string json )
{
    using namespace rapidjson;

    //std::cout << "JSON: " << json << std::endl;

    Document doc;
    CHECK_JSON_STRING( doc, json, AnalyticData() )

    AnalyticData ret;
    if( doc.HasMember( "source" ))
    {
        ret.source = doc[ "source" ].GetString();
    }
    if( doc.HasMember( "body" ))
    {
        // create string
        StringBuffer buffer;
        Writer< StringBuffer > writer( buffer );
        doc[ "body" ].Accept( writer );

        ret.data = buffer.GetString();
    }

    return ret;
}
