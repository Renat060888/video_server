
#include <iostream>
#include <sys/stat.h>

#include <microservice_common/common/ms_common_utils.h>
#include <microservice_common/system/logger.h>
#include <microservice_common/system/process_launcher.h>

#include "objrepr_bus_vs.h"
#include "config_reader.h"
#include "system_environment_facade_vs.h"

using namespace std;
using namespace common_types;

static constexpr const char * PRINT_HEADER = "SystemEnvironmentVS:";

SystemEnvironmentFacadeVS::SystemEnvironmentFacadeVS()
{

}

SystemEnvironmentFacadeVS::~SystemEnvironmentFacadeVS()
{
    VS_LOG_INFO << PRINT_HEADER << " begin shutdown" << endl;


    OBJREPR_BUS.shutdown();


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








    VS_LOG_INFO << PRINT_HEADER << " init success" << endl;
    return true;
}



