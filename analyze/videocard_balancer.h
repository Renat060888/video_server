#ifndef VIDEOCARD_BALANCER_H
#define VIDEOCARD_BALANCER_H

#include <unordered_map>
#include <mutex>

#include <microservice_common/system/system_monitor.h>

#include "analyzer_proxy.h"

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
        common_types::TProfileId profileId;
        void * dpfProcessorPtr;
        std::map<common_types::TProcessingId, common_types::SVideoResolution> processingsWithResolution;
    };

    VideocardBalancer();

    bool init( SInitSettings _settings );
    const std::string & getLastError(){ return m_lastError; }

    void getVideocard( common_types::TProcessingId _procId,
                       common_types::SVideoResolution _res,
                       common_types::TProfileId _profileId,
                       unsigned int & _videocardIdx,
                       void * & _dpfProcessorPtr );
    void returnVideocard( common_types::TProcessingId _procId );

private:
    void * getDpfProcessorPtr( common_types::TProfileId _profileId );

    // data
    std::string m_lastError;
    SInitSettings m_settings;
    std::unordered_multimap<common_types::TProfileId, SVideocard *> m_videocardsByProfileId;
    std::map<common_types::TProcessingId, SVideocard *> m_videocardsByProcessingId;

    // service




};

#endif // VIDEOCARD_BALANCER_H
