
#include "storage/video_recorder.h"
#include "system/config_reader.h"
#include "cmd_source_disconnect.h"
#include "datasource/source_manager_facade.h"

using namespace std;
using namespace common_types;

CommandSourceStopRetranslate::CommandSourceStopRetranslate( SIncomingCommandServices * _services ) :
    ICommandExternal(_services)
{

}

CommandSourceStopRetranslate::~CommandSourceStopRetranslate(){

}

#include <jsoncpp/json/writer.h>

bool CommandSourceStopRetranslate::exec(){

    if( 0 == (CONFIG_PARAMS.baseParams.SYSTEM_SERVER_FEATURES & (int16_t)EServerFeatures::RETRANSLATION) ){
        Json::Value response;
        response["error_msg"] = "retranslation is not available on this video server";

        // TODO: remove after protocol refactor (response/body)
        Json::FastWriter writer;
        Json::Value root;
        root["response"] = "fail";
        root["body"] = response;
        //

        sendResponse( writer.write(root) );
        return false;
    }

    if( ! ((SIncomingCommandServices *)m_services)->sourceManager->getServiceOfSourceRetranslation()->stopRetranslation( m_sourceToDisconnect ) ){
        Json::Value response;
        response["error_msg"] = ((SIncomingCommandServices *)m_services)->sourceManager->getLastError();

        // TODO: remove after protocol refactor (response/body)
        Json::FastWriter writer;
        Json::Value root;
        root["response"] = "fail";
        root["body"] = response;
        //

        sendResponse( writer.write(root) );
        return false;
    }

    // TODO: remove after protocol refactor (response/body)
    Json::FastWriter writer;
    Json::Value root;
    root["response"] = "success";
    root["body"] = "";
    //

    sendResponse( writer.write(root) );
    return true;
}







