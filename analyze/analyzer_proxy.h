#ifndef ANALYZER_PROXY_H
#define ANALYZER_PROXY_H

#include <map>

#include "common/common_types.h"
#include "common/common_types_with_plugins.h"

// interface
class IAnalyzer
{
public:
    struct SInitSettings {
        // pre-init
        common_types::TSensorId sensorId;
        uint64_t profileId;
        std::map<std::string, uint64_t> allowedDpfLabelObjreprClassinfo;
        std::string intervideosinkChannelName;
        int32_t processingFlags;
        void * dpfProcessorPtr;
        int32_t maxObjectsForBestImageTracking;

        // TODO: temp
        std::string VideoReceiverRtpPayloadSocket;

        int8_t usedVideocardIdx;
        // init
        common_types::TSessionNum currentSession;
        std::string processingId;
        std::string processingName;
        std::string sourceUrl;
        std::vector<common_types::SPluginMetadata> plugins;
        // this data sets to plugin via pointer
        common_types_with_plugins::SPointerContainer pointerContainer;

        std::string shmRtpStreamCaps;
        std::string shmRtpStreamEncoding;

        bool operator ==( const SInitSettings & _rhs ) const {

            return ( sensorId == _rhs.sensorId &&
                     profileId == _rhs.profileId &&
                     allowedDpfLabelObjreprClassinfo == _rhs.allowedDpfLabelObjreprClassinfo &&
                     intervideosinkChannelName == _rhs.intervideosinkChannelName &&
                     processingFlags == _rhs.processingFlags &&

                     usedVideocardIdx == _rhs.usedVideocardIdx &&

                     currentSession == _rhs.currentSession &&
                     processingId == _rhs.processingId &&
                     processingName == _rhs.processingName &&
                     sourceUrl == _rhs.sourceUrl &&
                     plugins == _rhs.plugins &&

                     pointerContainer == _rhs.pointerContainer &&

                     shmRtpStreamCaps == _rhs.shmRtpStreamCaps &&
                     shmRtpStreamEncoding == _rhs.shmRtpStreamEncoding
                   );
        }
    };

    struct SAnalyzeStatus {
        SAnalyzeStatus()
            : sensorId(0)
            , analyzeState(common_types::EAnalyzeState::UNDEFINED)
            , profileId(0)
        {}

        common_types::TSensorId sensorId;
        common_types::EAnalyzeState analyzeState;
        std::string processingId;
        std::string processingName;
        uint64_t profileId;
    };

    virtual ~IAnalyzer(){}

    virtual bool start( SInitSettings _settings ) = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void stop() = 0;

    virtual const SAnalyzeStatus & getStatus() = 0;
    virtual const SInitSettings & getSettings() = 0;
    virtual const std::string & getLastError() = 0;

    virtual std::vector<common_types::SAnalyticEvent> getAccumulatedEvents() = 0;
    virtual std::vector<common_types::SAnalyticEvent> getAccumulatedInstantEvents() = 0;

    virtual void setLivePlaying( bool _live ) = 0;
};

// proxy
class AnalyzerProxy : public IAnalyzer
{
public:
    AnalyzerProxy( IAnalyzer * _impl );
    ~AnalyzerProxy() override;

    bool start( SInitSettings _settings ) override;
    void resume() override;
    void pause() override;
    void stop() override;
    const SAnalyzeStatus & getStatus() override;

    const SInitSettings & getSettings() override;
    const std::string & getLastError() override;

    std::vector<common_types::SAnalyticEvent> getAccumulatedEvents() override;
    std::vector<common_types::SAnalyticEvent> getAccumulatedInstantEvents() override;

    void setLivePlaying( bool _live ) override;

private:
    IAnalyzer * m_impl;

};
using PAnalyzerProxy = std::shared_ptr<AnalyzerProxy>;

#endif // ANALYZER_PROXY_H
