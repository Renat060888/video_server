
#include "storage/storage_engine_facade.h"
#include "cmd_video_bookmark.h"

using namespace std;
using namespace common_types;

CommandVideoBookmark::CommandVideoBookmark( SIncomingCommandServices * _services )
    : ICommandExternal( _services )
{

}

#include <jsoncpp/json/writer.h>

bool CommandVideoBookmark::exec(){

    IRecordingMetadataStore * itf = ((SIncomingCommandServices *)m_services)->storageEngine->getServiceRecordingMetadataStore();

    if( m_write ){
        // TODO: temp
        itf->writeMetadata( m_bookmark );
        string response = "storage engine :: write bookmark error: " + ((SIncomingCommandServices *)m_services)->storageEngine->getLastError();

        // TODO: remove after protocol refactor (response/body)
        Json::FastWriter writer;
        Json::Value root;
        root["response"] = "fail";
        root["body"] = response;
        //

        sendResponse( writer.write(root) );
        return false;
    }
    else{
        // TODO: temp
        itf->deleteMetadata( m_bookmark );
        string response = "storage engine :: delete bookmark error: " + ((SIncomingCommandServices *)m_services)->storageEngine->getLastError();

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
