
#include <boost/property_tree/json_parser.hpp>
#include <microservice_common/system/logger.h>
#include <microservice_common/common/ms_common_utils.h>

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
    }
    catch( boost::property_tree::json_parser::json_parser_error & _ex ){
        VS_LOG_ERROR << PRINT_HEADER
                     << " parse failed of [" << _request->getIncomingMessage() << "]" << endl
                     << " reason: [" << _ex.what() << "]" << endl;
        return nullptr;
    }

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
    // datasource
    // -------------------------------------------------------------------------------------
    else if( "datasource" == parsedJson.get<string>(common_vars::cmd::COMMAND_TYPE) ){



    }
    // -------------------------------------------------------------------------------------
    // storage
    // -------------------------------------------------------------------------------------
    else if( "storage" == parsedJson.get<string>(common_vars::cmd::COMMAND_TYPE) ){



    }
    // -------------------------------------------------------------------------------------
    // analyze
    // -------------------------------------------------------------------------------------
    else if( "analyze" == parsedJson.get<string>(common_vars::cmd::COMMAND_TYPE) ){



    }
    else{
        VS_LOG_WARN << PRINT_HEADER << " unknown command type [" << parsedJson.get<string>(common_vars::cmd::COMMAND_TYPE) << "]" << endl;
        sendFailToExternal( _request, "I don't know such command type (-_-)" );
        return nullptr;
    }

    cmd->m_request = _request;
    return cmd;
}

void CommandFactory::sendFailToExternal( PEnvironmentRequest _request, const string _msg ){

    boost::property_tree::ptree root;
    root.add( "response", "fail" );
    root.add( "body", _msg );

    ostringstream oss;
    boost::property_tree::json_parser::write_json( oss, root );

    _request->setOutcomingMessage( oss.str() );
}



