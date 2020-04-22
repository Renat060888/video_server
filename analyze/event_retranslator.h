#ifndef EVENT_RETRANSLATOR_H
#define EVENT_RETRANSLATOR_H

#include <unordered_map>
#include <mutex>

#include <jsoncpp/json/reader.h>

#include "common/common_types.h"

class EventRetranslator : public common_types::IEventRetranslator, public INetworkObserver
{
public:
    struct SRetranslateSink {
        SRetranslateSink()
            : lastHeartbeatAtMillisec(0)
        {}
        PNetworkClient network;
        int64_t lastHeartbeatAtMillisec;
    };

    EventRetranslator();

    virtual void retranslateEvent( const common_types::SEvent & _event ) override;


private:
    virtual void callbackNetworkRequest( PEnvironmentRequest _request ) override;

    PNetworkClient createNewSink( common_types::TSensorId _sensorId );
    std::string messageDistillation( const std::string & _eventMsg );

    // data
    std::unordered_map<common_types::TSensorId, SRetranslateSink> m_retranslateSinks;

    // service
    Json::Reader m_jsonReader;
    std::mutex m_muSinks;


};

#endif // EVENT_RETRANSLATOR_H
