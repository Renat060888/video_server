#ifndef OBJREPR_INTERFACE_H
#define OBJREPR_INTERFACE_H

#include <string>
#include <queue>
#include <mutex>
#include <thread>
#include <map>

#include <objrepr/reprServer.h>

#include "common/common_types.h"

using TClassinfoNamesIds = std::map<std::string, uint64_t>;
using TSensorNamesIds = std::map<std::string, uint64_t>;

class ObjreprRepository
{
public:
    struct SInitSettings {                
        std::string eventType;
        std::string videoSource;
        std::string message;
    };

    ObjreprRepository();
    ~ObjreprRepository();

    bool init( SInitSettings _settings );
    const std::string & getLastError(){ return m_lastError; }

    // archive
    bool updateArchiveStatus( const common_types::SHlsMetadata & _metadata, bool _begin );
    bool setArchiveLiveStreamPlaylist( const common_types::SHlsMetadata & _metadata, const std::string & _playlistUrlFullPath );
    bool removeArchive( int64_t _archiveBeginTimeSec );

    // analyze
    bool writeProcessingObjectVideoAttr( common_types::TObjectId _objId, const std::string & _videoLocation );
    void writeAnalytic( const common_types::SAnalyticEvent & _event );

    // retranslate
    bool writeSensorRetranslateUrl( common_types::TSensorId _sensorId, const std::string & _streamUrl );

private:
    void threadAnalyticEventsProcessing();    
    std::vector<std::string> getHlsChunkFileNames();

    // archive
    void repairPossibleCorruptedArchives();
    void checkForDead( objrepr::SpatialObjectPtr & _archive );
    void checkForInconsistency( objrepr::SpatialObjectPtr & _archive );
    int64_t getTotalTsDurationSecFromPlaylist( const std::string & _playlistFullPath );

    TClassinfoNamesIds getVideoTypesClassinfoIds();
    TSensorNamesIds getSensorUrls();
    bool saveVideo( uint64_t _classinfoId, uint64_t _parentId, std::string _path, std::string _json );    

    objrepr::SpatialObjectPtr getSensorObjreprObject( const std::string & _sourceUrl );
    objrepr::SpatialObjectPtr getSensorObjreprObject( common_types::TSensorId _sensorId );
    objrepr::SpatialObjectPtr getArchiveObjreprObject( const std::string & _name, objrepr::SpatialObjectPtr _camera );


    ObjreprRepository( const ObjreprRepository & _inst ) = delete;
    ObjreprRepository & operator=( const ObjreprRepository & _inst ) = delete;



    // data
    SInitSettings m_settings;
    bool m_shutdownCalled;
    std::string m_lastError;

    // service
    std::thread * m_threadAnalyticEventsProcessing;
    std::queue<common_types::SAnalyticEvent> m_analyticEvents;
    std::mutex m_mutexEventsLock;    
};

#endif // OBJREPR_INTERFACE_H

