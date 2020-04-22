
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <microservice_common/system/logger.h>

#include "system/config_reader.h"
#include "common/common_vars.h"
#include "common/common_utils.h"
#include "database_manager.h"

using namespace std;
using namespace common_vars;
using namespace common_types;

static constexpr const char * PRINT_HEADER = "DatabaseMgr:";
static const string ARGS_DELIMETER = "$";

bool DatabaseManager::m_systemInited = false;
int DatabaseManager::m_instanceCounter = 0;
const std::string DatabaseManager::ALL_CLIENT_OPERATIONS = "";
const TPid DatabaseManager::ALL_PROCESS_EVENTS = 0;

// TODO: need ?
static string convertProcessArgsToStr( const std::vector<string> & _args ){

    string out = boost::algorithm::join( _args, ARGS_DELIMETER );

    return out;
}

static std::vector<string> convertProcessArgsFromStr( const string & _argsStr ){

    std::vector<string> out;
    boost::algorithm::split( out, _argsStr, boost::is_any_of(ARGS_DELIMETER) );

    return out;
}

DatabaseManager::DatabaseManager()
    : m_mongoClient(nullptr)
    , m_database(nullptr)
    , m_tableHlsMetadata(nullptr)
    , m_tableAnalyticEvents(nullptr)
    , m_tableAnalyticMetadata(nullptr)
{

}

DatabaseManager::~DatabaseManager()
{
    // TODO: do
//    mongoc_cleanup();

    for( mongoc_collection_t * collect : m_allCollections ){
        mongoc_collection_destroy( collect );
    }
    mongoc_database_destroy( m_database );
    mongoc_client_destroy( m_mongoClient );
}

void DatabaseManager::systemInit(){

    mongoc_init();    
    VS_LOG_INFO << PRINT_HEADER << " init success" << endl;
    m_systemInited = true;
}

// -------------------------------------------------------------------------------------
// service
// -------------------------------------------------------------------------------------
bool DatabaseManager::init( SInitSettings _settings ){

    m_settings = _settings;

    // init mongo
    const mongoc_uri_t * uri = mongoc_uri_new_for_host_port( _settings.host.c_str(), _settings.port );
    if( ! uri ){
        VS_LOG_ERROR << PRINT_HEADER << " mongo URI creation failed by host: " << _settings.host << endl;
        return false;
    }

    m_mongoClient = mongoc_client_new_from_uri( uri );
    if( ! m_mongoClient ){
        VS_LOG_ERROR << PRINT_HEADER << " mongo connect failed to: " << _settings.host << endl;
        return false;
    }

    m_database = mongoc_client_get_database( m_mongoClient, _settings.databaseName.c_str() );

    m_tableHlsMetadata = mongoc_client_get_collection( m_mongoClient,
        _settings.databaseName.c_str(),
        (string("video_server_") + mongo_fields::archiving::COLLECTION_NAME).c_str() );

    m_tableAnalyticEvents = mongoc_client_get_collection( m_mongoClient,
        _settings.databaseName.c_str(),
        (string("video_server_") + mongo_fields::analytic::COLLECTION_NAME).c_str() );

    m_tableService = mongoc_client_get_collection( m_mongoClient,
        _settings.databaseName.c_str(),
        (string("video_server_") + mongo_fields::service::COLLECTION_NAME).c_str() );

    m_tableAnalyticMetadata = mongoc_client_get_collection( m_mongoClient,
        _settings.databaseName.c_str(),
        (string("video_server_") + mongo_fields::analytic::metadata::COLLECTION_NAME).c_str() );

    m_allCollections.push_back( m_tableHlsMetadata );
    m_allCollections.push_back( m_tableAnalyticEvents );
    m_allCollections.push_back( m_tableService );
    m_allCollections.push_back( m_tableAnalyticMetadata );

    VS_LOG_INFO << PRINT_HEADER << " instance connected to [" << _settings.host << "]" << endl;

    return true;
}

inline mongoc_collection_t * DatabaseManager::getAnalyticContextTable( TContextId _ctxId, TSensorId _sensorId ){

    assert( _ctxId > 0 && _sensorId > 0 );

    mongoc_collection_t * contextTable = nullptr;
    auto iter = m_contextCollections.find( _ctxId );
    if( iter != m_contextCollections.end() ){
        contextTable = iter->second;
    }
    else{
        const string tableName = getTableName(_ctxId, _sensorId);
        contextTable = mongoc_client_get_collection(    m_mongoClient,
                                                        m_settings.databaseName.c_str(),
                                                        tableName.c_str() );

        createIndex( tableName, {mongo_fields::analytic::detected_object::SESSION,
                                 mongo_fields::analytic::detected_object::LOGIC_TIME}
                   );

        // TODO: add record to context info

        m_contextCollections.insert( {_ctxId, contextTable} );
    }

    return contextTable;
}

inline string DatabaseManager::getTableName( TContextId _ctxId, TSensorId _sensorId ){

    const string name = string("video_server_") +
                        mongo_fields::analytic::COLLECTION_NAME +
                        "_" +
                        std::to_string(_ctxId) +
                        "_" +
                        std::to_string(_sensorId);

    return name;
}

inline bool DatabaseManager::createIndex( const std::string & _tableName, const std::vector<std::string> & _fieldNames ){

    //
    bson_t keys;
    bson_init( & keys );

    for( const string & key : _fieldNames ){
        BSON_APPEND_INT32( & keys, key.c_str(), 1 );
    }

    //
    char * indexName = mongoc_collection_keys_to_index_string( & keys );
    bson_t * createIndex = BCON_NEW( "createIndexes",
                                     BCON_UTF8(_tableName.c_str()),
                                     "indexes", "[",
                                         "{", "key", BCON_DOCUMENT(& keys),
                                              "name", BCON_UTF8(indexName),
                                         "}",
                                     "]"
                                );

    //
    bson_t reply;
    bson_error_t error;
    const bool rt = mongoc_database_command_simple( m_database,
                                                    createIndex,
                                                    NULL,
                                                    & reply,
                                                    & error );


    if( ! rt ){
        VS_LOG_ERROR << PRINT_HEADER << " index creation failed, reason: " << error.message << endl;
        bson_destroy( createIndex );

        // TODO: remove later
        assert( rt );

        return false;
    }

    bson_destroy( createIndex );
    return false;
}

bool DatabaseManager::writeContextInfo( TContextId _ctxId ){

    if( common_vars::INVALID_CONTEXT_ID == _ctxId ){
        VS_LOG_ERROR << PRINT_HEADER << " invalid context id to write [" << _ctxId << "]" << endl;
        return false;
    }

    bson_t * doc = BCON_NEW( mongo_fields::service::LAST_LOADED_CONTEXT_ID.c_str(), BCON_INT64( _ctxId ) );

    bson_error_t error;
    const bool rt = mongoc_collection_insert( m_tableService,
                                              MONGOC_INSERT_NONE,
                                              doc,
                                              NULL,
                                              & error );

    if( 0 == rt ){
        VS_LOG_ERROR << PRINT_HEADER << " process event write failed, reason: " << error.message << endl;
        bson_destroy( doc );
        return false;
    }

    bson_destroy( doc );
    return true;
}

TContextId DatabaseManager::readContextInfo(){

    bson_t * query = BCON_NEW( nullptr );

    mongoc_cursor_t * cursor = mongoc_collection_find(  m_tableService,
                                                        MONGOC_QUERY_NONE,
                                                        0,
                                                        0,
                                                        1000000, // 10000 ~= inf
                                                        query,
                                                        nullptr,
                                                        nullptr );

    TContextId ctx = common_vars::INVALID_CONTEXT_ID;
    const bson_t * doc;
    while( mongoc_cursor_next( cursor, & doc ) ){

        bson_iter_t iter;

        bson_iter_init_find( & iter, doc, mongo_fields::service::LAST_LOADED_CONTEXT_ID.c_str() );
        ctx = bson_iter_int64( & iter );
    }

    mongoc_cursor_destroy( cursor );
    bson_destroy( query );

    return ctx;
}

void DatabaseManager::removeContextInfo(){

    bson_t * query = BCON_NEW( nullptr );
    const bool rt = mongoc_collection_remove( m_tableService, MONGOC_REMOVE_NONE, query, nullptr, nullptr );
    bson_destroy( query );
}

// -------------------------------------------------------------------------------------
// archive
// -------------------------------------------------------------------------------------
bool DatabaseManager::writeHlsMetadata( const SHlsMetadata & _metadata ){

    bson_t * query = BCON_NEW( mongo_fields::archiving::TAG.c_str(), BCON_UTF8( _metadata.tag.c_str() ) );
    bson_t * update = BCON_NEW( "$set", "{",
                                        mongo_fields::archiving::TAG.c_str(), BCON_UTF8( _metadata.tag.data() ),
                                        mongo_fields::archiving::SOURCE.c_str(), BCON_UTF8( _metadata.sourceUrl.c_str() ),
                                        mongo_fields::archiving::PLAYLIST_PATH.c_str(), BCON_UTF8( _metadata.playlistDirLocationFullPath.c_str() ),
                                        mongo_fields::archiving::START_TIME.c_str(), BCON_INT64( _metadata.timeStartMillisec ),
                                        mongo_fields::archiving::END_TIME.c_str(), BCON_INT64( _metadata.timeEndMillisec ),
                                   "}");

    const bool rt = mongoc_collection_update( m_tableHlsMetadata,
                                  MONGOC_UPDATE_UPSERT,
                                  query,
                                  update,
                                  NULL,
                                  NULL );

    if( ! rt ){
        // TODO: do
        return false;
    }

    bson_destroy( query );
    bson_destroy( update );

    return true;
}

std::vector<SHlsMetadata> DatabaseManager::readHlsMetadata(){

    bson_t * query = BCON_NEW( nullptr );

    mongoc_cursor_t * cursor = mongoc_collection_find(  m_tableHlsMetadata,
                                                        MONGOC_QUERY_NONE,
                                                        0,
                                                        0,
                                                        1000000, // 10000 ~= inf
                                                        query,
                                                        nullptr,
                                                        nullptr );

    std::vector<SHlsMetadata> out;
    const bson_t * doc;
    while( mongoc_cursor_next( cursor, & doc ) ){

        uint len;
        bson_iter_t iter;

        SHlsMetadata metadata;
        bson_iter_init_find( & iter, doc, mongo_fields::archiving::TAG.c_str() );
        metadata.tag = bson_iter_utf8( & iter, & len );
        bson_iter_init_find( & iter, doc, mongo_fields::archiving::SOURCE.c_str() );
        metadata.sourceUrl = bson_iter_utf8( & iter, & len );
        bson_iter_init_find( & iter, doc, mongo_fields::archiving::PLAYLIST_PATH.c_str() );
        metadata.playlistDirLocationFullPath = bson_iter_utf8( & iter, & len );
        bson_iter_init_find( & iter, doc, mongo_fields::archiving::END_TIME.c_str() );
        metadata.timeEndMillisec = bson_iter_int64( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::archiving::START_TIME.c_str() );
        metadata.timeStartMillisec = bson_iter_int64( & iter );

        out.push_back( metadata );
    }

    mongoc_cursor_destroy( cursor );
    bson_destroy( query );

    return out;
}

bool DatabaseManager::removeHlsStream( const SHlsMetadata & _metadata ){

    bson_t * query = BCON_NEW( "tag", BCON_UTF8( _metadata.tag.c_str() ));
    const bool rt = mongoc_collection_remove( m_tableHlsMetadata, MONGOC_REMOVE_NONE, query, nullptr, nullptr );
    bson_destroy( query );

    return rt;
}


// -------------------------------------------------------------------------------------
// bookmarks ( TODO: ? )
// -------------------------------------------------------------------------------------
bool DatabaseManager::insertBookmark( const SBookmark & _bookmark ){

}

bool DatabaseManager::deleteBookmark( const SBookmark & _bookmark ){

}

std::vector<SBookmark> DatabaseManager::getBookmarks(){

}


#include <jsoncpp/json/reader.h>

// -------------------------------------------------------------------------------------
// analytic events
// -------------------------------------------------------------------------------------
bool DatabaseManager::writeAnalyticEvent( const SAnalyticEvent & _analyticEvents ){

    //
    mongoc_collection_t * contextTable = getAnalyticContextTable( _analyticEvents.ctxId, _analyticEvents.sensorId );
    mongoc_bulk_operation_t * bulkedWrite = mongoc_collection_create_bulk_operation( contextTable, false, NULL );

    //
    Json::Reader jsonReader;
    Json::Value parsedRecord;
    if( ! jsonReader.parse( _analyticEvents.eventMessage.c_str(), parsedRecord, false ) ){
        VS_LOG_ERROR << "EventSendVisitor: parse failed of [1] Reason: [2] ["
                  << _analyticEvents.eventMessage << "] [" << jsonReader.getFormattedErrorMessages() << "]"
                  << endl;
        return false;
    }

    const int64_t astroTime = parsedRecord["astro_time"].asInt64();
    const TLogicStep logicTime = parsedRecord["logic_time"].asInt64();
    const TSessionNum session = parsedRecord["session"].asInt();

    //
    Json::Value newObjects = parsedRecord["new_objects"];
    for( Json::ArrayIndex i = 0; i < newObjects.size(); i++ ){
        Json::Value element = newObjects[ i ];

        const TObjectId id = element["objrepr_id"].asInt64();
        const int32_t state = element["state"].asInt();
        const float lat = element["lat"].asFloat();
        const float lon = element["lon"].asFloat();

        bson_t * doc = BCON_NEW( mongo_fields::analytic::detected_object::OBJRERP_ID.c_str(), BCON_INT64( id ),
                                 mongo_fields::analytic::detected_object::STATE.c_str(), BCON_INT32( state ),
                                 mongo_fields::analytic::detected_object::ASTRO_TIME.c_str(), BCON_INT64( astroTime ),
                                 mongo_fields::analytic::detected_object::LOGIC_TIME.c_str(), BCON_INT64( logicTime ),
                                 mongo_fields::analytic::detected_object::SESSION.c_str(), BCON_INT32( session ),
                                 mongo_fields::analytic::detected_object::LAT.c_str(), BCON_DOUBLE( lat ),
                                 mongo_fields::analytic::detected_object::LON.c_str(), BCON_DOUBLE( lon )
                               );

        mongoc_bulk_operation_insert( bulkedWrite, doc );
        bson_destroy( doc );
    }

    //
    Json::Value changedObjects = parsedRecord["changed_objects"];
    for( Json::ArrayIndex i = 0; i < changedObjects.size(); i++ ){
        Json::Value element = changedObjects[ i ];

        const TObjectId id = element["objrepr_id"].asInt64();
        const int32_t state = element["state"].asInt();
        const float lat = element["lat"].asFloat();
        const float lon = element["lon"].asFloat();

        bson_t * doc = BCON_NEW( mongo_fields::analytic::detected_object::OBJRERP_ID.c_str(), BCON_INT64( id ),
                                 mongo_fields::analytic::detected_object::STATE.c_str(), BCON_INT32( state ),
                                 mongo_fields::analytic::detected_object::ASTRO_TIME.c_str(), BCON_INT64( astroTime ),
                                 mongo_fields::analytic::detected_object::LOGIC_TIME.c_str(), BCON_INT64( logicTime ),
                                 mongo_fields::analytic::detected_object::SESSION.c_str(), BCON_INT32( session ),
                                 mongo_fields::analytic::detected_object::LAT.c_str(), BCON_DOUBLE( lat ),
                                 mongo_fields::analytic::detected_object::LON.c_str(), BCON_DOUBLE( lon )
                               );

        mongoc_bulk_operation_insert( bulkedWrite, doc );
        bson_destroy( doc );
    }

    //
    Json::Value disappearedObjects = parsedRecord["disappeared_objects"];
    for( Json::ArrayIndex i = 0; i < disappearedObjects.size(); i++ ){
        Json::Value element = disappearedObjects[ i ];

        const TObjectId id = element["objrepr_id"].asInt64();
        const int32_t state = element["state"].asInt();

        bson_t * doc = BCON_NEW( mongo_fields::analytic::detected_object::OBJRERP_ID.c_str(), BCON_INT64( id ),
                                 mongo_fields::analytic::detected_object::STATE.c_str(), BCON_INT32( state ),
                                 mongo_fields::analytic::detected_object::ASTRO_TIME.c_str(), BCON_INT64( astroTime ),
                                 mongo_fields::analytic::detected_object::LOGIC_TIME.c_str(), BCON_INT64( logicTime ),
                                 mongo_fields::analytic::detected_object::SESSION.c_str(), BCON_INT32( session )
                               );

        mongoc_bulk_operation_insert( bulkedWrite, doc );
        bson_destroy( doc );
    }

    bson_error_t error;
    const bool rt = mongoc_bulk_operation_execute( bulkedWrite, NULL, & error );
    if( 0 == rt ){
        VS_LOG_ERROR << PRINT_HEADER << " bulked process event write failed, reason: " << error.message << endl;
        mongoc_bulk_operation_destroy( bulkedWrite );
        return false;
    }
    mongoc_bulk_operation_destroy( bulkedWrite );

    return true;
}

SEventFromStorage DatabaseManager::readAnalyticEventData( const SAnalyticFilter & _filter ){

    mongoc_collection_t * contextTable = getAnalyticContextTable( _filter.ctxId, _filter.sensorId );
    assert( contextTable );

    bson_t * projection = nullptr;
    bson_t * query = nullptr;
    if( _filter.minLogicStep == _filter.maxLogicStep ){
        query = BCON_NEW( "$and", "[", "{", mongo_fields::analytic::detected_object::SESSION.c_str(), BCON_INT32(_filter.sessionId), "}",
                                       "{", mongo_fields::analytic::detected_object::LOGIC_TIME.c_str(), "{", "$eq", BCON_INT64(_filter.minLogicStep), "}", "}",
                                  "]"
                        );
    }
    else if( _filter.minLogicStep != 0 || _filter.maxLogicStep != 0 ){
        query = BCON_NEW( "$and", "[", "{", mongo_fields::analytic::detected_object::SESSION.c_str(), BCON_INT32(_filter.sessionId), "}",
                                       "{", mongo_fields::analytic::detected_object::LOGIC_TIME.c_str(), "{", "$gte", BCON_INT64(_filter.minLogicStep), "}", "}",
                                       "{", mongo_fields::analytic::detected_object::LOGIC_TIME.c_str(), "{", "$lte", BCON_INT64(_filter.maxLogicStep), "}", "}",
                                  "]"
                        );
    }
    else{
        query = BCON_NEW( nullptr );
    }

    mongoc_cursor_t * cursor = mongoc_collection_find(  contextTable,
                                                        MONGOC_QUERY_NONE,
                                                        0,
                                                        0,
                                                        1000000, // 10000 ~= inf
                                                        query,
                                                        projection,
                                                        nullptr );

    SEventFromStorage out;
    const uint32_t size = mongoc_cursor_get_batch_size( cursor );
//    out.objects.reserve( size );

    const bson_t * doc;
    while( mongoc_cursor_next( cursor, & doc ) ){
        bson_iter_t iter;

        SDetectedObject detectedObject;
        bson_iter_init_find( & iter, doc, mongo_fields::analytic::detected_object::OBJRERP_ID.c_str() );
        detectedObject.id = bson_iter_int64( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::analytic::detected_object::ASTRO_TIME.c_str() );
        detectedObject.timestampMillisec = bson_iter_int64( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::analytic::detected_object::LOGIC_TIME.c_str() );
        detectedObject.logicTime = bson_iter_int64( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::analytic::detected_object::SESSION.c_str() );
        detectedObject.session = bson_iter_int32( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::analytic::detected_object::STATE.c_str() );
        detectedObject.state = (SDetectedObject::EState)bson_iter_int32( & iter );

        if( detectedObject.state == SDetectedObject::EState::ACTIVE ){
            bson_iter_init_find( & iter, doc, mongo_fields::analytic::detected_object::LAT.c_str() );
            detectedObject.latDeg = bson_iter_double( & iter );
            bson_iter_init_find( & iter, doc, mongo_fields::analytic::detected_object::LON.c_str() );
            detectedObject.lonDeg = bson_iter_double( & iter );
            // TODO: heading
        }

        out.objects.push_back( detectedObject );
    }

    mongoc_cursor_destroy( cursor );
    bson_destroy( query );

    return out;
}

std::vector<SEventsSessionInfo> DatabaseManager::getAnalyticEventMetadata( TContextId _ctxId, TSensorId _sensorId ){

    bson_t * cmd = BCON_NEW(    "distinct", BCON_UTF8( getTableName(_ctxId, _sensorId).c_str() ),
                                "key", BCON_UTF8( mongo_fields::analytic::detected_object::SESSION.c_str() ),
                                "$sort", "{", "logic_time", BCON_INT32(-1), "}"
                            );

    bson_t reply;
    bson_error_t error;
    const bool rt = mongoc_database_command_simple( m_database,
                                                    cmd,
                                                    NULL,
                                                    & reply,
                                                    & error );

    // fill array with session numbers
    bson_iter_t iter;
    bson_iter_t arrayIter;

    if( ! (bson_iter_init_find( & iter, & reply, "values")
            && BSON_ITER_HOLDS_ARRAY( & iter )
            && bson_iter_recurse( & iter, & arrayIter ))
      ){
        VS_LOG_ERROR << PRINT_HEADER << "TODO: print" << endl;
        return std::vector<SEventsSessionInfo>();
    }

    // get info about each session
    std::vector<SEventsSessionInfo> out;

    while( bson_iter_next( & arrayIter ) ){
        if( BSON_ITER_HOLDS_INT32( & arrayIter ) ){
            const TSessionNum sessionNumber = bson_iter_int32( & arrayIter );

            SEventsSessionInfo info;
            info.number = sessionNumber;
            info.steps = getSessionSteps( _ctxId, _sensorId, sessionNumber );
            info.minLogicStep = info.steps.front().logicStep;
            info.maxLogicStep = info.steps.back().logicStep;
            info.minTimestampMillisec = info.steps.front().timestampMillisec;
            info.maxTimestampMillisec = info.steps.back().timestampMillisec;

            out.push_back( info );
        }
    }

    bson_destroy( cmd );
    bson_destroy( & reply );
    return out;
}

std::vector<SObjectStep> DatabaseManager::getSessionSteps( TContextId _ctxId, TSensorId _sensorId, TSessionNum _sesNum ){

    mongoc_collection_t * contextTable = getAnalyticContextTable( _ctxId, _sensorId );
    assert( contextTable );

    bson_t * pipeline = BCON_NEW( "pipeline", "[",
                                  "{", "$match", "{", "session", "{", "$eq", BCON_INT32(_sesNum), "}", "}", "}",
                                  "{", "$group", "{", "_id", "$logic_time", "maxAstroTime", "{", "$max", "$astro_time", "}", "}", "}",
                                  "{", "$project", "{", "_id", BCON_INT32(1), "maxAstroTime", BCON_INT32(1), "}", "}",
                                  "{", "$sort", "{", "_id", BCON_INT32(1), "}", "}",
                                  "]"
                                );

    mongoc_cursor_t * cursor = mongoc_collection_aggregate( contextTable,
                                                            MONGOC_QUERY_NONE,
                                                            pipeline,
                                                            nullptr,
                                                            nullptr );
    std::vector<SObjectStep> out;

    const bson_t * doc;
    while( mongoc_cursor_next( cursor, & doc ) ){
        bson_iter_t iter;

        SObjectStep step;

        bson_iter_init_find( & iter, doc, "_id" );
        step.logicStep = bson_iter_int64( & iter );

        bson_iter_init_find( & iter, doc, "maxAstroTime" );
        step.timestampMillisec = bson_iter_int64( & iter );

        out.push_back( step );
    }

    bson_destroy( pipeline );
    mongoc_cursor_destroy( cursor );

    return out;
}

void DatabaseManager::removeTotalAnalyticEventData( const SAnalyticFilter & _filter ){

    mongoc_collection_t * contextTable = getAnalyticContextTable( _filter.ctxId, _filter.sensorId );
    assert( contextTable );

    bson_t * query = BCON_NEW( nullptr );

    const bool result = mongoc_collection_remove( contextTable, MONGOC_REMOVE_NONE, query, nullptr, nullptr );

    bson_destroy( query );
}

std::unordered_map<TSensorId, SProcessedSensor> DatabaseManager::getProcessedSensors( TContextId _ctxId ){

    bson_t * query = BCON_NEW( mongo_fields::analytic::metadata::CTX_ID.c_str(), BCON_INT32(_ctxId) );

    mongoc_cursor_t * cursor = mongoc_collection_find(  m_tableAnalyticMetadata,
                                                        MONGOC_QUERY_NONE,
                                                        0,
                                                        0,
                                                        1000000, // 10000 ~= inf
                                                        query,
                                                        nullptr,
                                                        nullptr );

    std::unordered_map<TSensorId, SProcessedSensor> out;

    const bson_t * doc;
    while( mongoc_cursor_next( cursor, & doc ) ){

        bson_iter_t iter;

        SProcessedSensor sensor;
        bson_iter_init_find( & iter, doc, mongo_fields::analytic::metadata::SENSOR_ID.c_str() );
        sensor.id = bson_iter_int64( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::analytic::metadata::LAST_SESSION_ID.c_str() );
        sensor.lastSession = bson_iter_int32( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::analytic::metadata::UPDATE_STEP_MILLISEC.c_str() );
        sensor.updateStepMillisec = bson_iter_int64( & iter );

        out.insert( {sensor.id, sensor} );
    }

    mongoc_cursor_destroy( cursor );
    bson_destroy( query );

    return out;
}

bool DatabaseManager::writeProcessedSensor( const TContextId _ctxId, const SProcessedSensor & _sensor ){

    cout << PRINT_HEADER << "================== write sess: " << _sensor.lastSession << endl;

    bson_t * query = BCON_NEW( "$and", "[", "{", mongo_fields::analytic::metadata::CTX_ID.c_str(), BCON_INT32(_ctxId), "}",
                                            "{", mongo_fields::analytic::metadata::SENSOR_ID.c_str(), "{", "$eq", BCON_INT64(_sensor.id), "}", "}",
                                       "]"
                             );

    bson_t * update = BCON_NEW( "$set", "{",
                                        mongo_fields::analytic::metadata::CTX_ID.c_str(), BCON_INT32( _ctxId ),
                                        mongo_fields::analytic::metadata::SENSOR_ID.c_str(), BCON_INT64( _sensor.id ),
                                        mongo_fields::analytic::metadata::LAST_SESSION_ID.c_str(), BCON_INT32( _sensor.lastSession ),
                                        mongo_fields::analytic::metadata::UPDATE_STEP_MILLISEC.c_str(), BCON_INT64( _sensor.updateStepMillisec ),
                                        "}"
                               );

    const bool rt = mongoc_collection_update( m_tableAnalyticMetadata,
                                  MONGOC_UPDATE_UPSERT,
                                  query,
                                  update,
                                  NULL,
                                  NULL );

    if( ! rt ){
        // TODO: do
        return false;
    }

    bson_destroy( query );
    bson_destroy( update );

    return true;
}

void DatabaseManager::removeProcessedSensor( const TContextId _ctxId, const TSensorId _sensorId ){

    bson_t * query = BCON_NEW( "$and", "[", "{", mongo_fields::analytic::metadata::CTX_ID.c_str(), BCON_INT32(_ctxId), "}",
                                            "{", mongo_fields::analytic::metadata::SENSOR_ID.c_str(), "{", "$eq", BCON_INT64(_sensorId), "}", "}",
                                       "]"
                             );

    const bool result = mongoc_collection_remove( m_tableAnalyticMetadata, MONGOC_REMOVE_NONE, query, nullptr, nullptr );

    bson_destroy( query );
}









