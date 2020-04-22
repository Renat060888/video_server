
#include <microservice_common/system/logger.h>

#include "visitor_event_stat.h"

//#define ENABLE_VIDEO_ANALYTICS

#ifdef ENABLE_VIDEO_ANALYTICS
#include <video_analytics/analytic_events_data/manager_events.h>
#endif

using namespace std;
using namespace common_types;

VisitorEventStat::VisitorEventStat()
{

}

void VisitorEventStat::visit( const SAnalyticEvent * _event ){

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

#ifdef ENABLE_VIDEO_ANALYTICS
    // create VAS-event
    std::vector<analytic_events::Event> events;

    Json::Value newObjects = parsedRecord["new_objects"];
    for( int i = 0; i < newObjects.size(); i++ ){
        Json::Value newObjectRecord = newObjects[ i ];

        analytic_events::Event event;
        event.cat = analytic_events::TypeEvent::FINDED;
        event.id = generateUniqueId();
        event.class_object = newObjectRecord["objrepr_classinfo_name"].asString();
        event.time = CHRONO_NOW();
        event.pix_coords.x = newObjectRecord["x"].asDouble();
        event.pix_coords.y = newObjectRecord["y"].asDouble();
        event.pix_coords.width = newObjectRecord["w"].asDouble();
        event.pix_coords.height = newObjectRecord["h"].asDouble();
        event.sensor_id = _event->sensorId;

        events.push_back( event );
    }

//    Json::Value changedObjects = parsedRecord["changed_objects"];
//    for( int i = 0; i < changedObjects.size(); i++ ){
//        Json::Value chanObjectRecord = changedObjects[ i ];

//        analytic_events::Event event;
//        event.cat = analytic_events::TypeEvent::MOVED;
//        event.id = generateUniqueId();
//        event.class_object = chanObjectRecord["objrepr_classinfo_name"].asString();
//        event.time = CHRONO_NOW();
//        event.pix_coords.x = chanObjectRecord["x"].asDouble();
//        event.pix_coords.y = chanObjectRecord["y"].asDouble();
//        event.pix_coords.width = chanObjectRecord["w"].asDouble();
//        event.pix_coords.height = chanObjectRecord["h"].asDouble();
//        event.sensor_id = _event->sensorId;

//        events.push_back( event );
//    }

    Json::Value disappearedObjects = parsedRecord["disappeared_objects"];
    for( int i = 0; i < disappearedObjects.size(); i++ ){
        Json::Value disObjectRecord = disappearedObjects[ i ];

        analytic_events::Event event;
        event.cat = analytic_events::TypeEvent::LOSTED;
        event.id = generateUniqueId();
        event.class_object = disObjectRecord["objrepr_classinfo_name"].asString();
        event.time = CHRONO_NOW();
        event.sensor_id = _event->sensorId;

        events.push_back( event );
    }

    // push to VAS
    if( ! events.empty() ){
        analytic_events::ManagerEvents::instance().sendNewEvents( events );
    }
#endif
}

void VisitorEventStat::visit( const SOnceMoreEvent * _event ){

}
