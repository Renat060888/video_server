#ifndef STORAGE_ENGINE_FACADE_H
#define STORAGE_ENGINE_FACADE_H

#include <functional>

#include "common/common_types.h"
#include "visitor_event.h"
#include "visitor_metadata.h"
#include "visitor_internal_event.h"
#include "video_assembler.h"
#include "database_manager.h"

class StorageEngineFacade
        : public common_types::IEventStore
        , public common_types::IRecordingMetadataStore
        , public common_types::IVideoStreamStore
        , public common_types::IEventProcessor
{
    friend class VisitorInternalEvent;
public:
    struct SServiceLocator {
        SServiceLocator() :
            communicationProvider(nullptr),
            videoSources(nullptr)
        {}
        common_types::IInternalCommunication * communicationProvider;
        common_types::ISourcesProvider * videoSources;
    };

    struct SInitSettings {
        SServiceLocator serviceLocator;
    };

    StorageEngineFacade();
    virtual ~StorageEngineFacade();

    // facade services
    IEventStore * getServiceOfEventStore();
    IRecordingMetadataStore * getServiceRecordingMetadataStore();
    IVideoStreamStore * getServiceVideoStreamStore();
    IEventProcessor * getServiceEventProcessor();

    bool init( SInitSettings _settings );
    void shutdown();
    const std::string & getLastError(){ return m_lastError; }


private:
    void threadAssemblersMaintenance();

    virtual bool startRecording( common_types::SVideoSource _source ) override;
    virtual bool stopRecording( common_types::SVideoSource _source ) override;

    virtual void writeEvent( const common_types::SEvent & _event ) override;
    virtual void deleteEvent( const common_types::SEvent & _event ) override;
    virtual SEventFromStorage readEvents( const common_types::SAnalyticFilter & _filter ) override;

    virtual void writeMetadata( const common_types::SMetadata & _metadata ) override;
    virtual void deleteMetadata( const common_types::SMetadata & _metadata ) override;
    virtual std::vector<common_types::SHlsMetadata> readMetadatas() override;

    virtual bool addEventProcessor( const std::string _sourceUrl, const common_types::TProcessingId & _procId ) override;
    virtual bool removeEventProcessor( const common_types::TProcessingId & _procId ) override;
    virtual void processEvent( const common_types::SInternalEvent & _event ) override;

    // data
    std::string m_lastError;
    SInitSettings m_settings;
    std::map<common_types::TProcessingId, PVideoAssembler> m_videoAssemblers;
    bool m_shutdownCalled;

    // service
    std::mutex m_muVideoAsm;
    std::thread * m_threadAssemblersMaintenance;

    EventWriteVisitor m_visitorWriteEvent;
    EventDeleteVisitor m_visitorDeleteEvent;
    MetadataWriteVisitor m_visitorWriteMetadata;
    MetadataDeleteVisitor m_visitorDeleteMetadata;
    VisitorInternalEvent m_visitorForInternalEvents;


    ObjreprRepository * m_objreprRepos;
    DatabaseManager * m_database;


};

#endif // STORAGE_ENGINE_FACADE_H
