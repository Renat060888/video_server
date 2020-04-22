#ifndef DATABASE_MANAGER_ASTRA_H
#define DATABASE_MANAGER_ASTRA_H

#include <unordered_map>

#include <mongoc.h>

#include "common/common_types.h"

class DatabaseManager
{
    static void systemInit();

    static bool m_systemInited;
    static int m_instanceCounter;
public:
    static const std::string ALL_CLIENT_OPERATIONS;
    static const common_types::TPid ALL_PROCESS_EVENTS;

    struct SInitSettings {
        SInitSettings() :
            port(MONGOC_DEFAULT_PORT)
        {}
        std::string host;
        uint16_t port;
        std::string databaseName;
    };

    static DatabaseManager * getInstance(){
        if( ! m_systemInited ){
            systemInit();
            m_systemInited = true;
        }
        m_instanceCounter++;
        return new DatabaseManager();
    }

    static void destroyInstance( DatabaseManager * & _inst ){
        delete _inst;
        _inst = nullptr;
        m_instanceCounter--;
    }

    bool init( SInitSettings _settings );

    // TODO: interfaces for various purposes - IVideoMetadataStore, IAnalyzeResultStore, IWALStore, ...

    // service
    bool writeContextInfo( common_types::TContextId _ctxId );
    common_types::TContextId readContextInfo();
    void removeContextInfo();

    // storage
    bool writeHlsMetadata( const common_types::SHlsMetadata & _metadata );
    std::vector<common_types::SHlsMetadata> readHlsMetadata();
    bool removeHlsStream( const common_types::SHlsMetadata & _metadata );

    // bookmark
    bool insertBookmark( const common_types::SBookmark & _bookmark );
    bool deleteBookmark( const common_types::SBookmark & _bookmark );
    std::vector<common_types::SBookmark> getBookmarks();

    // analyze - data
    bool writeAnalyticEvent( const common_types::SAnalyticEvent & _analyticEvents );
    common_types::SEventFromStorage readAnalyticEventData( const common_types::SAnalyticFilter & _filter );
    void removeTotalAnalyticEventData( const common_types::SAnalyticFilter & _filter );
    // analyze - metadata
    std::vector<common_types::SEventsSessionInfo> getAnalyticEventMetadata( common_types::TContextId _ctxId, common_types::TSensorId _sensorId );
    std::unordered_map<common_types::TSensorId, common_types::SProcessedSensor> getProcessedSensors( common_types::TContextId _ctxId );
    bool writeProcessedSensor( const common_types::TContextId _ctxId, const common_types::SProcessedSensor & _sensor );
    void removeProcessedSensor( const common_types::TContextId _ctxId, const common_types::TSensorId _sensorId );


private:    
    DatabaseManager();
    ~DatabaseManager();

    DatabaseManager( const DatabaseManager & _inst ) = delete;
    DatabaseManager & operator=( const DatabaseManager & _inst ) = delete;

    inline mongoc_collection_t * getAnalyticContextTable( common_types::TContextId _ctxId, common_types::TSensorId _sensorId );
    inline std::string getTableName( common_types::TContextId _ctxId, common_types::TSensorId _sensorId );
    inline bool createIndex( const std::string & _tableName, const std::vector<std::string> & _fieldNames );

    std::vector<common_types::SObjectStep> getSessionSteps( common_types::TContextId _ctxId, common_types::TSensorId _sensorId, common_types::TSessionNum _sesNum );


    // data
    mongoc_collection_t * m_tableService;
    mongoc_collection_t * m_tableHlsMetadata;
    mongoc_collection_t * m_tableAnalyticEvents;    
    mongoc_collection_t * m_tableAnalyticMetadata;
    std::vector<mongoc_collection_t *> m_allCollections;
    std::unordered_map<common_types::TContextId, mongoc_collection_t *> m_contextCollections;
    SInitSettings m_settings;


    // service
    mongoc_client_t * m_mongoClient;
    mongoc_database_t * m_database;

};

#endif // DATABASE_MANAGER_ASTRA_H
