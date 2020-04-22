
#include <microservice_common/system/logger.h>

#include "common/common_utils.h"
#include "system/config_reader.h"
#include "storage_engine_facade.h"

using namespace std;
using namespace common_types;

static constexpr const char * PRINT_HEADER = "StorageEngine:";

StorageEngineFacade::StorageEngineFacade()
    : m_database(nullptr)
    , m_threadAssemblersMaintenance(nullptr)
    , m_objreprRepos(nullptr)
    , m_visitorForInternalEvents(this)
    , m_shutdownCalled(false)
{

}

StorageEngineFacade::~StorageEngineFacade(){

    shutdown();
}

IEventStore * StorageEngineFacade::getServiceOfEventStore(){
    return this;
}

IRecordingMetadataStore * StorageEngineFacade::getServiceRecordingMetadataStore(){
    return this;
}

IVideoStreamStore * StorageEngineFacade::getServiceVideoStreamStore(){
    return this;
}

IEventProcessor * StorageEngineFacade::getServiceEventProcessor(){
    return this;
}

bool StorageEngineFacade::init( SInitSettings _settings ){

    m_settings = _settings;

    // database
    DatabaseManager * database = DatabaseManager::getInstance();

    DatabaseManager::SInitSettings settings;
    settings.host = CONFIG_PARAMS.baseParams.MONGO_DB_ADDRESS;
    settings.databaseName = CONFIG_PARAMS.baseParams.MONGO_DB_NAME;

    if( ! database->init(settings) ){
        return false;
    }

    // objrepr
    ObjreprRepository * objrepr = new ObjreprRepository();

    ObjreprRepository::SInitSettings settings2;
    if( ! objrepr->init(settings2) ){
        return false;
    }

    // TODO: video recorder


    // distribute to visitors ( TODO: may be separate instance for each ? )
    m_visitorWriteEvent.m_database = database;
    m_visitorWriteEvent.m_objrepr = objrepr;
    m_visitorDeleteEvent.m_database = database;
    m_visitorDeleteEvent.m_objrepr = objrepr;
    m_visitorWriteMetadata.m_database = database;
    m_visitorWriteMetadata.m_objrepr = objrepr;
    m_visitorDeleteMetadata.m_database = database;
    m_visitorDeleteMetadata.m_objrepr = objrepr;

    m_objreprRepos = objrepr;
    m_database = database;

    //
    m_threadAssemblersMaintenance = new std::thread( & StorageEngineFacade::threadAssemblersMaintenance, this );

    VS_LOG_INFO << PRINT_HEADER << " init success" << endl;
    return true;
}

void StorageEngineFacade::shutdown(){

    if( ! m_shutdownCalled ){
        VS_LOG_INFO << PRINT_HEADER << " begin shutdown" << endl;

        m_shutdownCalled = true;

        common_utils::threadShutdown( m_threadAssemblersMaintenance );
        DatabaseManager::destroyInstance( m_database );
        delete m_objreprRepos;

        VS_LOG_INFO << PRINT_HEADER << " shutdown success" << endl;
    }
}

bool StorageEngineFacade::addEventProcessor( const string _sourceUrl, const TProcessingId & _procId ){

    //
    m_muVideoAsm.lock();
    auto iter = m_videoAssemblers.find( _procId );
    if( iter != m_videoAssemblers.end() ){
        VS_LOG_WARN << "video assembler for procId [" << _procId << "] already exist" << endl;
        m_muVideoAsm.unlock();
        return false;
    }
    m_muVideoAsm.unlock();

    //
    VideoAssembler::SInitSettings settings;
    settings.correlatedProcessingId = _procId;
    settings.communicationWithVideoSource = m_settings.serviceLocator.videoSources->getCommunicatorWithSource( _sourceUrl );

    PVideoAssembler assembler = std::make_shared<VideoAssembler>();
    if( ! assembler->init(settings) ){
        m_lastError = assembler->getLastError();
        return false;
    }

    m_muVideoAsm.lock();
    m_videoAssemblers.insert( {_procId, assembler} );
    m_muVideoAsm.unlock();

    return true;
}

bool StorageEngineFacade::removeEventProcessor( const TProcessingId & _procId ){

    m_muVideoAsm.lock();
    auto iter = m_videoAssemblers.find( _procId );
    if( iter != m_videoAssemblers.end() ){
        m_videoAssemblers.erase( iter );
        m_muVideoAsm.unlock();
        return true;
    }
    else{
        m_muVideoAsm.unlock();
        return false;
    }
}

void StorageEngineFacade::processEvent( const SInternalEvent & _event ){

    _event.accept( & m_visitorForInternalEvents );
}

void StorageEngineFacade::threadAssemblersMaintenance(){

    VS_LOG_INFO << PRINT_HEADER << " start an assmeblers maintenance THREAD" << endl;

    constexpr int listenIntervalMillisec = 10;

    while( ! m_shutdownCalled ){

        m_muVideoAsm.lock();
        for( auto & valuePair : m_videoAssemblers ){
            PVideoAssembler & assembler = valuePair.second;

            const vector<VideoAssembler::SAssembledVideo *> readyVideos = assembler->getReadyVideos();
            for( VideoAssembler::SAssembledVideo * video : readyVideos ){

                const int64_t timeWriteFileBegin = common_utils::getCurrentTimeMillisec();
                const bool rt = m_objreprRepos->writeProcessingObjectVideoAttr( video->forObjectId, video->location );
                VS_LOG_INFO << PRINT_HEADER
                            << " for obj: " << video->forObjectId << " loc: " << video->location
                            << " video file write duration (millisec): "
                            <<  common_utils::getCurrentTimeMillisec() - timeWriteFileBegin
                            << endl;
            }
        }
        m_muVideoAsm.unlock();



        this_thread::sleep_for( chrono::milliseconds(listenIntervalMillisec) );
    }

    VS_LOG_INFO << PRINT_HEADER << " assmeblers maintenance THREAD is stopped" << endl;
}

bool StorageEngineFacade::startRecording( SVideoSource _source ){
    // TODO: do
}

bool StorageEngineFacade::stopRecording( SVideoSource _source ){
    // TODO: do
}

void StorageEngineFacade::writeEvent( const SEvent & _event ){
    _event.accept( & m_visitorWriteEvent );
}

SEventFromStorage StorageEngineFacade::readEvents( const SAnalyticFilter & _filter ){
    return m_database->readAnalyticEventData( _filter );
}

void StorageEngineFacade::deleteEvent( const SEvent & _event ){
    _event.accept( & m_visitorDeleteEvent );
}

void StorageEngineFacade::writeMetadata( const SMetadata & _metadata ){
    _metadata.accept( & m_visitorWriteMetadata );
}

std::vector<SHlsMetadata> StorageEngineFacade::readMetadatas(){
    return m_database->readHlsMetadata();
}

void StorageEngineFacade::deleteMetadata( const SMetadata & _metadata ){
    _metadata.accept( & m_visitorDeleteMetadata );
}
