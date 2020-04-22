#ifndef VIDEO_SOURCE_PROXY_H
#define VIDEO_SOURCE_PROXY_H

#include <string>
#include <memory>

#include "common/common_types.h"

// interface
class IVideoSource
{
public:
    static constexpr int CONNECT_RETRIES_INFINITE = 0;
    static constexpr int CONNECT_RETRIES_IMMEDIATELY = -1;

    // TODO: do
    enum EStreamType {
        SHM_SRC,
        INTERVIDEO_SRC,
        UNDEFINED
    };

    struct SState {
        EStreamType streamType;
        std::string streamAddress;
    };

    struct SInitSettings {
        SInitSettings()
            : connectRetriesTimeoutSec(CONNECT_RETRIES_IMMEDIATELY)
        {}

        common_types::SVideoSource source;
        SystemEnvironmentFacadeVS * systemEnvironment;
        int connectRetriesTimeoutSec;
    };

    virtual ~IVideoSource(){}

    virtual const SInitSettings & getSettings() = 0;
    virtual const std::string & getLastError() = 0;

    // NOTE: source interface for HOW connect ( see also ISourcesProvider )
    virtual bool connect( const SInitSettings & _settings ) = 0;
    virtual void disconnect() = 0;
    virtual void addObserver( common_types::ISourceObserver * _observer ) = 0;

    virtual PNetworkClient getCommunicator() = 0;
    virtual common_types::ISourcesProvider::SSourceParameters getStreamParameters() = 0;
    virtual int32_t getReferenceCount() = 0;
};

// proxy
class VideoSourceProxy : public IVideoSource
{
public:
    VideoSourceProxy( IVideoSource * _impl );
    ~VideoSourceProxy();

    virtual const SInitSettings & getSettings() override;
    virtual const std::string & getLastError() override;

    virtual bool connect( const SInitSettings & _settings ) override;
    virtual void disconnect() override;
    virtual void addObserver( common_types::ISourceObserver * _observer ) override;

    virtual PNetworkClient getCommunicator() override;
    virtual common_types::ISourcesProvider::SSourceParameters getStreamParameters() override;
    virtual int32_t getReferenceCount() override;    

private:
    IVideoSource * m_impl;
};
using PVideoSourceProxy = std::shared_ptr<VideoSourceProxy>;

#endif // VIDEO_SOURCE_PROXY_H
