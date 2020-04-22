#ifndef HLS_SOURCE_H
#define HLS_SOURCE_H

#include <string>
#include <thread>
#include <atomic>

#include <gst/gst.h>

#include "archive_creator_proxy.h"

class ArchiveCreatorLocal : public AArchiveCreator
{
    friend gboolean callbackGstSourceMessage( GstBus * bus, GstMessage * message, gpointer user_data );
public:    
    ArchiveCreatorLocal();
    ~ArchiveCreatorLocal();

    virtual bool start( const SInitSettings & _settings ) override;
    virtual void stop() override;
    virtual void tick() override;
    virtual bool isConnectionLosted() const override { return m_connectionLosted.load(); }
    virtual const SInitSettings & getSettings() const override { return m_settings; }
    virtual const SCurrentState & getCurrentState() override { return m_currentState; }
    virtual const std::string & getLastError() override { return m_lastError; }

private:
    static gboolean callbackGstSourceMessage( GstBus * bus, GstMessage * message, gpointer user_data );

    virtual bool getArchiveSessionInfo( int64_t & _startRecordTimeMillisec,
                                        std::string & _sessionName,
                                        std::string & _playlistName ) override;

    bool updateHlsSinkProperties();
    bool launchGstreamPipeline( const SInitSettings & _settings );
    void pauseGstreamPipeline();
    std::string definePipelineDescription( const SInitSettings & _settings );
    void tryToReconnect();

    // data
    bool m_closed;
    std::string m_liveStreamPlaylistDirFullPath;
    std::string m_originalPlaylistFullPath;
    SInitSettings m_settings;
    SCurrentState m_currentState;
    std::string m_lastError;
    std::atomic<bool> m_connectionLosted;

    // service
    GstElement * m_gstPipeline;
    GMainLoop * m_glibMainLoop;
    std::thread * m_threadGLibMainLoop;
};
using PArchiveCreator = std::shared_ptr<ArchiveCreatorLocal>;
using PArchiveCreatorConst = std::shared_ptr<const ArchiveCreatorLocal>;

#endif // HLS_SOURCE_H
