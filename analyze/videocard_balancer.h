#ifndef VIDEOCARD_BALANCER_H
#define VIDEOCARD_BALANCER_H

#include <unordered_map>
#include <mutex>

#include <microservice_common/system/system_monitor.h>

#include "video_server_common/analyze/analyzer_proxy.h"

class VideocardBalancer
{
public:
    struct SInitSettings {
        SInitSettings()
        {}
        std::vector<PAnalyzerProxy> * analyzers;
        std::mutex * mutexAnalyzersLock;
    };

    struct SVideocard {
        SVideocard()
            : idx(SYSTEM_MONITOR.NO_AVAILABLE_CARDS)
            , profileId(0)
            , dpfProcessorPtr(nullptr)
        {}
        unsigned int idx;
        TProfileId profileId;
        void * dpfProcessorPtr;
        std::map<TProcessingId, SVideoResolution> processingsWithResolution;
    };

    VideocardBalancer();

    bool init( SInitSettings _settings );
    const std::string & getLastError(){ return m_lastError; }

    void getVideocard( TProcessingId _procId,
                       SVideoResolution _res,
                       TProfileId _profileId,
                       unsigned int & _videocardIdx,
                       void * & _dpfProcessorPtr );
    void returnVideocard( TProcessingId _procId );

private:
    void * getDpfProcessorPtr( TProfileId _profileId );

    // data
    std::string m_lastError;
    SInitSettings m_settings;
    std::unordered_multimap<TProfileId, SVideocard *> m_videocardsByProfileId;
    std::map<TProcessingId, SVideocard *> m_videocardsByProcessingId;

    // service




};

#endif // VIDEOCARD_BALANCER_H
