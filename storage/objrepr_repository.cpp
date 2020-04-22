
#include <microservice_common/system/logger.h>

#include "system/config_reader.h"
#include "system/objrepr_bus_vs.h"
#include "common/common_utils.h"
#include "objrepr_repository.h"

using namespace std;
using namespace common_types;

static const std::string ARCHIVE_INSTANCE_CLASSINFO_NAME = "Видеоархив";
static const std::string ARCHIVE_INSTANCE_ATTR_TIME_BEGIN = "begin_time";
static const std::string ARCHIVE_INSTANCE_ATTR_TIME_END = "end_time";
static const std::string ARCHIVE_INSTANCE_ATTR_PLAYLIST_URL = "playlist_url";
static const std::string ARCHIVE_INSTANCE_ATTR_LIVE_STREAM_PLAYLIST_URL = "live_stream_playlist_url";
static const std::string ARCHIVE_NAME_PREFIX = "vs_archive";

static constexpr const char * PRINT_HEADER = "ObjreprRepos:";

ObjreprRepository::ObjreprRepository()
    : m_threadAnalyticEventsProcessing(nullptr)
    , m_shutdownCalled(false)
{

}

ObjreprRepository::~ObjreprRepository()
{
    m_shutdownCalled = true;
    common_utils::threadShutdown( m_threadAnalyticEventsProcessing );
}


bool ObjreprRepository::init( SInitSettings _settings ){

    // TODO: 'init' called only in StorageEngine

    // TODO: ObjreprRepository not a singleton !
    m_threadAnalyticEventsProcessing = new std::thread( & ObjreprRepository::threadAnalyticEventsProcessing, this );

    repairPossibleCorruptedArchives();







    return true;
}

void ObjreprRepository::repairPossibleCorruptedArchives(){

    objrepr::SpatialObjectManager * objManager = objrepr::RepresentationServer::instance()->objectManager();
    objrepr::GlobalDataManager * globalManager = objrepr::RepresentationServer::instance()->globalManager();

    vector<objrepr::SpatialObjectPtr> sensors;

    // traverse on all cameras and them archives in current context
    vector<objrepr::ClassinfoPtr> classes = globalManager->classinfoListByCat( "sensor" );
    for( objrepr::ClassinfoPtr & currentClassinfo : classes ){

        vector<objrepr::SpatialObjectPtr> classinfoObjects =  objManager->getObjectsByClassinfo( currentClassinfo->id() );
        for( objrepr::SpatialObjectPtr & object : classinfoObjects ){

            objrepr::SensorObjectPtr sensor = boost::dynamic_pointer_cast<objrepr::SensorObject>( object );
            if( sensor ){
                sensors.push_back( sensor );
            }
        }
    }

    // compare with info in database( semi-real, metadata ) / stored actual data
    for( objrepr::SpatialObjectPtr & sensor : sensors ){
        vector<objrepr::SpatialObjectPtr> sensorChilds =  objManager->getObjectsByParent( sensor->id() );

        for( objrepr::SpatialObjectPtr & child : sensorChilds ){

            if( string(child->classinfo()->name()) == ARCHIVE_INSTANCE_CLASSINFO_NAME ){
                objrepr::SpatialObjectPtr archive = child;

                checkForDead( archive );
                checkForInconsistency( archive );
            }
        }
    }
}

void ObjreprRepository::checkForDead( objrepr::SpatialObjectPtr & _archive ){

    // 1 delete archive-objects which have no stored actual data
    objrepr::StringAttributePtr playlistPathAttr = boost::dynamic_pointer_cast<objrepr::StringAttribute>( _archive->attrMap()->getAttr(ARCHIVE_INSTANCE_ATTR_PLAYLIST_URL.c_str()) );
    if( ! playlistPathAttr ){
        VS_LOG_CRITICAL << "objrepr video-archive-info instance " << _archive->name() << "]"
                     << "attr not found: " << ARCHIVE_INSTANCE_ATTR_PLAYLIST_URL
                     << endl;
        return;
    }

    const string playlistPathStr = playlistPathAttr->valueAsString();
    const string::size_type lastSlashPos = playlistPathStr.find_last_of("/");
    const string playlistName = playlistPathStr.substr( lastSlashPos + 1, playlistPathStr.size() - lastSlashPos );
    const string withoutPlaylistName = playlistPathStr.substr( 0, lastSlashPos );
    const string::size_type lastSlashPos2 = withoutPlaylistName.find_last_of("/");
    const string tag = withoutPlaylistName.substr( lastSlashPos2 + 1, withoutPlaylistName.size() - lastSlashPos2 );

    // TODO: what if ROOT DIR changed ?
}

void ObjreprRepository::checkForInconsistency( objrepr::SpatialObjectPtr & _archive ){

    objrepr::SpatialObjectManager * objManager = objrepr::RepresentationServer::instance()->objectManager();

    // get attrs
    objrepr::TimeAttributePtr timeBeginAttr = boost::dynamic_pointer_cast<objrepr::TimeAttribute>( _archive->attrMap()->getAttr(ARCHIVE_INSTANCE_ATTR_TIME_BEGIN.c_str()) );
    if( ! timeBeginAttr ){
        VS_LOG_CRITICAL << "objrepr video-archive-info instance [" << _archive->name() << "]"
                     << " attr not found: " << ARCHIVE_INSTANCE_ATTR_TIME_BEGIN
                     << endl;
        return;
    }

    objrepr::TimeAttributePtr timeEndAttr = boost::dynamic_pointer_cast<objrepr::TimeAttribute>( _archive->attrMap()->getAttr(ARCHIVE_INSTANCE_ATTR_TIME_END.c_str()) );
    if( ! timeEndAttr ){
        VS_LOG_CRITICAL << "objrepr video-archive-info instance [" << _archive->name() << "]"
                     << " attr not found: " << ARCHIVE_INSTANCE_ATTR_TIME_END
                     << endl;
        return;
    }

    objrepr::StringAttributePtr playlistPathAttr = boost::dynamic_pointer_cast<objrepr::StringAttribute>( _archive->attrMap()->getAttr(ARCHIVE_INSTANCE_ATTR_PLAYLIST_URL.c_str()) );
    if( ! playlistPathAttr ){
        VS_LOG_CRITICAL << "objrepr video-archive-info instance " << _archive->name() << "]"
                     << "attr not found: " << ARCHIVE_INSTANCE_ATTR_PLAYLIST_URL
                     << endl;
        return;
    }

    // vars to locate a directory with data
    const string playlistPathStr = playlistPathAttr->valueAsString();
    const string::size_type lastSlashPos = playlistPathStr.find_last_of("/");
    const string playlistName = playlistPathStr.substr( lastSlashPos + 1, playlistPathStr.size() - lastSlashPos );
    const string withoutPlaylistName = playlistPathStr.substr( 0, lastSlashPos );
    const string::size_type lastSlashPos2 = withoutPlaylistName.find_last_of("/");
    const string sessionName = withoutPlaylistName.substr( lastSlashPos2 + 1, withoutPlaylistName.size() - lastSlashPos2 );

    // close
    const uint64_t timeEndNum = timeEndAttr->value();
    const time_t inUnixTimeFormat = timeEndNum;
    const tm * localTime = localtime( & inUnixTimeFormat );

    if( 0 == localTime->tm_sec ){
        const string archivePlaylistDirFullPath = CONFIG_PARAMS.HLS_WEBSERVER_DIR_ROOT + "/" + sessionName + "/" + playlistName;

        int64_t totalTsDurationSec = 0;
        try {
            boost::filesystem::path filePath( archivePlaylistDirFullPath );
            if( ! boost::filesystem::exists(filePath) ){
                VS_LOG_ERROR << "playlist [" << archivePlaylistDirFullPath << "] of archive [" << _archive->name() << "] is not exist" << endl;
                return;
            }

            totalTsDurationSec = getTotalTsDurationSecFromPlaylist( archivePlaylistDirFullPath );
            if( -1 == totalTsDurationSec ){
                return;
            }

        } catch( const boost::filesystem::filesystem_error & _err ){
            VS_LOG_WARN << "boost exception, WHAT: " << _err.what() << " CODE MSG: " << _err.code().message() << endl;
        }

        const uint64_t timeBeginStr = timeBeginAttr->value(); // TODO: sec / millisec ?
        const int64_t newCalculatedValue = timeBeginStr + totalTsDurationSec;

        timeEndAttr->setValue( newCalculatedValue );
        VS_LOG_WARN << "repair END of corrupted archive [" << _archive->name() << "] to [" << timeEndAttr->valueAsString() << "]" << endl;

        if( ! objManager->commitObject(_archive->id()) ){
            VS_LOG_ERROR << "objrepr video-archive-info instance commit failed"
                      << endl;
            return;
        }
    }
}

int64_t ObjreprRepository::getTotalTsDurationSecFromPlaylist( const std::string & _playlistFullPath ){

    std::ifstream playlistFile( _playlistFullPath );

    if( ! playlistFile.is_open() ){
        VS_LOG_ERROR << "file could not be open [" << _playlistFullPath << "]" << endl;
        return -1;
    }

    double tsTotalDuration = 0.0f;

    string line;
    while( std::getline(playlistFile, line) ){

        if( line.find("#EXTINF") != string::npos ){
            const string::size_type afterSemicolonPos = line.find(":") + 1;
            const string tsDurationStr = line.substr( afterSemicolonPos, line.size() - afterSemicolonPos - 1 ); // 1: without comma at the end
            tsTotalDuration += std::stod( tsDurationStr );
        }
    }

    int64_t out = tsTotalDuration;
    return out;
}

objrepr::SpatialObjectPtr ObjreprRepository::getSensorObjreprObject( const std::string & _sourceUrl ){

    // NOTE: if multiple cameras with same URL - error
    objrepr::SpatialObjectManager * objManager = objrepr::RepresentationServer::instance()->objectManager();
    objrepr::GlobalDataManager * globalManager = objrepr::RepresentationServer::instance()->globalManager();

    objrepr::SpatialObjectPtr out;

    vector<objrepr::ClassinfoPtr> classes = globalManager->classinfoListByCat( "sensor" );
    for( objrepr::ClassinfoPtr & currentClassinfo : classes ){

        vector<objrepr::SpatialObjectPtr> classinfoObjects =  objManager->getObjectsByClassinfo( currentClassinfo->id() );
        for( objrepr::SpatialObjectPtr & object : classinfoObjects ){

            objrepr::SensorObjectPtr sensor = boost::dynamic_pointer_cast<objrepr::SensorObject>( object );
            if( sensor ){
                if( string(sensor->dataUrl()) == _sourceUrl ){
                    if( out ){
                        VS_LOG_ERROR << "multiple cameras found with same URL: " << sensor->id() << " and " << out->id() << endl;
                        return nullptr;
                    }
                    else{
                        out = sensor;
                    }
                }
            }
        }
    }

    return out;
}

objrepr::SpatialObjectPtr ObjreprRepository::getSensorObjreprObject( TSensorId _sensorId ){

    objrepr::SpatialObjectManager * objManager = objrepr::RepresentationServer::instance()->objectManager();
    return objManager->getObject( _sensorId );
}

objrepr::SpatialObjectPtr ObjreprRepository::getArchiveObjreprObject( const std::string & _name, objrepr::SpatialObjectPtr _camera ){

    objrepr::SpatialObjectManager * objManager = objrepr::RepresentationServer::instance()->objectManager();
    objrepr::GlobalDataManager * globalManager = objrepr::RepresentationServer::instance()->globalManager();

    objrepr::ClassinfoPtr archiveClassinfo;

    // search archive classinfo
    vector<objrepr::ClassinfoPtr> classes = globalManager->classinfoListByCat( "container" );
    for( objrepr::ClassinfoPtr & currentClassinfo : classes ){

        if( string(currentClassinfo->name()) == ARCHIVE_INSTANCE_CLASSINFO_NAME ){
            archiveClassinfo = currentClassinfo;
        }
    }

    if( ! archiveClassinfo ){
        VS_LOG_CRITICAL << "objrepr video-archive-info classinfo not found"
                     << endl;
        return nullptr;
    }

    objrepr::SpatialObjectPtr out;
    vector<objrepr::SpatialObjectPtr> mirrorChilds =  objManager->getObjectsByParent( _camera->id() );
    for( objrepr::SpatialObjectPtr & child : mirrorChilds ){

        if( child->name() == _name ){
            out = child;

            objrepr::TimeAttributePtr timeBeginAttr = boost::dynamic_pointer_cast<objrepr::TimeAttribute>( out->attrMap()->getAttr(ARCHIVE_INSTANCE_ATTR_TIME_BEGIN.c_str()) );
            if( ! timeBeginAttr ){
                VS_LOG_CRITICAL << "objrepr video-archive-info instance attr not found: " << ARCHIVE_INSTANCE_ATTR_TIME_BEGIN
                             << endl;
                return nullptr;
            }

            objrepr::TimeAttributePtr timeEndAttr = boost::dynamic_pointer_cast<objrepr::TimeAttribute>( out->attrMap()->getAttr(ARCHIVE_INSTANCE_ATTR_TIME_END.c_str()) );
            if( ! timeEndAttr ){
                VS_LOG_CRITICAL << "objrepr video-archive-info instance attr not found: " << ARCHIVE_INSTANCE_ATTR_TIME_END
                             << endl;
                return nullptr;
            }

            objrepr::StringAttributePtr playlistPathAttr = boost::dynamic_pointer_cast<objrepr::StringAttribute>( out->attrMap()->getAttr(ARCHIVE_INSTANCE_ATTR_PLAYLIST_URL.c_str()) );
            if( ! playlistPathAttr ){
                VS_LOG_CRITICAL << "objrepr video-archive-info instance attr not found: " << ARCHIVE_INSTANCE_ATTR_PLAYLIST_URL
                             << endl;
                return nullptr;
            }

            VS_LOG_INFO << "Archive found [" << _name << "] with following attributes:"
                     << " [" << ARCHIVE_INSTANCE_ATTR_TIME_BEGIN << "] = " << timeBeginAttr->valueAsString()
                     << " [" << ARCHIVE_INSTANCE_ATTR_TIME_END << "] = " << timeEndAttr->valueAsString()
                     << " [" << ARCHIVE_INSTANCE_ATTR_PLAYLIST_URL << "] = " << playlistPathAttr->valueAsString()
                     << endl;
        }
    }

    if( ! out ){
        out = objManager->addObject( archiveClassinfo->id(), _camera->id() );
        if( ! out ){
            VS_LOG_CRITICAL << "objrepr video-archive-info instance creation failed"
                         << endl;
            return nullptr;
        }

        out->setName( _name.c_str() );
        VS_LOG_INFO << "Archive not found [" << _name << "] - create new" << endl;
    }

    return out;
}

bool ObjreprRepository::updateArchiveStatus( const SHlsMetadata & _metadata, bool _begin ){

    objrepr::SpatialObjectManager * objManager = objrepr::RepresentationServer::instance()->objectManager();

    // inside that object will be created/modified archive info
    objrepr::SpatialObjectPtr camera = ( _metadata.sensorId != 0 ? getSensorObjreprObject(_metadata.sensorId)
                                                                 : getSensorObjreprObject(_metadata.sourceUrl) );
    if( ! camera ){
        VS_LOG_ERROR << "objrepr-sensor not found for sensor id [" << _metadata.sensorId
                  << "] or source url [" << _metadata.sourceUrl << "]"
                  << endl;
        return false;
    }
//    else{
//        LOG_INFO << "found sensor [" << camera->name()
//                 << "] with id [" << camera->id()
//                 << "] for source url [" << _metadata.sourceUrl << "]"
//                 << endl;
//    }

    // get archive
    const string archiveName = ARCHIVE_NAME_PREFIX + "_" + common_utils::timeMillisecToStr( _metadata.timeStartMillisec );
    objrepr::SpatialObjectPtr archiveObject = getArchiveObjreprObject( archiveName, camera );
    if( ! archiveObject ){
        VS_LOG_ERROR << "objrepr-archive not found for source url [" << _metadata.sourceUrl << "]"
                  << endl;
        return false;
    }

    // modify info
    VS_LOG_TRACE << "Set attributes to:";
    if( _begin ){
        VS_LOG_TRACE << " [" << ARCHIVE_INSTANCE_ATTR_TIME_BEGIN << "] to = [" << common_utils::timeMillisecToStr( _metadata.timeStartMillisec ) << "]";

        objrepr::TimeAttributePtr timeBeginAttr = boost::dynamic_pointer_cast<objrepr::TimeAttribute>( archiveObject->attrMap()->getAttr(ARCHIVE_INSTANCE_ATTR_TIME_BEGIN.c_str()) );
        if( ! timeBeginAttr ){
            VS_LOG_CRITICAL << "objrepr video-archive-info instance attr not found: " << ARCHIVE_INSTANCE_ATTR_TIME_BEGIN
                         << endl;
            return false;
        }
        const bool rt = timeBeginAttr->setValue( (_metadata.timeStartMillisec / 1000) );

        VS_LOG_TRACE << " [" << ARCHIVE_INSTANCE_ATTR_TIME_END << "] to = [" << (uint64_t)0 << "]";

        objrepr::TimeAttributePtr timeEndAttr = boost::dynamic_pointer_cast<objrepr::TimeAttribute>( archiveObject->attrMap()->getAttr(ARCHIVE_INSTANCE_ATTR_TIME_END.c_str()) );
        if( ! timeEndAttr ){
            VS_LOG_CRITICAL << "objrepr video-archive-info instance attr not found: " << ARCHIVE_INSTANCE_ATTR_TIME_END
                         << endl;
            return false;
        }
        const bool rt2 = timeEndAttr->setValue( (uint64_t)0 );
    }

    if( ! _begin ){
        VS_LOG_TRACE << " [" << ARCHIVE_INSTANCE_ATTR_TIME_END << "] to = [" << common_utils::timeMillisecToStr( _metadata.timeEndMillisec ) << "]";

        objrepr::TimeAttributePtr timeEndAttr = boost::dynamic_pointer_cast<objrepr::TimeAttribute>( archiveObject->attrMap()->getAttr(ARCHIVE_INSTANCE_ATTR_TIME_END.c_str()) );
        if( ! timeEndAttr ){
            VS_LOG_CRITICAL << "objrepr video-archive-info instance attr not found: " << ARCHIVE_INSTANCE_ATTR_TIME_END
                         << endl;
            return false;
        }
        const bool rt = timeEndAttr->setValue( (_metadata.timeEndMillisec / 1000) );
    }

    if( ! _metadata.playlistWebLocationFullPath.empty() ){
        VS_LOG_TRACE << " [" << ARCHIVE_INSTANCE_ATTR_PLAYLIST_URL << "] to = [" << _metadata.playlistWebLocationFullPath << "]";

        objrepr::StringAttributePtr playlistPathAttr = boost::dynamic_pointer_cast<objrepr::StringAttribute>( archiveObject->attrMap()->getAttr(ARCHIVE_INSTANCE_ATTR_PLAYLIST_URL.c_str()) );
        if( ! playlistPathAttr ){
            VS_LOG_CRITICAL << "objrepr video-archive-info instance attr not found: " << ARCHIVE_INSTANCE_ATTR_PLAYLIST_URL
                         << endl;
            return false;
        }
        const bool rt = playlistPathAttr->setValue( _metadata.playlistWebLocationFullPath.c_str() );
    }

    if( 0 == _metadata.timeStartMillisec && 0 == _metadata.timeEndMillisec && _metadata.playlistWebLocationFullPath.empty() ){
        VS_LOG_TRACE << " no updates";
    }
    VS_LOG_TRACE << endl;

    if( ! objManager->commitObject(archiveObject->id()) ){
        VS_LOG_ERROR << "objrepr video-archive-info instance commit failed"
                  << endl;
        return false;
    }

    return true;
}

bool ObjreprRepository::setArchiveLiveStreamPlaylist( const SHlsMetadata & _metadata, const std::string & _playlistUrlFullPath ){

    objrepr::SpatialObjectManager * objManager = objrepr::RepresentationServer::instance()->objectManager();

    // inside that object will be created/modified archive info
    objrepr::SpatialObjectPtr camera = ( _metadata.sensorId != 0 ? getSensorObjreprObject(_metadata.sensorId)
                                                                 : getSensorObjreprObject(_metadata.sourceUrl) );
    if( ! camera ){
        VS_LOG_ERROR << "objrepr-sensor not found for sensor id [" << _metadata.sensorId
                  << "] or source url [" << _metadata.sourceUrl << "]"
                  << endl;
        return false;
    }
//    else{
//        LOG_INFO << "found sensor [" << camera->name()
//                 << "] with id [" << camera->id()
//                 << "] for source url [" << _metadata.sourceUrl << "]"
//                 << endl;
//    }

    // get archive
    const string archiveName = ARCHIVE_NAME_PREFIX + "_" + common_utils::timeMillisecToStr( _metadata.timeStartMillisec );
    objrepr::SpatialObjectPtr archiveObject = getArchiveObjreprObject( archiveName, camera );
    if( ! archiveObject ){
        VS_LOG_ERROR << "objrepr-archive not found for source url [" << _metadata.sourceUrl << "]"
                  << endl;
        return false;
    }

    // set playlist path
    VS_LOG_TRACE << " [" << ARCHIVE_INSTANCE_ATTR_LIVE_STREAM_PLAYLIST_URL << "] to = [" << _playlistUrlFullPath << "]";

    objrepr::StringAttributePtr liveStreamPlaylistPathAttr = boost::dynamic_pointer_cast<objrepr::StringAttribute>( archiveObject->attrMap()->getAttr(ARCHIVE_INSTANCE_ATTR_LIVE_STREAM_PLAYLIST_URL.c_str()) );
    if( ! liveStreamPlaylistPathAttr ){
        VS_LOG_CRITICAL << "objrepr video-archive-info instance attr not found: " << ARCHIVE_INSTANCE_ATTR_LIVE_STREAM_PLAYLIST_URL
                     << endl;
        return false;
    }
    const bool rt = liveStreamPlaylistPathAttr->setValue( _playlistUrlFullPath.c_str() );

    if( ! objManager->commitObject(archiveObject->id()) ){
        VS_LOG_ERROR << "objrepr video-archive-info instance commit failed"
                  << endl;
        return false;
    }

    return true;
}

bool ObjreprRepository::removeArchive( int64_t _archiveBeginTimeSec ){

    // TODO: ?
}

static bool isFileEmpty( const std::string & _videoLocation ){

    ifstream file( _videoLocation, std::ios::in );
    if( file.is_open() ){
        file.seekg( 0, ios::end );
        const ifstream::pos_type fileSize = file.tellg();
        return ( 0 == fileSize );
    }
    else{
        VS_LOG_ERROR << PRINT_HEADER << " file doesn't exist [" << _videoLocation << "]" << endl;
        return true;
    }
}

bool ObjreprRepository::writeProcessingObjectVideoAttr( TObjectId _objId, const std::string & _videoLocation ){
    static const std::string OBJECT_INSTANCE_ATTR_VIDEO = "video";

    if( ! isFileEmpty(_videoLocation) ){
        VS_LOG_INFO << PRINT_HEADER << " save for object [" << _objId << "] video file [" << _videoLocation << "]" << endl;
    }
    else{
        VS_LOG_ERROR << PRINT_HEADER
                     << " ABORT saving for object [" << _objId << "]"
                     << " video file [" << _videoLocation << "]"
                     << " file is EMPTY"
                     << endl;
        return false;
    }

    objrepr::SpatialObjectManager * objManager = objrepr::RepresentationServer::instance()->objectManager();
    objrepr::SpatialObjectPtr obj = objManager->getObject( _objId );

    // set file path
    objrepr::FileAttributePtr videoFullPathAttr = boost::dynamic_pointer_cast<objrepr::FileAttribute>( obj->attrMap()->getAttr(OBJECT_INSTANCE_ATTR_VIDEO.c_str()) );
    if( ! videoFullPathAttr ){
        VS_LOG_ERROR   << PRINT_HEADER
                    << " objrepr object instance attr not found [" << OBJECT_INSTANCE_ATTR_VIDEO << "]"
                    << endl;
        return false;
    }

    const bool rt = videoFullPathAttr->setValue( _videoLocation.c_str() );
    if( ! rt ){
        VS_LOG_ERROR   << PRINT_HEADER
                    << " video file not catched by objrepr [" << _videoLocation << "]"
                    << endl;
        return false;
    }

    if( ! objManager->commitObject(obj->id()) ){
        VS_LOG_ERROR   << PRINT_HEADER
                    << " objrepr object instance commit failed, id [" << obj->id() << "]"
                    << endl;
        return false;
    }

    //
    const string objreprVideoFilePath = string(objrepr::RepresentationServer::instance()->globalManager()->mediaPath())
            + string("/")
            + videoFullPathAttr->valueAsString();

    VS_LOG_INFO << PRINT_HEADER << " file path by objrepr: " << objreprVideoFilePath << endl;
    if( isFileEmpty(objreprVideoFilePath) ){
        VS_LOG_ERROR << PRINT_HEADER
                     << " POSTCHECK for object [" << _objId << "]"
                     << " video file [" << objreprVideoFilePath << "]"
                     << " file is EMPTY"
                     << endl;
    }

    return true;
}

bool ObjreprRepository::writeSensorRetranslateUrl( TSensorId _sensorId, const std::string & _streamUrl ){
    static const std::string SENSOR_RETRANSLATE_URL_ATTR = "proxy_url";

    VS_LOG_TRACE << PRINT_HEADER << " set to sensor [" << _sensorId << "] retranslate url [" << _streamUrl << "]" << endl;

    objrepr::SpatialObjectManager * objManager = objrepr::RepresentationServer::instance()->objectManager();
    objrepr::SpatialObjectPtr sensorObj = objManager->getObject( _sensorId );

    // set file path
    objrepr::FileAttributePtr retranslateUrlAttr = boost::dynamic_pointer_cast<objrepr::FileAttribute>( sensorObj->attrMap()->getAttr(SENSOR_RETRANSLATE_URL_ATTR.c_str()) );
    if( ! retranslateUrlAttr ){
        VS_LOG_WARN   << PRINT_HEADER
                    << " objrepr object instance attr not found [" << SENSOR_RETRANSLATE_URL_ATTR << "]"
                    << endl;
        return false;
    }

    const bool rt = retranslateUrlAttr->setValue( _streamUrl.c_str() );
    if( ! rt ){
        VS_LOG_ERROR   << PRINT_HEADER
                    << " retranslate url not setted by objrepr [" << _streamUrl << "]"
                    << endl;
        return false;
    }

    if( ! objManager->commitObject(sensorObj->id()) ){
        VS_LOG_ERROR   << PRINT_HEADER
                    << " objrepr object instance commit failed, id [" << sensorObj->id() << "]"
                    << endl;
        return false;
    }

    return true;
}

void ObjreprRepository::writeAnalytic( const SAnalyticEvent & _event ){

    m_mutexEventsLock.lock();
    m_analyticEvents.push( _event);
    m_mutexEventsLock.unlock();


}

void ObjreprRepository::threadAnalyticEventsProcessing(){

    VS_LOG_INFO << PRINT_HEADER << " start an events processing THREAD" << endl;

    constexpr int listenIntervalMillisec = 10;

    while( ! m_shutdownCalled ){

        SAnalyticEvent event;

        // get event
        m_mutexEventsLock.lock();
        if( ! m_analyticEvents.empty() ){
            event = m_analyticEvents.front();
            m_analyticEvents.pop();
        }
        m_mutexEventsLock.unlock();

        // process


        this_thread::sleep_for( chrono::milliseconds(listenIntervalMillisec) );
    }

    VS_LOG_INFO << PRINT_HEADER << " events processing THREAD is stopped" << endl;
}

TClassinfoNamesIds ObjreprRepository::getVideoTypesClassinfoIds(){

    string attrType = m_settings.eventType;// = GET_CMD_LINE_VALUE( "attr-event-type", "Тип события" );
    TClassinfoNamesIds videoTypesClassinfoIds;

    objrepr::GlobalDataManager * globMgr = objrepr::RepresentationServer::instance()->globalManager();
    for( objrepr::ClassinfoPtr classinfo : globMgr->classinfoListByCat( "video" ) ){

        objrepr::AttributePtr attr = classinfo->attrMap()->getAttr( attrType.c_str() );

        if( attr ){
            string attrValue = attr->valueAsString();
            videoTypesClassinfoIds[ attrValue ] = classinfo->id();
        }
    }

    return videoTypesClassinfoIds;
}

TSensorNamesIds ObjreprRepository::getSensorUrls(){

    TSensorNamesIds sensorUrlIds;
    string attrSourceName = m_settings.videoSource;// = GET_CMD_LINE_VALUE( "attr-video-source", "Источник видео" );

    auto globMgr = objrepr::RepresentationServer::instance()->globalManager();
    auto objMgr = objrepr::RepresentationServer::instance()->objectManager();

    for( objrepr::ClassinfoPtr classinfo : globMgr->classinfoListByCat( "sensor" ) ){

        auto sensorObjects = objMgr->getObjectsByClassinfo( classinfo->id() );
        for( objrepr::SpatialObjectPtr object : sensorObjects ){

            objrepr::SensorObjectPtr sensorObj = boost::dynamic_pointer_cast< objrepr::SensorObject >( object );
            if( ! sensorObj ){
                continue;
            }

            auto attrSource = sensorObj->attrMap()->getAttr( attrSourceName.data() );
            if( ! attrSource ){
                continue;
            }

            const string source = attrSource->valueAsString();
            if( source.empty() ){
                continue;
            }

            sensorUrlIds[ source ] = sensorObj->id();
        }
    }

    return sensorUrlIds;
}

bool ObjreprRepository::saveVideo( uint64_t _classinfoId, uint64_t _parentId, std::string _path, std::string _json ){

    objrepr::SpatialObjectManager * objManager = objrepr::RepresentationServer::instance()->objectManager();

    string attr_message_name = m_settings.message;// = GET_CMD_LINE_VALUE( "attr-message", "Сообщение" );

    objrepr::VideoObjectPtr videoObject = dynamic_pointer_cast<objrepr::VideoObject>( objManager->addObject(_classinfoId, _parentId) );
    if( ! videoObject ){
        return false;
    }

    objrepr::AttributePtr attr_message = videoObject->attrMap()->getAttr( attr_message_name.data() );
    if( attr_message ){
        attr_message->setValue( _json.data() );
    }

    videoObject->setVideoUrl( _path.data() );

    return objManager->commitObject( videoObject->id() );
}

std::vector<std::string> ObjreprRepository::getHlsChunkFileNames(){

    std::vector<std::string> out;





    return out;
}
