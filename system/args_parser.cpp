
#include "common/common_vars.h"
#include "common/common_types.h"
#include "args_parser.h"

using namespace std;

bpo::options_description ArgsParser::getArgumentsDescrTemplateMethodPart() {

    bpo::options_description desc("Available options");

    desc.add_options()
            ("help,h","show this help")
            ("version,V", "version of program")
            ("about,A", "description")
            ("main-config,C", bpo::value<std::string>(), "main config file")
            ("main-config-sample,P", "print main config sample")
            ("daemon,D", "start as daemon")
            ("start", "start server")
            ("stop", "stop server")
            ("sources", "get available sources")
            ("cp-test", "run for child process testing")
            ("analyze", "command for analyze start ( need 'sensor-id' and 'profile-id' parameters also")
            ("archiving", "command for archiving start ( need 'sensor-id' parameter also")
            ("sensor-id", bpo::value<common_types::TSensorId>(), "objrepr sensor id of camera")
            ("profile-id", bpo::value<common_types::TProfileId>(), "objrepr profile id for DPF")
            ("role,R", bpo::value<std::string>(), "role of this video-server module [ detector | classificator | identificator ]")
            (common_vars::cmd::CMD_NAME_CONNECT.c_str(), bpo::value<std::string>(), "connect new source to server")
            (common_vars::cmd::CMD_NAME_DISCONNECT.c_str(), bpo::value<std::string>(), "disconnect source")
            ;

    return desc;
}

void ArgsParser::checkArgumentsTemplateMethodPart( const bpo::variables_map & _varMap ) {

    if( _varMap.find("main-config") != _varMap.end() ){
        m_commmandLineArgs[EVideoServerArguments::MAIN_CONFIG_PATH] = _varMap["main-config"].as<std::string>();
    }

    if( _varMap.find("daemon") != _varMap.end() ){
        m_commmandLineArgs[EVideoServerArguments::AS_DAEMON] = "bla-bla";
    }

    if( _varMap.find("start") != _varMap.end() ){
        m_commmandLineArgs[EVideoServerArguments::SHELL_CMD_START_VIDEO_SERVER] = "bla-bla";
    }

    if( _varMap.find("stop") != _varMap.end() ){
        m_commmandLineArgs[EVideoServerArguments::SHELL_CMD_TO_VIDEO_SERVER] = AArgsParser::getSettings().commandConvertor->getCommandFromProgramArgs( {
                {common_vars::cmd::SHUTDOWN_SERVER, ""}
                } );
    }

    if( _varMap.find("sources") != _varMap.end() ){
        m_commmandLineArgs[EVideoServerArguments::SHELL_CMD_TO_VIDEO_SERVER] = AArgsParser::getSettings().commandConvertor->getCommandFromProgramArgs( {
               {"sources", ""}
               } );
    }

    if( _varMap.find("cp-test") != _varMap.end() ){
        m_commmandLineArgs[EVideoServerArguments::CHILD_PROCESS] = "bla-bla";
    }

    if( _varMap.find("analyze") != _varMap.end() ){
        if( _varMap.find("sensor-id") == _varMap.end() || _varMap.find("profile-id") == _varMap.end() ){
            PRELOG_ERR << "ERROR: no 'sensor-id' or 'profile-id' parameter provided for analyze" << endl;
            exit( EXIT_FAILURE );
        }

        const common_types::TSensorId sensorId = _varMap["sensor-id"].as<common_types::TSensorId>();
        const common_types::TProfileId profileId = _varMap["profile-id"].as<common_types::TProfileId>();
        m_commmandLineArgs[EVideoServerArguments::SHELL_CMD_TO_VIDEO_SERVER] = AArgsParser::getSettings().commandConvertor->getCommandFromProgramArgs( {
               {"analyze", "start"},
               {"sensor-id", std::to_string(sensorId)},
               {"profile-id", std::to_string(profileId)},
               } );
    }

    if( _varMap.find("archiving") != _varMap.end() ){
        if( _varMap.find("sensor-id") == _varMap.end() ){
            PRELOG_ERR << "ERROR: no 'sensor-id' parameter provided for archiving" << endl;
            exit( EXIT_FAILURE );
        }

        const common_types::TSensorId sensorId = _varMap["sensor-id"].as<common_types::TSensorId>();
        m_commmandLineArgs[EVideoServerArguments::SHELL_CMD_TO_VIDEO_SERVER] = AArgsParser::getSettings().commandConvertor->getCommandFromProgramArgs( {
               {"archiving", "start"},
               {"sensor-id", std::to_string(sensorId)},
               } );
    }

    if( _varMap.find("role") != _varMap.end() ){
        const string role = _varMap["role"].as<std::string>();
        if( role != "detector" && role != "classificator" && role != "identificator" ){
            PRELOG_ERR << "ERROR: possible roles - detector | classificator | identificator" << endl;
            exit( EXIT_FAILURE );
        }
        m_commmandLineArgs[EVideoServerArguments::THIS_PROCESS_ROLE] = role;
    }
    else{
        m_commmandLineArgs[EVideoServerArguments::THIS_PROCESS_ROLE] = "regular";
    }

    if( _varMap.find(common_vars::cmd::CMD_NAME_CONNECT) != _varMap.end() ){
        m_commmandLineArgs[EVideoServerArguments::SHELL_CMD_TO_VIDEO_SERVER] = AArgsParser::getSettings().commandConvertor->getCommandFromProgramArgs( {
                {common_vars::cmd::CMD_NAME_CONNECT, _varMap[common_vars::cmd::CMD_NAME_CONNECT].as<std::string>()}
                } );
    }

    if( _varMap.find(common_vars::cmd::CMD_NAME_DISCONNECT) != _varMap.end() ){
        m_commmandLineArgs[EVideoServerArguments::SHELL_CMD_TO_VIDEO_SERVER] = AArgsParser::getSettings().commandConvertor->getCommandFromProgramArgs( {
               {common_vars::cmd::CMD_NAME_DISCONNECT, _varMap[common_vars::cmd::CMD_NAME_DISCONNECT].as<std::string>()}
               } );
    }
}

void ArgsParser::version() {

    // TODO: do
}

void ArgsParser::about() {

    // TODO: do
}
