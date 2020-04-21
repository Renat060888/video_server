
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <microservice_common/common/ms_common_utils.h>

#include "unified_command_convertor_vs.h"
#include "system/config_reader.h"

using namespace std;

static constexpr const char * PRINT_HEADER = "UnifiedCommandConvertor:";

UnifiedCommandConvertorVS::UnifiedCommandConvertorVS()
{

}

std::string UnifiedCommandConvertorVS::getCommandFromProgramArgs( const std::map<common_types::TCommandLineArgKey, common_types::TCommandLineArgVal> & _args ){

    assert( false && "TODO: do" );
}

std::string UnifiedCommandConvertorVS::getCommandFromConfigFile( const std::string & _command ){

    // parse base part
    boost::property_tree::ptree config;

    istringstream contentStream( _command );
    try{
        boost::property_tree::json_parser::read_json( contentStream, config );
    }
    catch( boost::property_tree::json_parser::json_parser_error & _ex ){
        PRELOG_ERR << ::PRINT_HEADER << " parse failed of [" << _command << "]" << endl
             << "Reason: [" << _ex.what() << "]" << endl;
        return string();
    }

    // create unified command
    const string ctxName = CONFIG_READER.setParameterNew<std::string>( config, "ctx_name", string("") );
    const string missionName = CONFIG_READER.setParameterNew<std::string>( config, "mission_name", string("") );

    boost::property_tree::ptree unifiedCommand;
    unifiedCommand.add( "cmd_type", "service" );
    unifiedCommand.add( "cmd_name", "context_listen_start" );
    unifiedCommand.add( "ctx_name", ctxName );
    unifiedCommand.add( "mission_name", missionName );

    ostringstream oss;
    boost::property_tree::json_parser::write_json( oss, unifiedCommand );
    const string cmdStr = oss.str();

    return cmdStr;
}

std::string UnifiedCommandConvertorVS::getCommandFromHTTPRequest( const std::string & _httpMethod,
                                        const std::string & _uri,
                                        const std::string & _queryString,
                                        const std::string & _body ){

    assert( false && "TODO: do" );
}
