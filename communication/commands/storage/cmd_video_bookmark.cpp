
#include "storage/storage_engine_facade.h"
#include "cmd_video_bookmark.h"

using namespace std;
using namespace common_types;

CommandVideoBookmark::CommandVideoBookmark( SIncomingCommandServices * _services )
    : ICommandExternal( _services )
{

}

bool CommandVideoBookmark::exec(){

    IRecordingMetadataStore * itf = m_services->storageEngine->getServiceRecordingMetadataStore();

    if( m_write ){
        // TODO: temp
        itf->writeMetadata( m_bookmark );
        string response = "storage engine :: write bookmark error: " + m_services->storageEngine->getLastError();

        sendResponse( response, false );
        return false;
    }
    else{
        // TODO: temp
        itf->deleteMetadata( m_bookmark );
        string response = "storage engine :: delete bookmark error: " + m_services->storageEngine->getLastError();

        sendResponse( response, false );
        return false;
    }

    sendResponse( Json::Value(), true );
    return true;
}
