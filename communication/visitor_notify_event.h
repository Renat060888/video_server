#ifndef VISITOR_NOTIFY_EVENT_H
#define VISITOR_NOTIFY_EVENT_H

#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/writer.h>
#include <microservice_common/system/object_pool.h>

#include "commands/analyze/cmd_outgoing_analyze_event.h"
#include "common/common_types.h"

// TODO: do
//#include "video_server_common/communication/protocols/internal_processing_objects.pb.h"

class EventSendVisitor : public common_types::IEventVisitor {
    // visitor-"owner"
    friend class AsyncNotifier;
public:
    EventSendVisitor();
    virtual ~EventSendVisitor(){}

    virtual void visit( const common_types::SAnalyticEvent * _event ) override;
    virtual void visit( const common_types::SAnalyzeStateEvent * _event ) override;
    virtual void visit( const common_types::SArchiverStateEvent * _event ) override;
    virtual void visit( const common_types::SOnceMoreEvent * _event ) override;


private:
    // data
    common_types::SIncomingCommandServices m_services;

    // service
    Json::FastWriter m_jsonWriter;
    Json::Reader m_jsonReader;
    ObjectPool<CommandOutgoingAnalyzeEvent, common_types::SIncomingCommandServices *> m_poolOfEvents;

    // TODO: do
//    video_server_protocol::ProcessedObjectsContainer m_protobufForProcessingObjects;
};

#endif // VISITOR_NOTIFY_EVENT_H
