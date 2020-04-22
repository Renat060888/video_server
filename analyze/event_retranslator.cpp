
#include <objrepr/reprServer.h>
#include <microservice_common/system/logger.h>

#include "common/common_utils.h"
#include "communication/objrepr_listener.h"
#include "communication/protocols/info_channel_data.h"
#include "communication/protocols/yas/yas_compressor.h"
#include "event_retranslator.h"

using namespace std;
using namespace common_types;

static constexpr const char * PRINT_HEADER = "EventRetran:";
static constexpr int64_t HEARTBEAT_INTERVAL_MILLISEC = 10000;

EventRetranslator::EventRetranslator()
{

}

void EventRetranslator::retranslateEvent( const SEvent & _event ){

    // TODO: temp crutch for just one event type
    const SAnalyticEvent * ae = (const SAnalyticEvent *)( & _event );

    std::lock_guard<std::mutex> lock( m_muSinks );

    // retranslate to existing sink
    auto iter = m_retranslateSinks.find( ae->sensorId );
    if( iter != m_retranslateSinks.end() ){
        SRetranslateSink & sink = iter->second;

        // check that retranslation point is alive
        if( (common_utils::getCurrentTimeMillisec() - sink.lastHeartbeatAtMillisec) <= HEARTBEAT_INTERVAL_MILLISEC ){
            const string msg = messageDistillation( ae->eventMessage );

            PEnvironmentRequest request = sink.network->getRequestInstance();
            request->setOutcomingMessage( msg );
        }
    }
    // create new sink
    else{
        // new network ( subscribe to camera )
        SRetranslateSink sink;
        sink.network = createNewSink( ae->sensorId );
//        sink.network = nullptr; // TODO: get from CommGateway -> getEventRetranslateCommunicator()

        m_retranslateSinks.insert( {ae->sensorId, sink} );

        // TODO: ...and only then retranslate ( mutex ! )

    }
}

PNetworkClient EventRetranslator::createNewSink( TSensorId _sensorId ){    

    //
    ObjreprListener::SInitSettings settings;
    settings.listenedObjectId = _sensorId;

    PObjreprListener network = std::make_shared<ObjreprListener>( -1 );
    if( ! network->init(settings) ){
        VS_LOG_ERROR << PRINT_HEADER << " objrepr listener with sensor id [" << _sensorId << "] not inited" << endl;
        return nullptr;
    }

    //
    PNetworkProvider provider = std::dynamic_pointer_cast<INetworkProvider>( network );
    provider->addObserver( this );

    return network;
}

static int64_t g_frameProcessingId = 0;

string EventRetranslator::messageDistillation( const std::string & _eventMsg ){

    // parse analyzer event
    Json::Value parsedRecord;
    if( ! m_jsonReader.parse( _eventMsg.c_str(), parsedRecord, false ) ){
        VS_LOG_ERROR << "EventRetranslator: parse failed of [1] Reason: [2] ["
                  << _eventMsg << "] [" << m_jsonReader.getFormattedErrorMessages() << "]"
                  << endl;
        return std::string();
    }

    const int totalCount = parsedRecord["total_count"].asInt();
    if( 0 == totalCount ){
        return std::string();
    }

    // create message
    const int64_t timestamp = parsedRecord["timestamp"].asInt64();

    info_channel_data::DetectedFrame detectedFrame;
    detectedFrame.objects.reserve( totalCount );

    Json::Value detectedObjectRecords = parsedRecord["detected_objects"];
    for( Json::ArrayIndex i = 0; i < detectedObjectRecords.size(); i++ ){
        Json::Value element = detectedObjectRecords[ i ];

        info_channel_data::DetectedObject obj;
        obj.x = element["x"].asInt();
        obj.y = element["y"].asInt();
        obj.w = element["w"].asInt();
        obj.h = element["h"].asInt();
        obj.timestamp = timestamp;

        detectedFrame.objects.push_back( obj );
    }

    detectedFrame.processing_index = ++g_frameProcessingId;

    // compress
    const string distilled = yas_compressor::YasCompressor::compressData( detectedFrame );
    return distilled;
}

void EventRetranslator::callbackNetworkRequest( PEnvironmentRequest _request ){

    // heartbeats from all sensors
    const string msg = _request->getIncomingMessage();
    const TSensorId sid = ( * (TSensorId *)( _request->getUserData() ) );

    // check message type
    info_channel_data::DetectionMessage detectMsg = yas_compressor::YasCompressor::decompressData< info_channel_data::DetectionMessage >( msg );
    if( info_channel_data::MessageType::MT_REQUEST == detectMsg.getMessageType() ){

        VS_LOG_INFO << PRINT_HEADER
                    << " request for subscribe to sensor id: " << sid
                    << " msg [" << msg << "]"
                    << endl;

        m_muSinks.lock();
        auto iter = m_retranslateSinks.find( sid );
        if( iter != m_retranslateSinks.end() ){
            SRetranslateSink & sink = iter->second;

            sink.lastHeartbeatAtMillisec = common_utils::getCurrentTimeMillisec();
        }
        m_muSinks.unlock();
    }
}









