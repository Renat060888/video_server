
#include <boost/format.hpp>
#include <jsoncpp/json/writer.h>

#include "datasource/source_manager_facade.h"
#include "storage/video_recorder.h"
#include "cmd_sources_get.h"

using namespace std;
using namespace common_types;

CommandSourcesGet::CommandSourcesGet( SIncomingCommandServices * _services ) :
    ICommandExternal(_services)
{

}

CommandSourcesGet::~CommandSourcesGet()
{

}

bool CommandSourcesGet::exec(){

    Json::Value array;

    for( PConstVideoRetranslator rtspSource : ((SIncomingCommandServices *)m_services)->sourceManager->getRTSPSources() ){

        Json::Value arrElement;
        arrElement["clients_count"] = rtspSource->getState().clientCount;
        arrElement["source_url"] = rtspSource->getSettings().sourceUrl;
        arrElement["port"] = rtspSource->getSettings().serverPort;
        arrElement["portfix"] = rtspSource->getSettings().rtspPostfix;

        const string fullPath = ( boost::format( "rtsp://%1%:%2%%3%" )
                             % rtspSource->getSettings().listenIp
                             % rtspSource->getSettings().serverPort
                             % rtspSource->getSettings().rtspPostfix
                             ).str();

        arrElement["full_path"] = fullPath;

        array.append( arrElement );
    }

    // TODO: source may be just recorded, without RTSP-server launch
//    for( const HLSCreator::SCurrentState & hlsState : m_services->videoRecorder->getHLSStates() ){

//    }

    Json::Value root;
    root["sources"] = array;

    // TODO: remove after protocol refactor (response/body)
    Json::FastWriter writer;
    Json::Value root2;
    root2["response"] = "success";
    root2["body"] = root;
    //

    sendResponse( writer.write(root2) );
    return true;
}
