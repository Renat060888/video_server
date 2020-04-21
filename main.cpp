
//#define RUN_TESTS

#ifdef RUN_TESTS
#include <microservice_common/unit_tests/storage_tests.h>
#endif

#include <boost/property_tree/json_parser.hpp>
#include <microservice_common/system/logger.h>
#include <microservice_common/system/daemonizator.h>
#include <microservice_common/communication/shell.h>

#include "communication/unified_command_convertor_vs.h"
#include "system/args_parser.h"
#include "system/config_reader.h"
#include "system/path_locator.h"
#include "video_server.h"

using namespace std;

static bool initSingletons( int _argc, char ** _argv, char ** _env ){

    // cmd line arguments
    ArgsParser::SInitSettings settings;
    settings.commandConvertor = & UNIFIED_COMMAND_CONVERTOR;
    settings.argc = _argc;
    settings.argv = _argv;
    settings.printConfigExample = std::bind( & AConfigReader::getConfigExample, & CONFIG_READER );
    if( ! ARGS_PARSER.init(settings) ){
        return false;
    }

    // configs
    ConfigReader::SIninSettings settings3;
    settings3.commandConvertor = & UNIFIED_COMMAND_CONVERTOR;
    settings3.mainConfigPath = ARGS_PARSER.getVal(EPlayerArguments::MAIN_CONFIG_PATH);
    settings3.env = _env;
    settings3.projectName = "video_server";
    if( ! CONFIG_READER.init(settings3) ){
        return false;
    }

    // logger
    logger_common::SInitSettings settings2;
    settings2.loggerName = "VideoServer";
    settings2.unilogConfigPath = CONFIG_PARAMS.baseParams.SYSTEM_UNILOG_CONFIG_PATH;

    if( CONFIG_PARAMS.baseParams.SYSTEM_LOG_TO_STDOUT ){
        settings2.logEndpoints = (logger_common::ELogEndpoints)( (int)settings2.logEndpoints | (int)logger_common::ELogEndpoints::Stdout );
    }
    if( CONFIG_PARAMS.baseParams.SYSTEM_LOG_TO_FILE ){
        settings2.logEndpoints = (logger_common::ELogEndpoints)( (int)settings2.logEndpoints | (int)logger_common::ELogEndpoints::File );
        settings2.fileName = CONFIG_PARAMS.baseParams.SYSTEM_LOGFILE_NAME;
        settings2.filePath = CONFIG_PARAMS.baseParams.SYSTEM_REGULAR_LOGFILE_PATH;
        settings2.rotationSizeMb = CONFIG_PARAMS.baseParams.SYSTEM_LOGFILE_ROTATION_SIZE_MB;
    }

    Logger::singleton().initGlobal( settings2 );

    return true;
}

static void parseResponse( const std::string & _msg ){

    // parse base part
    boost::property_tree::ptree parsedRepsonseJson;
    try{
        istringstream contentStream( _msg );
        boost::property_tree::json_parser::read_json( contentStream, parsedRepsonseJson );
    }
    catch( boost::property_tree::json_parser::json_parser_error & _ex ){
        VS_LOG_ERROR    << " parse failed of [" << _msg << "]" << endl
                        << "Reason: [" << _ex.what() << "]" << endl;
        return;
    }

    VS_LOG_INFO << "response from unix-socket-dss [" <<  _msg << "]" << endl;

    // TODO: print values from parsedRepsonseJson ?
}

static bool executeShellCommand(){

    if( ARGS_PARSER.isKeyExist(EPlayerArguments::SHELL_CMD_START_VIDEO_SERVER) ){

        // deamonize
        if( ARGS_PARSER.isKeyExist(EPlayerArguments::AS_DAEMON) ){
            if( ! Daemonizator::turnIntoDaemon() ){
                return false;
            }

            // reinit logger for daemon
            logger_common::SInitSettings settings2;
            settings2.loggerName = "VideoServerDaemon";
            settings2.unilogConfigPath = CONFIG_PARAMS.baseParams.SYSTEM_UNILOG_CONFIG_PATH;
            settings2.logEndpoints = logger_common::ELogEndpoints::File;
            settings2.fileName = CONFIG_PARAMS.baseParams.SYSTEM_LOGFILE_NAME;
            settings2.filePath = CONFIG_PARAMS.baseParams.SYSTEM_REGULAR_LOGFILE_PATH;
            settings2.rotationSizeMb = CONFIG_PARAMS.baseParams.SYSTEM_LOGFILE_ROTATION_SIZE_MB;

            Logger::singleton().initGlobal( settings2 );

            VS_LOG_INFO << "============================ RECORDER AGENT AS DAEMON ========================" << endl;
        }

        // launch agent
        {
            VideoServer::SInitSettings settings;
            VideoServer server;
            if( server.init(settings) ){
                server.launch();
            }
            else{
                return false;
            }
        }
    }
    else if( ARGS_PARSER.isKeyExist(EPlayerArguments::SHELL_CMD_TO_VIDEO_SERVER) ){

        // reinit logger for client side
        logger_common::SInitSettings settings;
        settings.loggerName = "VideoServerShellClient";
        settings.unilogConfigPath = CONFIG_PARAMS.baseParams.SYSTEM_UNILOG_CONFIG_PATH;
        Logger::singleton().initGlobal( settings );

        // shell client
        Shell::SInitSettings settings2;
        settings2.shellMode = Shell::EShellMode::CLIENT;
        settings2.messageMode = Shell::EMessageMode::WITHOUT_SIZE;
        settings2.socketFileName = PATH_LOCATOR.getShellImitationDomainSocket();
        Shell shell( INetworkEntity::INVALID_CONN_ID );
        if( ! shell.init(settings2) ){
            return false;
        }

        // send message to server
        const string message =  ARGS_PARSER.getVal(EPlayerArguments::SHELL_CMD_TO_VIDEO_SERVER);
        const string response = shell.makeBlockedRequest( message );
        parseResponse( response );
    }
    else{
        assert( false );
    }

    return true;
}

int main( int argc, char ** argv, char ** env ){

    if( ! initSingletons(argc, argv, env) ){
        PRELOG_ERR << "============================ VIDEO SERVER START FAILED (singletons area) ============================" << endl;
        return -1;
    }

#ifndef RUN_TESTS
    VS_LOG_INFO << endl;
    VS_LOG_INFO << endl;
    if( ! executeShellCommand() ){
        VS_LOG_ERROR << "============================ VIDEO SERVER START FAILED ============================" << endl;
        return -1;
    }
#else
    StorageTests storageTests;
    storageTests.runTests();
#endif

    VS_LOG_INFO << endl;
    VS_LOG_INFO << endl;
    VS_LOG_INFO << "============================ VIDEO SERVER PROCESS RETURNS '0' ============================" << endl;
    return 0;
}







