
#include <microservice_common/system/logger.h>

#include "videocard_balancer.h"
#include "system/config_reader.h"

using namespace std;
using namespace common_types;

static constexpr const char * PRINT_HEADER = "VideocardBalancer:";

VideocardBalancer::VideocardBalancer()
{

}

bool VideocardBalancer::init( SInitSettings _settings ){

    m_settings = _settings;

    return true;
}

void VideocardBalancer::getVideocard( TProcessingId _procId,
                                      SVideoResolution _res,
                                      TProfileId _profileId,
                                      unsigned int & _videocardIdx,
                                      void * & _dpfProcessorPtr ){

    _videocardIdx = 0;
    _dpfProcessorPtr = nullptr;
    bool allCardsWithThisProfileOverflowed = true;

    // check this profile
    auto iter = m_videocardsByProfileId.find( _profileId );
    if( iter != m_videocardsByProfileId.end() ){

        // traverse on cards with such profile
        auto iterRange = m_videocardsByProfileId.equal_range( _profileId );
        for( auto iter = iterRange.first; iter != iterRange.second; ++iter ){

            SVideocard * videocard = iter->second;

            // max processings per card
            if( static_cast<int32_t>(videocard->processingsWithResolution.size()) < CONFIG_PARAMS.PROCESSING_MAX_PROCESSINGS_PER_VIDEOCARD ){
                _videocardIdx = videocard->idx;
                _dpfProcessorPtr = getDpfProcessorPtr( _profileId );
                assert( _dpfProcessorPtr && "dpf processor from used card is NULL" );

                videocard->processingsWithResolution.insert( {_procId, _res} );
                m_videocardsByProcessingId.insert( {_procId, videocard} );

                allCardsWithThisProfileOverflowed = false;
            }
            else{
                stringstream ss;
                ss << PRINT_HEADER
                   << " videocard [" << videocard->idx << "]"
                   << " with profile [" << videocard->profileId << "]"
                   << " already overflowed [" << videocard->processingsWithResolution.size() << "]"
                   << " check next card...";
                VS_LOG_INFO << ss.str() << endl;

                continue;
            }
        }
    }

    // new/existing profile to new card
    if( iter == m_videocardsByProfileId.end() || allCardsWithThisProfileOverflowed ){
        _videocardIdx = SYSTEM_MONITOR.getNextVideoCardIdx();

        if( SystemMonitor::NO_AVAILABLE_CARDS == _videocardIdx ){
            m_lastError = SYSTEM_MONITOR.getLastError();
            return;
        }

        SVideocard * videocard = new SVideocard();
        videocard->idx = _videocardIdx;
        videocard->dpfProcessorPtr = _dpfProcessorPtr;
        videocard->profileId = _profileId;
        videocard->processingsWithResolution.insert( {_procId, _res} );

        m_videocardsByProfileId.insert( {_profileId, videocard} );
        m_videocardsByProcessingId.insert( {_procId, videocard} );
    }
}

void VideocardBalancer::returnVideocard( TProcessingId _procId ){

    auto iter = m_videocardsByProcessingId.find( _procId );
    if( iter != m_videocardsByProcessingId.end() ){
        SVideocard * card = iter->second;

        card->processingsWithResolution.erase( _procId );

        if( card->processingsWithResolution.empty() ){
            SYSTEM_MONITOR.releaseVideoCardIdx( card->idx );

            m_videocardsByProcessingId.erase( _procId );
            m_videocardsByProfileId.erase( card->profileId );

            delete card;
        }
    }
}

void * VideocardBalancer::getDpfProcessorPtr( TProfileId _profileId ){

    m_settings.mutexAnalyzersLock->lock();
    for( auto iter = m_settings.analyzers->begin(); iter != m_settings.analyzers->end(); ++iter ){
        PAnalyzerProxy analyzer = ( * iter );

        if( analyzer->getSettings().profileId == _profileId ){
            m_settings.mutexAnalyzersLock->unlock();
            return analyzer->getSettings().pointerContainer.dpfProcessorPtr;
        }
    }
    m_settings.mutexAnalyzersLock->unlock();
    return nullptr;
}








