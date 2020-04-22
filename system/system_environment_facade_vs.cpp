
#include <iostream>
#include <sys/stat.h>

#include <gst/gst.h>
#include <microservice_common/common/ms_common_utils.h>
#include <microservice_common/system/logger.h>
#include <microservice_common/system/process_launcher.h>

#include "objrepr_bus_vs.h"
#include "config_reader.h"
#include "args_parser.h"
#include "system_environment_facade_vs.h"

using namespace std;
using namespace common_types;

static constexpr const char * PRINT_HEADER = "SystemEnvironmentVS:";

SystemEnvironmentFacadeVS::SystemEnvironmentFacadeVS(){

    // GStreamer init
    int argc = ARGS_PARSER.getSettings().argc;
    char ** argv = ARGS_PARSER.getSettings().argv;

    const string pluginsPath = ConfigReader::singleton().get().GSTREAMER_CUSTOM_PLUGINS_PATH;
    if( ! pluginsPath.empty() ){
        VS_LOG_INFO << "setenv GST_PLUGIN_PATH to: " << pluginsPath << endl;
        ::setenv( "GST_PLUGIN_PATH", pluginsPath.c_str(), 0 );
    }

    VS_LOG_INFO << "GStreamer additional plugins path - " << pluginsPath << endl;

//    ::setenv( "GST_DEBUG", "rtpjpegpay:7", 0 );
    ::setenv( "GST_DEBUG", "*:3", 0 );
//    ::setenv( "GST_DEBUG", "fatal_warnings", 0 );

    VS_LOG_INFO << "GStreamer init begin..." << endl;
    ::gst_init( & argc, & argv );
    VS_LOG_INFO << "...GStreamer init done" << endl;
}

SystemEnvironmentFacadeVS::~SystemEnvironmentFacadeVS(){

    VS_LOG_INFO << PRINT_HEADER << " begin shutdown" << endl;


    OBJREPR_BUS.shutdown();
    ::gst_deinit();


    VS_LOG_INFO << PRINT_HEADER << " shutdown success" << endl;
}

bool SystemEnvironmentFacadeVS::init( const SInitSettings & _settings ){

    // bus
    ObjreprBus::SInitSettings busSettings;
    busSettings.objreprConfigPath = CONFIG_PARAMS.baseParams.OBJREPR_CONFIG_PATH;
    busSettings.initialContextName = CONFIG_PARAMS.baseParams.OBJREPR_INITIAL_CONTEXT_NAME;
    if( ! OBJREPR_BUS.init(busSettings) ){
        VS_LOG_CRITICAL << "objrepr init failed: " << OBJREPR_BUS.getLastError() << endl;
        return false;
    }

    // base class
    SInitSettings settingsForBase;
    settingsForBase.services.contextControlService = & OBJREPR_BUS;
    if( ! SystemEnvironmentFacade::init(_settings) ){
        return false;
    }

    m_state.settings = _settings;

    // register in GDM
    if( _settings.registerInGdm ){
        if( ! OBJREPR_BUS.registerVideoServerInGdm(CONFIG_PARAMS.baseParams.OBJREPR_GDM_VIDEO_CONTAINER_CLASSINFO_NAME,
                                                   CONFIG_PARAMS.baseParams.SYSTEM_UNIQUE_ID) ){
            VS_LOG_CRITICAL << PRINT_HEADER << " objrepr register in GDM failed :(" << endl;
            return false;
        }
    }



    VS_LOG_INFO << PRINT_HEADER << " init success" << endl;
    return true;
}



