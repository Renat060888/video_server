#ifndef VIDEO_SOURCE_H
#define VIDEO_SOURCE_H

#include <cassert>
#include <memory>
#include <thread>

#include <gst/gst.h>

#include "video_source_proxy.h"

class VideoSourceLocal : public IVideoSource
{
public:    
    VideoSourceLocal();
    ~VideoSourceLocal() override;

    virtual const SInitSettings & getSettings() override { return m_settings; }
    virtual const std::string & getLastError() override { return m_lastError; }

    virtual bool connect( const SInitSettings & _settings ) override;
    virtual void disconnect() override;
    virtual void addObserver( common_types::ISourceObserver * _observer ) override;
    virtual PNetworkClient getCommunicator() { assert( false && "communication is not needed in local version" ); }

    virtual int32_t getReferenceCount() override { return m_referenceCounter; }
    virtual common_types::ISourcesProvider::SSourceParameters getStreamParameters() override;

private:
    static gboolean callback_gst_source_message( GstBus * bus, GstMessage * message, gpointer user_data );

    std::string definePipelineDescription( const SInitSettings & _settings );

    // data
    SInitSettings m_settings;
    std::string m_lastError;
    int32_t m_referenceCounter;
    std::vector<common_types::ISourceObserver *> m_observers;

    //
    GstElement * m_gstPipeline;
    GMainLoop * m_glibMainLoop;
    std::thread * m_threadGLibMainLoop;
};
using PVideoSourceLocal = std::shared_ptr<VideoSourceLocal>;


#endif // VIDEO_SOURCE_H
