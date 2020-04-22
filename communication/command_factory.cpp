
#include <boost/property_tree/json_parser.hpp>
#include <microservice_common/system/logger.h>

#include "commands/source/cmd_source_connect.h"
#include "commands/source/cmd_source_disconnect.h"
#include "commands/source/cmd_sources_get.h"
#include "commands/service/cmd_shutdown_server.h"
#include "commands/service/cmd_context_open.h"
#include "commands/service/cmd_context_close.h"
#include "commands/analyze/cmd_outgoing_analyze_event.h"
#include "commands/analyze/cmd_analyze_start.h"
#include "commands/analyze/cmd_analyze_stop.h"
#include "commands/analyze/cmd_analyze_status.h"
#include "commands/storage/cmd_archive_info.h"
#include "commands/storage/cmd_download_video.h"
#include "commands/storage/cmd_archive_start.h"
#include "commands/storage/cmd_archive_stop.h"
#include "commands/storage/cmd_archive_status.h"
#include "commands/storage/cmd_video_bookmark.h"
#include "common/common_utils.h"
#include "common/common_vars.h"
#include "command_factory.h"

using namespace std;
using namespace common_types;

static constexpr const char * PRINT_HEADER = "CommandFactory:";

CommandFactory::CommandFactory( common_types::SIncomingCommandServices & _commandServices )
    : m_commandServices(_commandServices)
{

}

PCommand CommandFactory::createCommand( PEnvironmentRequest _request ){

//        VS_LOG_DBG << PRINT_HEADER << common_utils::getCurrentDateTimeStr() << " incoming msg [" << _request->getIncomingMessage() << "]" << endl;

    // check
    if( _request->getIncomingMessage().empty() ){
        sendFailToExternal( _request, "I don't see the command (-_-)" );
        return nullptr;
    }

    // parse
    boost::property_tree::ptree parsedJson;
    try{
        istringstream contentStream( _request->getIncomingMessage() );
        boost::property_tree::json_parser::read_json( contentStream, parsedJson );


    PCommand cmd;

    // -------------------------------------------------------------------------------------
    // service
    // -------------------------------------------------------------------------------------
    if( "service" == parsedJson.get<string>(common_vars::cmd::COMMAND_TYPE) ){
        if( common_vars::cmd::CMD_NAME_PING == parsedJson.get<string>(common_vars::cmd::COMMAND_NAME) ){
            PCommandPing cmd2 = m_poolOfPings.getInstance( & m_commandServices );
            cmd = cmd2;
        }
        else if( common_vars::cmd::SHUTDOWN_SERVER == parsedJson.get<string>(common_vars::cmd::COMMAND_NAME) ){
            PCommandShutdownServer cmd2 = std::make_shared<CommandShutdownServer>( & m_commandServices );
            cmd = cmd2;
        }
        else if( common_vars::cmd::CMD_NAME_CTX_OPEN == parsedJson.get<string>(common_vars::cmd::COMMAND_NAME) ){
            PCommandContextOpen cmd2 = std::make_shared<CommandContextOpen>( & m_commandServices );
            cmd2->m_contextId = parsedJson.get<TContextId>("context_id");
            cmd2->m_requestFullText = _request->getIncomingMessage();
            cmd = cmd2;
        }
        else if( common_vars::cmd::CMD_NAME_CTX_CLOSE == parsedJson.get<string>(common_vars::cmd::COMMAND_NAME) ){
            PCommandContextClose cmd2 = std::make_shared<CommandContextClose>( & m_commandServices );
            cmd = cmd2;
        }
        else{
            VS_LOG_WARN << "unknown service command name [" << parsedJson.get<string>(common_vars::cmd::COMMAND_NAME) << "]"
                        << " of msg [" << _request->getIncomingMessage() << "]"
                        << endl;
            sendFailToExternal( _request, "I don't know such command name of 'external service' command type (-_-)" );
            return nullptr;
        }
    }
    // -------------------------------------------------------------------------------------
    // datasource ( TODO: rename command also to 'datasource' )
    // -------------------------------------------------------------------------------------
    else if( "source" == parsedJson.get<string>(common_vars::cmd::COMMAND_TYPE) ){
        if( common_vars::cmd::CMD_NAME_CONNECT == parsedJson.get<string>(common_vars::cmd::COMMAND_NAME) ){
            PCommandSourceStartRetranslate cmd1 = std::make_shared<CommandSourceStartRetranslate>( & m_commandServices );
            cmd1->m_sourceToConnect.sourceUrl = parsedJson.get<string>("url");
            cmd1->m_sourceToConnect.userId = parsedJson.get<string>("user_id");
            cmd1->m_sourceToConnect.userPassword = parsedJson.get<string>("user_passwd");
            cmd1->m_sourceToConnect.latencyMillisec = parsedJson.get<int>("latency_sec") * 1000;
            cmd1->m_sourceToConnect.outputEncoding = common_utils::convertOutputEncodingType( parsedJson.get<string>("output_encoding") );
            cmd1->m_requestFullText = _request->getIncomingMessage();
            cmd = cmd1;
        }
        else if( common_vars::cmd::CMD_NAME_DISCONNECT == parsedJson.get<string>(common_vars::cmd::COMMAND_NAME) ){
            PCommandSourceStopRetranslate cmd1 = std::make_shared<CommandSourceStopRetranslate>( & m_commandServices );
            cmd1->m_sourceToDisconnect.sourceUrl = parsedJson.get<string>("url");
            cmd = cmd1;
        }
        else if( common_vars::cmd::CMD_NAME_GET == parsedJson.get<string>(common_vars::cmd::COMMAND_NAME) ){
            PCommandGetSources cmd1 = std::make_shared<CommandSourcesGet>( & m_commandServices );
            cmd = cmd1;
        }
        else{
            VS_LOG_WARN << "unknown command name: " << parsedJson.get<string>(common_vars::cmd::COMMAND_NAME) << endl;;
            sendFailToExternal( _request, "I don't know such command name of 'source' command type (-_-)" );
            return nullptr;
        }
    }
    // -------------------------------------------------------------------------------------
    // storage
    // -------------------------------------------------------------------------------------
    else if( "storage" == parsedJson.get<string>(common_vars::cmd::COMMAND_TYPE) ){
        if( common_vars::cmd::CMD_NAME_ARCHIVE_START == parsedJson.get<string>(common_vars::cmd::COMMAND_NAME) ){
            PCommandArchiveStart cmd1 = std::make_shared<CommandArchiveStart>( & m_commandServices );
            cmd1->m_sensorId = parsedJson.get<uint64_t>("sensor_id");
            cmd1->m_archivingName = parsedJson.get<string>("archiving_name");
            cmd1->m_archivingId = parsedJson.get<string>("archiving_id");
            cmd1->m_requestFullText = _request->getIncomingMessage();
            cmd = cmd1;
        }
        else if( common_vars::cmd::CMD_NAME_ARCHIVE_STOP == parsedJson.get<string>(common_vars::cmd::COMMAND_NAME) ){
            PCommandArchiveStop cmd1 = std::make_shared<CommandArchiveStop>( & m_commandServices );
            cmd1->m_archivingId = parsedJson.get<string>("archiving_id");
            cmd1->m_destroy = parsedJson.get<bool>("destroy_current_session");
            cmd = cmd1;
        }
        else if( common_vars::cmd::CMD_NAME_ARCHIVE_STATUS == parsedJson.get<string>(common_vars::cmd::COMMAND_NAME) ){
            PCommandArchiveStatus cmd1 = std::make_shared<CommandArchiveStatus>( & m_commandServices );
            cmd1->m_archivingId = parsedJson.get<string>("archiving_id");
            cmd = cmd1;
        }
        else if( common_vars::cmd::CMD_NAME_ARCHIVE_INFO == parsedJson.get<string>(common_vars::cmd::COMMAND_NAME) ){
            PCommandArchiveInfo cmd1 = std::make_shared<CommandArchiveInfo>( & m_commandServices );

            // get archive info ( in - filter, out - list[source:playlist_path:time start end:quality: ...] )

            cmd = cmd1;
        }
        else if( common_vars::cmd::CMD_NAME_DOWNLOAD == parsedJson.get<string>(common_vars::cmd::COMMAND_NAME) ){
            PCommandDownloadVideo cmd1 = std::make_shared<CommandDownloadVideo>( & m_commandServices );
            cmd1->m_archiveId = parsedJson.get<int32_t>("archive_id");
            cmd1->m_timeRangeSec.first = parsedJson.get<int64_t>("time_range_begin_sec");
            cmd1->m_timeRangeSec.second = parsedJson.get<int64_t>("time_range_end_sec");

            // download video fragment ( in - archive id, time range, out - path to created video file )

            cmd = cmd1;
        }
        else if( common_vars::cmd::CMD_NAME_BOOKMARK == parsedJson.get<string>(common_vars::cmd::COMMAND_NAME) ){
            PCommandVideoBookmark cmd1 = std::make_shared<CommandVideoBookmark>( & m_commandServices );
            cmd1->m_write = parsedJson.get<bool>("write");
            cmd1->m_bookmark.archiveId = parsedJson.get<int>("archive_id");
            cmd1->m_bookmark.label = parsedJson.get<string>("label");
            cmd1->m_bookmark.sourceUrl = parsedJson.get<string>("source_url");
            cmd1->m_bookmark.timePointSec = parsedJson.get<int64_t>("time_point_sec");

            cmd = cmd1;
        }
        else{
            VS_LOG_WARN << "unknown command name: " << parsedJson.get<string>(common_vars::cmd::COMMAND_NAME) << endl;
            sendFailToExternal( _request, "I don't know such command name of 'storage' command type (-_-)" );
            return nullptr;
        }
    }
    // -------------------------------------------------------------------------------------
    // analyze
    // -------------------------------------------------------------------------------------
    else if( "analyze" == parsedJson.get<string>(common_vars::cmd::COMMAND_TYPE) ){
        if( common_vars::cmd::CMD_NAME_ANALYZE_START == parsedJson.get<string>(common_vars::cmd::COMMAND_NAME) ){
            PCommandAnalyzeStart cmd1 = std::make_shared<CommandAnalyzeStart>( & m_commandServices );
            cmd1->m_sensorId = parsedJson.get<uint64_t>("sensor_id");
            cmd1->m_profileId = parsedJson.get<uint64_t>("profile_id");
            cmd1->m_processingId = parsedJson.get<string>("processing_id");
            cmd1->resume = parsedJson.get<bool>("resume_current_session");
            cmd1->m_processingName = parsedJson.get<string>("processing_name");
            cmd1->zoneRect[ 0 ] = parsedJson.get<int>("zone_x");
            cmd1->zoneRect[ 1 ] = parsedJson.get<int>("zone_y");
            cmd1->zoneRect[ 2 ] = parsedJson.get<int>("zone_w");
            cmd1->zoneRect[ 3 ] = parsedJson.get<int>("zone_h");
            cmd1->m_requestFullText = _request->getIncomingMessage();
            boost::property_tree::ptree mapArray = parsedJson.get_child("dpf_to_objrepr");
            for( auto iter = mapArray.begin(); iter != mapArray.end(); ++iter ){
                boost::property_tree::ptree arrEl = iter->second;

                cmd1->m_dpfLabelToObjreprClassinfo;
            }
            cmd = cmd1;
        }
        else if( common_vars::cmd::CMD_NAME_ANALYZE_STOP == parsedJson.get<string>(common_vars::cmd::COMMAND_NAME) ){
            PCommandAnalyzeStop cmd1 = std::make_shared<CommandAnalyzeStop>( & m_commandServices );
            cmd1->m_sensorId = parsedJson.get<uint64_t>("sensor_id");
            cmd1->m_processingId = parsedJson.get<string>("processing_id");
            cmd1->destroy = parsedJson.get<bool>("destroy_current_session");
            cmd = cmd1;
        }
        else if( common_vars::cmd::CMD_NAME_STATUS == parsedJson.get<string>(common_vars::cmd::COMMAND_NAME) ){
            PCommandAnalyzeStatus cmd1 = std::make_shared<CommandAnalyzeStatus>( & m_commandServices );
            cmd1->m_sensorId = parsedJson.get<uint64_t>("sensor_id");
            cmd1->m_processingId = parsedJson.get<string>("processing_id");
            cmd = cmd1;
        }
        else if( common_vars::cmd::CMD_NAME_PLAY_LIVE == parsedJson.get<string>(common_vars::cmd::COMMAND_NAME) ){
            // TODO: uncomment
//            PCommandPlayLive cmd1 = std::make_shared<CommandPlayLive>( & m_commandServices );
//            cmd1->m_live = parsedRecord["live"].asBool();
//            cmd = cmd1;
        }
        else{
            VS_LOG_WARN << "unknown analyze command name: " << parsedJson.get<string>(common_vars::cmd::COMMAND_NAME)
                        << " of msg [" << _request->getIncomingMessage() << "]"
                        << endl;
//            sendFailToExternal( _request, "I don't know such command name of 'analyze' command type (-_-)" );
            sendFailToExternal( _request, "{ \"status\" : \"something shit happens\" }" );
            return nullptr;
        }
    }
    else{
        VS_LOG_WARN << PRINT_HEADER << " unknown command type [" << parsedJson.get<string>(common_vars::cmd::COMMAND_TYPE) << "]" << endl;
        sendFailToExternal( _request, "I don't know such command type (-_-)" );
        return nullptr;
    }


    cmd->m_request = _request;
    return cmd;

    }
    catch( boost::property_tree::json_parser::json_parser_error & _ex ){
        VS_LOG_ERROR << PRINT_HEADER
                     << " parse failed of [" << _request->getIncomingMessage() << "]" << endl
                     << " reason: [" << _ex.what() << "]" << endl;
        return nullptr;
    }
}

void CommandFactory::sendFailToExternal( PEnvironmentRequest _request, const string _msg ){

    boost::property_tree::ptree root;
    root.add( "response", "fail" );
    root.add( "body", _msg );

    ostringstream oss;
    boost::property_tree::json_parser::write_json( oss, root );

    _request->setOutcomingMessage( oss.str() );
}



