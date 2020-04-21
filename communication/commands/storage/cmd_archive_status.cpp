
#include "cmd_archive_status.h"
#include "storage/video_recorder.h"
#include "video_server_common/common/common_utils_specific.h"

using namespace std;
using namespace common_types;

const std::string CommandArchiveStatus::ALL_ARCHIVINGS = "all_archivings";

CommandArchiveStatus::CommandArchiveStatus( SIncomingCommandServices * _services )
    : ICommandExternal( _services )
{

}

bool CommandArchiveStatus::exec(){

    Json::Value response;
    Json::Value statusesRecords;

    // one archiver
    if( m_archivingId != ALL_ARCHIVINGS ){

        ArchiveCreatorProxy::SCurrentState status = m_services->videoRecorder->getArchiverState( m_archivingId );

        Json::Value statusRecord;
        statusRecord["sensor_id"] = (unsigned long long)status.initSettings->sensorId;
        statusRecord["state"] = common_utils::convertArchivingStateToStr( status.state );
        statusRecord["archiving_id"] = status.initSettings->archivingId;
        statusRecord["archiving_name"] = status.initSettings->archivingName;

        statusesRecords.append( statusRecord );
    }
    // all archivers
    else{
        for( const ArchiveCreatorProxy::SCurrentState & status : m_services->videoRecorder->getArchiverStates() ){

            Json::Value statusRecord;
            statusRecord["sensor_id"] = (unsigned long long)status.initSettings->sensorId;
            statusRecord["state"] = common_utils::convertArchivingStateToStr( status.state );
            statusRecord["archiving_id"] = status.initSettings->archivingId;
            statusRecord["archiving_name"] = status.initSettings->archivingName;

            statusesRecords.append( statusRecord );
        }
    }

    response["archivers_status"] = statusesRecords;
    sendResponse( response, true );

    return true;
}
