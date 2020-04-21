
#include <boost/format.hpp>
#include <jsoncpp/json/writer.h>

#include "video_source/source_manager.h"
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

    for( PConstVideoRetranslator rtspSource : m_services->sourceManager->getRTSPSources() ){

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

    sendResponse( root, true );
    return true;
}
