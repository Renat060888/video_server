
#include "video_server_common/system/config_reader.h"
#include "video_source/source_manager.h"
#include "storage/video_recorder.h"
#include "cmd_source_connect.h"

using namespace std;
using namespace common_types;

CommandSourceStartRetranslate::CommandSourceStartRetranslate( SIncomingCommandServices * _services ) :
    ICommandExternal( _services )
{

}

bool CommandSourceStartRetranslate::exec(){

    if( 0 == (CONFIG_PARAMS.SYSTEM_SERVER_FEATURES & (int16_t)EServerFeatures::RETRANSLATION) ){
        Json::Value response;
        response["error_msg"] = "retranslation is not available on this video server";
        sendResponse( response, false );
        return false;
    }

    if( ! m_services->sourceManager->getServiceOfSourceRetranslation()->launchRetranslation( m_sourceToConnect ) ){
        Json::Value response;
        response["error_msg"] = m_services->sourceManager->getLastError();
        sendResponse( response, false );
        return false;
    }

    sendResponse( Json::Value(), true );
    return true;
}
