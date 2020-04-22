
#include <iostream>

#include <boost/format.hpp>
#include <microservice_common/system/logger.h>

#include "common/common_utils.h"
#include "system/path_locator.h"
#include "video_retranslator.h"

const std::string VideoRetranslator::DEFAULT_LISTEN_ITF = "0.0.0.0";

static constexpr const char * PRINT_HEADER = "Retranslator:";

using namespace std;
using namespace common_types;

// NOTE: speed-preset (speed/quality tradeoff): ultrafast / superfast / veryfast / faster / fast / medium / slow / slower / veryslow
static const std::map<EOutputEncoding, string> OUTPUT_ENCODINGS = {
    {EOutputEncoding::H264, "! queue ! x264enc speed-preset=veryfast tune=zerolatency ! h264parse ! rtph264pay name=pay0 pt=96"},
    {EOutputEncoding::JPEG, "! queue ! jpegenc quality=60 ! jifmux ! rtpjpegpay name=pay0 pt=96"}
                                                                  };

static const std::map<string, string> OUTPUT_ENCODINGS_FROM_RTP = { // pt=96
    {"H264", "rtph264depay ! rtph264pay name=pay0"},
    {"JPEG", "rtpjpegdepay ! rtpjpegpay name=pay0"},
    {"MP4V-ES", "rtpmp4vdepay ! rtpmp4vpay name=pay0"}
                                                                  };

VideoRetranslator::VideoRetranslator()
    : m_threadGLibMainLoop( nullptr )
    , m_glibMainLoop( nullptr )
    , m_gstRtspServer( nullptr )
    , m_addressPool( nullptr )
    , m_gstRtspServerId( 0 )
{

}

VideoRetranslator::~VideoRetranslator(){

    if( m_threadGLibMainLoop ){
        stop();
    }

    if( m_settings.monitorPipeline ){
        m_pipelineMonitor.stop();
    }
}

bool VideoRetranslator::start( const SInitSettings & _settings ){

    m_settings = _settings;

    const string pipelineDscr = definePipelineDescription( _settings );
    if( pipelineDscr.empty() ){
        VS_LOG_ERROR << PRINT_HEADER << " empty pipeline for URL [" << _settings.sourceUrl << "] Find reason" << endl;
        return false;
    }

    // -----------------------------------------------
    // monitoring mode
    // -----------------------------------------------
    if( _settings.monitorPipeline ){
        PipelineMonitor::SInitSettings sets;
        sets.pipeline = pipelineDscr;
        sets.listenBuffers = true;
        sets.listenEvents = true;
        sets.listenMessages = true;
        sets.listenQueries = true;
        sets.elementsToListen = { "shmsrc_raw" };

        return m_pipelineMonitor.launch( sets );
    }

    string retranslationUrl;
    if( ! launchRTSPServer(_settings, pipelineDscr, retranslationUrl) ){
        return false;
    }

    const bool rt = m_objreprRepository.writeSensorRetranslateUrl( _settings.sensorId, retranslationUrl );

    return true;
}

bool VideoRetranslator::launchRTSPServer( const SInitSettings & _settings, const std::string & _pipelineDscr, std::string & _retranslationUrl ){

    // -----------------------------------------------
    // main retranslator mode
    // -----------------------------------------------
    m_gstRtspServer = gst_rtsp_server_new();
    gst_rtsp_server_set_address( m_gstRtspServer, _settings.listenIp.c_str() );
    gst_rtsp_server_set_service( m_gstRtspServer, std::to_string( _settings.serverPort ).data() );

    /* get the mount points for this server, every server has a default object
     * that be used to map uri mount points to media factories */
    GstRTSPMountPoints * mounts = gst_rtsp_server_get_mount_points( m_gstRtspServer );


    /* make a media factory for a test stream. The default media factory can use
     * gst-launch syntax to create pipelines.
     * any launch line works as long as it contains elements named pay%d. Each
     * element with pay%d names will be a stream */
    GstRTSPMediaFactory * factory = gst_rtsp_media_factory_new();
    gst_rtsp_media_factory_set_launch( factory, _pipelineDscr.c_str() );

    // TODO: for multiple connections to the same stream
//    gst_rtsp_media_factory_set_shared( factory, TRUE );
//    gst_rtsp_media_factory_set_protocols( factory, GST_RTSP_LOWER_TRANS_UDP );
//    gst_rtsp_media_factory_set_rtp_rtcp_muxed( factory, TRUE );

    // set outcoming udp packets address range
    if( _settings.outcomingUdpPacketsRangeBegin != 0 && _settings.outcomingUdpPacketsRangeEnd != 0 ){

        VS_LOG_INFO << PRINT_HEADER << " stream's udp ports range is enable" << endl;

        m_addressPool = gst_rtsp_address_pool_new();
        constexpr int UDP_PACKET_TTL = 0;
        if( ! gst_rtsp_address_pool_add_range( m_addressPool,
                                                GST_RTSP_ADDRESS_POOL_ANY_IPV4,
                                                GST_RTSP_ADDRESS_POOL_ANY_IPV4,
                                                _settings.outcomingUdpPacketsRangeBegin,
                                                _settings.outcomingUdpPacketsRangeEnd,
                                                UDP_PACKET_TTL ) ){
            VS_LOG_ERROR << PRINT_HEADER
                         << " invalid stream's udp ports range: " << _settings.outcomingUdpPacketsRangeBegin
                         << " : " << _settings.outcomingUdpPacketsRangeEnd
                         << endl;

            return false;
        }

        gst_rtsp_media_factory_set_address_pool( factory, m_addressPool );
        gst_rtsp_address_pool_dump( m_addressPool );
    }

    const string postfix = "/" + common_utils::filterByDigitAndNumber( _settings.sourceUrl );

    /* attach the test factory to the /test url */
    gst_rtsp_mount_points_add_factory( mounts, postfix.c_str(), factory );

    /* don't need the ref to the mounts anymore */
    g_object_unref( mounts );

    /* attach the server to the default maincontext */
    m_gstRtspServerId = gst_rtsp_server_attach( m_gstRtspServer, nullptr );

    g_signal_connect( m_gstRtspServer, "client-connected", (GCallback)callbackClientConnected, this );
    g_timeout_add_seconds( 10, (GSourceFunc)callbackCleanupExpiredSessions, m_gstRtspServer );

    /* start serving */
//    m_glibMainLoop = g_main_loop_new( nullptr, FALSE );
//    m_threadGLibMainLoop = new std::thread( g_main_loop_run, m_glibMainLoop );

    _retranslationUrl = "rtsp://" + _settings.listenIp + ":" + std::to_string(_settings.serverPort) + postfix;

    const string msg = ( boost::format( "GST RTSP server started on [%1%] with follow pipeline [%2%] stream udp ports range [%3%:%4%]" )
                         % _retranslationUrl
                         % _pipelineDscr
                         % _settings.outcomingUdpPacketsRangeBegin
                         % _settings.outcomingUdpPacketsRangeEnd
                         ).str();
    VS_LOG_INFO << PRINT_HEADER << " " << msg << endl;

    return true;
}

void VideoRetranslator::stop(){

    const string postfix = "/" + common_utils::filterByDigitAndNumber( m_settings.sourceUrl );

    // remove mount point to prevent new clients connecting
    GstRTSPMountPoints * mounts = gst_rtsp_server_get_mount_points( m_gstRtspServer );
    gst_rtsp_mount_points_remove_factory( mounts, postfix.c_str() );
    g_object_unref( mounts );

    // disconnect clients
    gst_rtsp_server_client_filter( m_gstRtspServer, callbackClientFilter, nullptr );

    // rtsp server instance
    g_main_loop_quit( m_glibMainLoop );
    gst_object_unref( m_gstRtspServer );
    assert( g_source_remove( m_gstRtspServerId ) );    

    // TODO: ?
//    g_main_loop_unref( m_glibMainLoop );

    // thread
    common_utils::threadShutdown( m_threadGLibMainLoop );

    if( m_addressPool ){
        g_object_unref( m_addressPool );
    }

    const bool rt = m_objreprRepository.writeSensorRetranslateUrl( m_settings.sensorId, string() );

    VS_LOG_INFO << PRINT_HEADER
                << " retranslation of source [" << m_settings.sourceUrl << "] is stopped"
                << endl;
}

GstRTSPFilterResult VideoRetranslator::callbackClientFilter( GstRTSPServer * _server, GstRTSPClient * /*_client*/, gpointer /*_userData*/ ){

    // simple filter to shutdown all clients
    VS_LOG_INFO << PRINT_HEADER
                << " client disconnected from server addr: [" << gst_rtsp_server_get_address( _server )
                << "] service [" << gst_rtsp_server_get_service( _server )
                << "] "
                << endl;

    return GST_RTSP_FILTER_REMOVE;
}

void VideoRetranslator::callbackClientConnected( GstRTSPServer * _server, GstRTSPClient * _client, gpointer user_data ){

    VS_LOG_INFO << PRINT_HEADER
                << "client connected to server addr: [" << gst_rtsp_server_get_address( _server )
                << "] service [" << gst_rtsp_server_get_service( _server ) << "]"
                << endl;

    VideoRetranslator * retranslator = static_cast<VideoRetranslator *>( user_data );
    retranslator->watchForClient( _client );

//    gst_rtsp_address_pool_dump( (( RTSPSource * )user_data)->m_addressPool );
}

void VideoRetranslator::watchForClient( GstRTSPClient * _client ){

    g_signal_connect( _client, "describe-request", (GCallback)callbackClientDescribeRequest, this );
    g_signal_connect( _client, "check-requirements", (GCallback)callbackClientCheckRequirements, this );
    g_signal_connect( _client, "send-message", (GCallback)callbackClientSendMessage, this );
}

void VideoRetranslator::callbackClientDescribeRequest( GstRTSPClient * _client, GstRTSPContext * _ctx, gpointer _userData ){

//    LOG_INFO << "===================== DESCRIBE REQUEST FROM CLIENT ====================="
//             << endl;



}

void VideoRetranslator::callbackClientCheckRequirements( GstRTSPClient * _client, GstRTSPContext * _ctx, char ** _arr, gpointer _userData ){

//    LOG_INFO << "===================== CHECK REQUIREMENTS FROM CLIENT ====================="
//             << endl;



}

void VideoRetranslator::callbackClientSendMessage( GstRTSPClient * _client, GstRTSPSession * _session, gpointer _msg, gpointer _userData ){

//    LOG_INFO << "===================== SEND MESSAGE TO CLIENT ===================== : "
//             << endl;





}

gboolean VideoRetranslator::callbackCleanupExpiredSessions( GstRTSPServer * _server ){

    GstRTSPSessionPool * pool = gst_rtsp_server_get_session_pool( _server );
    gst_rtsp_session_pool_cleanup( pool );
    g_object_unref( pool );

//    LOG_INFO << "cleanup expired sessions for server addr: [" << gst_rtsp_server_get_address( _server )
//             << "] service [" << gst_rtsp_server_get_service( _server )
//             << "] "
//             << endl;

    return TRUE;
}

std::string VideoRetranslator::definePipelineDescription( const SInitSettings & _settings ){

    // TODO: for temp - video/x-raw,format=YV12,width=1920,height=1080,framerate=30/1
    // TODO: for temp - application/x-rtp,clock-rate=90000

#ifdef SEPARATE_SINGLE_SOURCE
#ifdef REMOTE_PROCESSING
//    // shared memory ( video/x-raw )
//    const string outputEncoding = OUTPUT_ENCODINGS.find( _settings.outputEncoding )->second;
//    string source;
//    source = ( boost::format( "shmsrc socket-path=%1% do-timestamp=true is-live=true name=shmsrc_raw"
//                              " ! %2%"
//                              " %3%"
//                              )
//               % PATH_LOCATOR.getVideoReceiverRawPayloadSocket( _settings.sourceUrl )
//               % _settings.shmRawStreamCaps
//               % outputEncoding
//               ).str();
//    return source;
    // shared memory ( application/x-rtp )
    string encodingFromRtp;
    auto iter = OUTPUT_ENCODINGS_FROM_RTP.find( _settings.shmRtpStreamEncoding );
    if( iter != OUTPUT_ENCODINGS_FROM_RTP.end() ){
        encodingFromRtp = iter->second;
    }
    else{
        VS_LOG_ERROR << "payloader element not found for encoding: [" << _settings.shmRtpStreamEncoding << "]"
                  << endl;
        return string();
    }
    string source;
    source = ( boost::format( "shmsrc socket-path=%1% do-timestamp=true is-live=true name=shmsrc_raw"
                              " ! %2%"
                              " ! %3%"
                              )
               % PATH_LOCATOR.getVideoReceiverRtpPayloadSocket( _settings.sourceUrl )
               % _settings.shmRtpStreamCaps
               % encodingFromRtp
               ).str();
    return source;
#else
//    // shared memory ( video/x-raw )
//    const string outputEncoding = OUTPUT_ENCODINGS.find( _settings.outputEncoding )->second;
//    string source;
//    source = ( boost::format( "shmsrc socket-path=/tmp/socket_video_server_shm_test do-timestamp=true is-live=true"
//                              " ! video/x-raw,format=YV12,width=1920,height=1080,framerate=30/1"
//                              " %1%"
//                              )
//               % outputEncoding
//               ).str();
//    return source;
    // shared memory ( application/x-rtp )
    const string outputEncoding = OUTPUT_ENCODINGS.find( _settings.outputEncoding )->second;
    string source;
    source = ( boost::format( "shmsrc socket-path=/tmp/socket_video_server_rtpshm_test do-timestamp=true is-live=true"
                              " ! application/x-rtp,clock-rate=90000"
                              " ! rtph264depay ! rtph264pay name=pay0 pt=96"
                              )
               ).str();
    return source;
//    // intervideo
//    const string outputEncoding = OUTPUT_ENCODINGS.find( _settings.outputEncoding )->second;
//    string source;
//    source = ( boost::format( "intervideosrc channel=%1% "
//                              " ! video/x-raw,format=YV12,width=1920,height=1080,framerate=30/1"
//                              " %2%"
//                              )
//               % _settings.intervideosinkChannelName
//               % outputEncoding
//               ).str();
//    return source;
#endif
#else
    const string outputEncoding = OUTPUT_ENCODINGS.find( _settings.outputEncoding )->second;    

    string source;
    // RTSP ( debug=1 for print camera <-> server negotiate, videorate for smoothing )
    if( _settings.sourceUrl.find("rtsp://") != string::npos ){
        source = ( boost::format( "rtspsrc latency=%1% location=%2% "
                                  "! decodebin "
                                  "%3% "
                                  )
                   % _settings.latencyMillisec
                   % _settings.sourceUrl
                   % outputEncoding
                   ).str();
    }
    // HTTP
    else if( _settings.sourceUrl.find("http://") != string::npos ){

        // with id & password
        if( ! _settings.userId.empty() && ! _settings.userPassword.empty() ){
            source = ( boost::format( "souphttpsrc name=source is-live=true do-timestamp=true location=%1% user-id=%2% user-pw=%3% "
                                      "! decodebin "
                                      "%4% "
                                      )
                       % _settings.sourceUrl
                       % _settings.userId
                       % _settings.userPassword
                       % outputEncoding
                       ).str();
        }
        // without authentication
        else{
            source = ( boost::format( "souphttpsrc name=source location=%1% "
                                      "! decodebin "
                                      "%2% "
                                      )
                       % _settings.sourceUrl
                       % outputEncoding
                       ).str();
        }
    }
    else{
        LOG_ERROR << "unsupported protocol:"
                  << " " << _settings.sourceUrl
                  << endl;
        return string();
    }

    return source;
#endif
}

const VideoRetranslator::SCurrentState & VideoRetranslator::getState() const {

    GstRTSPSessionPool * pool = gst_rtsp_server_get_session_pool( m_gstRtspServer );
    guint sessionsCount = gst_rtsp_session_pool_get_n_sessions( pool );
    gst_rtsp_session_pool_cleanup( pool );

    // TODO: or mutable ?
    const_cast<VideoRetranslator::SCurrentState &>( m_state ).clientCount = sessionsCount;
    return m_state;
}


