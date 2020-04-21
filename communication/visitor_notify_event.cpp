
#include <microservice_common/system/logger.h>

#include "common/common_utils.h"
#include "visitor_notify_event.h"

using namespace std;
using namespace common_types;

EventSendVisitor::EventSendVisitor()
{

}

void EventSendVisitor::visit( const SAnalyticEvent * _event ){

//    const bool rt = m_protobufForProcessingObjects.ParseFromString( _event->eventMessage );

    // parse analyzer event
    Json::Value parsedRecord;
    if( ! m_jsonReader.parse( _event->eventMessage.c_str(), parsedRecord, false ) ){
        VS_LOG_ERROR << "EventSendVisitor: parse failed of [1] Reason: [2] ["
                  << _event->eventMessage << "] [" << m_jsonReader.getFormattedErrorMessages() << "]"
                  << endl;
        return;
    }

    const int totalCount = parsedRecord["total_count"].asInt();
    if( 0 == totalCount ){
        return;
    }

    // create message    
    Json::Value root;
    try{
        root[ "cmd_type" ] = "analyze_from_server";
        root[ "cmd_name" ] = "event";
        root[ "total_count" ] = totalCount;
        root[ "sensor_id" ] = (unsigned long long)_event->sensorId;
        root[ "processing_id"] = _event->processingId;
        root[ "new_objects" ] = parsedRecord["new_objects"];
        root[ "changed_objects" ] = parsedRecord["changed_objects"];
        root[ "disappeared_objects" ] = parsedRecord["disappeared_objects"];
#ifndef Astra
    } catch( Json::Exception & _ex ){
#else
    } catch( std::exception & _ex ){
#endif
        VS_LOG_ERROR << "jsoncpp exception (EventSendVisitor::visit): " << _ex.what() << endl;
        return;
    }

    // create command
    PCommandOutgoingAnalyzeEvent cmd = m_poolOfEvents.getInstance( & m_services );
    cmd->m_outcomingMsg = m_jsonWriter.write( root );
    cmd->exec();
}

void EventSendVisitor::visit( const SAnalyzeStateEvent * _event ){

    // create message
    Json::Value root;
    root[ "cmd_type" ] = "analyze_from_server";
    root[ "cmd_name" ] = "state_changed";

    Json::Value analyzeState;
    analyzeState[ "state" ] = common_utils::convertAnalyzeStateToStr( _event->analyzeState );
    analyzeState[ "sensor_id" ] = (unsigned long long)_event->sensorId;
    analyzeState[ "error_msg" ] = _event->message;
    analyzeState["processing_id"] = _event->processingId;

    root["analyzer"] = analyzeState;

    // create command
    PCommandOutgoingAnalyzeEvent cmd = std::make_shared<CommandOutgoingAnalyzeEvent>( & m_services );
    cmd->m_outcomingMsg = m_jsonWriter.write( root );
    cmd->exec();
}

void EventSendVisitor::visit( const SArchiverStateEvent * _event ){

    // create message
    Json::Value root;
    root[ "cmd_type" ] = "analyze_from_server";
    root[ "cmd_name" ] = "state_changed";

    Json::Value archiverState;
    archiverState[ "state" ] = common_utils::convertArchivingStateToStr( _event->archivingState );
    archiverState[ "sensor_id" ] = (unsigned long long)_event->sensorId;
    archiverState[ "error_msg" ] = _event->message;
    archiverState["archiving_id"] = _event->archivingId;

    root["archiver"] = archiverState;

    // create command
    PCommandOutgoingAnalyzeEvent cmd = std::make_shared<CommandOutgoingAnalyzeEvent>( & m_services );
    cmd->m_outcomingMsg = m_jsonWriter.write( root );
    cmd->exec();
}

void EventSendVisitor::visit( const SOnceMoreEvent * ){

    // TODO: ?
}


