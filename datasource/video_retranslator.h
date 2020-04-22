#ifndef RTSP_SOURCE_H
#define RTSP_SOURCE_H

#include <string>
#include <thread>

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
//#include "gst_rtsp_server/rtsp-server/rtsp-server.h"

#include "system/pipeline_monitor.h"
#include "common/common_types.h"
#include "storage/objrepr_repository.h"

class VideoRetranslator
{
    friend void callbackClientConnected( GstRTSPServer * _server, GstRTSPClient * _client , gpointer user_data );
public:
    static const std::string DEFAULT_LISTEN_ITF;

    struct SInitSettings {
        SInitSettings()
            : listenIp("0.0.0.0")
            , sensorId(0)
            , serverPort(0)
            , outcomingUdpPacketsRangeBegin(0)
            , outcomingUdpPacketsRangeEnd(0)
            , latencyMillisec(2000)
            , outputEncoding(common_types::EOutputEncoding::UNDEFINED)
            , monitorPipeline(false)
        {

        }

        std::string listenIp;
        std::string sourceUrl;
        common_types::TSensorId sensorId;
        int serverPort;
        int outcomingUdpPacketsRangeBegin;
        int outcomingUdpPacketsRangeEnd;

        // TODO: ?
        std::string rtspPostfix;

        // TODO: moved to VideoSource
        std::string userId;
        std::string userPassword;
        int latencyMillisec;

        common_types::EOutputEncoding outputEncoding;
        std::string intervideosinkChannelName;

        std::string shmRawStreamCaps;
        std::string shmRtpStreamEncoding;
        std::string shmRtpStreamCaps;

        bool monitorPipeline;
        common_types::SVideoSource copyOfSourceParameters;
    };

    struct SCurrentState {
        SCurrentState() :
            clientCount(0){

        }

        int clientCount;
    };

    VideoRetranslator();
    ~VideoRetranslator();

    bool start( const SInitSettings & _settings );
    void stop();

    const SInitSettings & getSettings() const { return m_settings; }
    const SCurrentState & getState() const;
    const std::string & getLastError() { return m_lastError; }

private:
    static GstRTSPFilterResult callbackClientFilter( GstRTSPServer * _server, GstRTSPClient * _client, gpointer _userData );
    static void callbackClientConnected( GstRTSPServer * _server, GstRTSPClient * _client, gpointer user_data );
    static gboolean callbackCleanupExpiredSessions( GstRTSPServer * _server );

    static void callbackClientDescribeRequest( GstRTSPClient * _client, GstRTSPContext * _ctx, gpointer _userData );
    static void callbackClientCheckRequirements( GstRTSPClient * _client, GstRTSPContext * _ctx, char ** _arr, gpointer _userData );
    static void callbackClientSendMessage( GstRTSPClient * _client, GstRTSPSession * _session, gpointer _msg, gpointer _userData );

    bool launchRTSPServer( const SInitSettings & _settings, const std::string & _pipelineDscr, std::string & _retranslationUrl );
    std::string definePipelineDescription( const SInitSettings & _settings );
    void watchForClient( GstRTSPClient * _client );

    // data
    std::thread * m_threadGLibMainLoop;
    GMainLoop * m_glibMainLoop;
    GstRTSPServer * m_gstRtspServer;
    GstRTSPAddressPool * m_addressPool;
    guint m_gstRtspServerId;
    std::string m_retranslationUrl;

    // service
    SInitSettings m_settings;
    SCurrentState m_state;
    std::string m_lastError;
    PipelineMonitor m_pipelineMonitor;
    ObjreprRepository m_objreprRepository;
};
using PVideoRetranslator = std::shared_ptr<VideoRetranslator>;
using PConstVideoRetranslator = std::shared_ptr<const VideoRetranslator>;

#endif // RTSP_SOURCE_H
