
#include <iostream>

#include <boost/format.hpp>
#include <microservice_common/system/logger.h>

#include "common/common_utils.h"
#include "system/config_reader.h"
#include "system/path_locator.h"
#include "archive_creator.h"

using namespace std;
using namespace common_types;

static constexpr const char * PRINT_HEADER = "Archiver:";

/*
PIPELINE:

rtspsrc name=source latency=100 message-forward=true buffer-mode=4 location=rtsp://admin:depart21@hiwatch2 tee name=rtp allow-not-linked=true ! queue leaky=2 ! decodebin name=decbin

tee name=raw allow-not-linked=true

raw. ! queue leaky=2 ! videoconvert name=blockpoint ! capsfilter caps=video/x-raw,format=BGR !

tee allow-not-linked=true name=checkpoint

raw. ! queue leaky=2 ! videoconvert ! capsfilter caps=video/x-raw,format=I420 ! identity name=filterpoint ! videobalance name=balance ! videoconvert ! videorate ! capsfilter caps=video/x-raw,format=I420 ! valve name=vl ! shmsink socket-path=/tmp/video_server_vr_raw_payload_sock_admin:depart21@hiwatch2_4 shm-size=300000000 wait-for-connection=1 sync=true name=shm

rtp. ! queue leaky=0 ! capsfilter caps=application/x-rtp,encoding-name=H264 ! rtph264depay ! h264parse disable-passthrough=true config-interval=-1 ! mpegtsmux ! hlssink name=hlssink target-duration=2 playlist-length=0 max-files=60

rtp. ! queue leaky=2 ! shmsink socket-path=/tmp/video_server_vr_rtp_payload_sock_admin:depart21@hiwatch2_4 shm-size=300000000 wait-for-connection=1 sync=true name=rtpshm
*/

static const std::map<string, string> OUTPUT_ENCODINGS_FROM_RTP = {
    {"H264", "rtph264depay ! h264parse disable-passthrough=true config-interval=-1 ! mpegtsmux ! hlssink name=hlssink target-duration=2 playlist-length=0 max-files=60"},
    {"JPEG", " ! "},
    {"MP4V-ES", " ! "}
                                                                  };

ArchiveCreatorLocal::ArchiveCreatorLocal()
    : m_gstPipeline( nullptr )
    , m_glibMainLoop( nullptr )
    , m_threadGLibMainLoop( nullptr )
    , m_connectionLosted(false)
{

}

ArchiveCreatorLocal::~ArchiveCreatorLocal(){

    stop();
}

bool ArchiveCreatorLocal::start( const SInitSettings & _settings ){

    m_settings = _settings;
    m_currentState.initSettings = & m_settings;

    //
    if( ! getArchiveSessionInfo(m_currentState.timeStartMillisec, m_currentState.sessionName, m_currentState.playlistName) ){
        return false;
    }

    //
    if( ! launchGstreamPipeline(_settings) ){
        return false;
    }

    //
    SHlsMetadata meta;
    meta.sourceUrl = _settings.sourceUrl;
    meta.sensorId = _settings.sensorId;
    meta.timeStartMillisec = m_currentState.timeStartMillisec;
    meta.timeEndMillisec = 0;
    meta.playlistWebLocationFullPath = CONFIG_PARAMS.HLS_WEBSERVER_URL_ROOT + "/" + m_currentState.sessionName + "/" + m_currentState.playlistName;

    if( ! AArchiveCreator::m_objreprRepository.updateArchiveStatus(meta, true) ){
        VS_LOG_ERROR << "archive [" << _settings.sourceUrl << "] 'begin' marker update failed" << endl;
        return false;
    }

    //
    const string saveLocation = CONFIG_PARAMS.HLS_WEBSERVER_DIR_ROOT + "/" + m_currentState.sessionName;
    if( ! AArchiveCreator::createMasterPlaylist(saveLocation, _settings) ){
        return false;
    }

    //
    const string urlLocation = CONFIG_PARAMS.HLS_WEBSERVER_URL_ROOT + "/" + m_currentState.sessionName;
    m_liveStreamPlaylistDirFullPath = AArchiveCreator::createLiveStreamPlaylist( saveLocation, urlLocation, meta, _settings );
    if( m_liveStreamPlaylistDirFullPath.empty() ){
        return false;
    }

    //
    m_originalPlaylistFullPath = saveLocation + "/" + m_currentState.playlistName;
    m_currentState.state = EArchivingState::ACTIVE;
    m_closed = false;

    // NOTE: recording will start with video-receiver launch
    VS_LOG_INFO << PRINT_HEADER
                << " archive open [" << _settings.sourceUrl << "]"
                << " time start [" << meta.timeStartMillisec << "]"
                << " playlist web full path [" << meta.playlistWebLocationFullPath << "]"
                << endl;

    return true;
}

bool ArchiveCreatorLocal::launchGstreamPipeline( const SInitSettings & _settings ){

    const string pipelineDscr = definePipelineDescription( _settings );
    if( pipelineDscr.empty() ){
        return false;
    }

    // launch
    GError * gerr = nullptr;
    m_gstPipeline = gst_parse_launch( pipelineDscr.c_str(), & gerr );
    if( gerr ){
        VS_LOG_ERROR << "HLSCreator - couldn't construct pipeline: [1], gst message: [2] " << pipelineDscr << " " << gerr->message << endl;
        g_clear_error( & gerr );
        return false;
    }

    // to be notified of messages from this pipeline, mostly EOS
    GstBus * bus = gst_element_get_bus( m_gstPipeline );
    gst_bus_add_watch( bus, callbackGstSourceMessage, this );
    gst_object_unref( bus );

    updateHlsSinkProperties();

    // state
    gst_element_set_state( GST_ELEMENT( m_gstPipeline ), GST_STATE_PLAYING );

    // run loop
//    m_glibMainLoop = g_main_loop_new( NULL, FALSE );
//    m_threadGLibMainLoop = new std::thread( g_main_loop_run, m_glibMainLoop );

    const string msg = ( boost::format( "archiver started with follow GSt pipeline [%1%]" )
                         % pipelineDscr
                         ).str();
    VS_LOG_INFO << msg << endl;

    return true;
}

bool ArchiveCreatorLocal::updateHlsSinkProperties(){

    GstElement * hlssink = gst_bin_get_by_name( GST_BIN(m_gstPipeline), "hlssink" );
    if( ! hlssink ){
        std::cerr << "ERROR: HLSSINK element not found in pipeline" << std::endl;
        return false;
    }

    static const std::string DEFAULT_CHUNCK_TEMPLATE = "archive_%05d.ts";

    // место-шаблон куда будут складываться чанки
    const std::string chunksLocation = CONFIG_PARAMS.HLS_WEBSERVER_DIR_ROOT + "/" + m_currentState.sessionName + "/" + DEFAULT_CHUNCK_TEMPLATE;
    g_object_set( hlssink, "location", chunksLocation.c_str(), NULL );

    // префикс http-адреса до директории сессии для адресации к чанкам ( http://lenin:8888/archive_session_N/ + some chunk in playlist )
    const std::string playlistWebRoot = CONFIG_PARAMS.HLS_WEBSERVER_URL_ROOT + "/" + m_currentState.sessionName;
    g_object_set( hlssink, "playlist-root", playlistWebRoot.c_str(), NULL );

    // место создания плейлиста
    const std::string playlistDirFullLocation = CONFIG_PARAMS.HLS_WEBSERVER_DIR_ROOT + "/" + m_currentState.sessionName + "/" + m_currentState.playlistName;
    g_object_set( hlssink, "playlist-location", playlistDirFullLocation.c_str(), NULL );

    VS_LOG_INFO << PRINT_HEADER << " UPDATE HLSSINK 'location' " << chunksLocation << std::endl;
    VS_LOG_INFO << PRINT_HEADER << " UPDATE HLSSINK 'playlist-root' " << playlistWebRoot << std::endl;
    VS_LOG_INFO << PRINT_HEADER << " UPDATE HLSSINK 'playlist-location' " << playlistDirFullLocation << std::endl;

    return true;
}

void ArchiveCreatorLocal::stop(){

    if( m_closed ){
        return;
    }

    pauseGstreamPipeline();

    // edit objrepr object
    SHlsMetadata meta;
    meta.sourceUrl = m_settings.sourceUrl;
    meta.timeStartMillisec = m_currentState.timeStartMillisec;
    meta.timeEndMillisec = common_utils::getCurrentTimeMillisec();
    meta.playlistDirLocationFullPath = "";

    if( ! AArchiveCreator::m_objreprRepository.updateArchiveStatus(meta, false) ){
        VS_LOG_ERROR << "archive [" << m_settings.sourceUrl << "] 'end' marker update failed" << endl;
        return;
    }

    // final update of live stream playlist
    AArchiveCreator::transferOriginalPlaylistToLiveStream( m_originalPlaylistFullPath, m_liveStreamPlaylistDirFullPath );

    //
    AArchiveCreator::checkOriginalPlaylistConsistent( m_originalPlaylistFullPath );

    m_closed = true;
    m_currentState.state = EArchivingState::READY;

    VS_LOG_INFO << "archiver on  [" << m_settings.sourceUrl << "] is stopped"
                << endl;
}

void ArchiveCreatorLocal::pauseGstreamPipeline(){

    // TODO: find out memory leak on delete Hls Wrapper
    if( ! m_gstPipeline ){
        return;
    }

    // stop pipeline
    gst_element_send_event( m_gstPipeline, gst_event_new_eos() );

    GstStateChangeReturn ret = gst_element_set_state( m_gstPipeline, GST_STATE_PAUSED );
    ret = gst_element_set_state( m_gstPipeline, GST_STATE_NULL );
    if( GST_STATE_CHANGE_FAILURE == ret ){
        VS_LOG_ERROR << "Unable to set the pipeline to the null state." << endl;
    }

    // destroy pipeline
    g_object_run_dispose( G_OBJECT( m_gstPipeline ) );
    gst_object_unref( m_gstPipeline );
    m_gstPipeline = nullptr;

    // destroy loop
    if( m_glibMainLoop ){
        g_main_loop_quit( m_glibMainLoop );
        g_main_loop_unref( m_glibMainLoop );
    }

    common_utils::threadShutdown( m_threadGLibMainLoop );
}

void ArchiveCreatorLocal::tick(){

    AArchiveCreator::transferOriginalPlaylistToLiveStream( m_originalPlaylistFullPath, m_liveStreamPlaylistDirFullPath );
}

bool ArchiveCreatorLocal::getArchiveSessionInfo(    int64_t & _startRecordTimeMillisec,
                                                    std::string & _sessionName,
                                                    std::string & _playlistName ){

    // generate
    const int64_t archiveCreateTimeMillisec = common_utils::getCurrentTimeMillisec();

    static const std::string DEFAULT_SESSION_TEMPLATE = "%1%_%2%";
    const string url = common_utils::filterByDigitAndNumber( m_settings.sourceUrl );
    std::string sessionName = ( boost::format( DEFAULT_SESSION_TEMPLATE )
                                % url
                                % ( archiveCreateTimeMillisec / 1000 )
                              ).str();

    static const std::string DEFAULT_PLAYLIST_NAME = "live_stream.m3u8";

    // create resources
    const std::string fullPath = CONFIG_PARAMS.HLS_WEBSERVER_DIR_ROOT + "/" + sessionName;
    const int res = ::mkdir( fullPath.c_str(), 0755 );
    if( -1 == res ){
        stringstream ss;
        ss << " can't create archive directory [" << fullPath << "]"
           << " for session [" << sessionName << "]"
           << " reason [" << strerror( errno ) << "]"
           << std::endl;
        VS_LOG_ERROR << PRINT_HEADER << ss.str() << endl;
        m_lastError = ss.str();
        return false;
    }

    // set
    _startRecordTimeMillisec = archiveCreateTimeMillisec;
    _sessionName = sessionName;
    _playlistName = DEFAULT_PLAYLIST_NAME;

    return true;
}

/* called when we get a GstMessage from the source pipeline when we get EOS, we notify the appsrc of it. */
gboolean ArchiveCreatorLocal::callbackGstSourceMessage( GstBus * _bus, GstMessage * _message, gpointer _userData ){

    switch( GST_MESSAGE_TYPE(_message) ){
    case GST_MESSAGE_EOS : {
        g_print("The source got dry ('END OF STREAM' message)");
        break;
    }
    case GST_MESSAGE_ERROR : {
        GError * err;
        gchar * debug;
        gst_message_parse_error( _message, & err, & debug );

        VS_LOG_ERROR << "LocalArchiver received error from GstObject: [" << _bus->object.name
                  << "] msg: [" << err->message
                  << "] Debug [" << debug
                  << "]"
                  << endl;

        g_error_free( err );
        g_free( debug );

        g_main_loop_quit( (( ArchiveCreatorLocal * )_userData)->m_glibMainLoop );

        (( ArchiveCreatorLocal * )_userData)->m_currentState.state = EArchivingState::CRUSHED;
        break;
    }
    case GST_MESSAGE_STATE_CHANGED : {

        break;
    }
    case GST_MESSAGE_BUFFERING : {

        break;
    }
    case GST_MESSAGE_ELEMENT : {

        break;
    }
    default:
        break;
    }

    return TRUE;
}

void ArchiveCreatorLocal::tryToReconnect(){

    constexpr int32_t reconnectAfterSec = 5;

    VS_LOG_WARN << "may be lost signal from [" << m_settings.sourceUrl << "]. Retry to connect after [" << reconnectAfterSec << "] seconds" << endl;
    gst_element_set_state( GST_ELEMENT( m_gstPipeline ), GST_STATE_READY );

    this_thread::sleep_for( chrono::seconds( CONFIG_PARAMS.baseParams.SYSTEM_CONNECT_RETRY_PERIOD_SEC / 2 ) );

//    gst_element_set_state( GST_ELEMENT( m_gstPipeline ), GST_STATE_PAUSED );

    VS_LOG_WARN << "retry to connect" << endl;
    this_thread::sleep_for( chrono::seconds( CONFIG_PARAMS.baseParams.SYSTEM_CONNECT_RETRY_PERIOD_SEC / 2 ) );

    gst_element_set_state( GST_ELEMENT( m_gstPipeline ), GST_STATE_PLAYING );
}

std::string ArchiveCreatorLocal::definePipelineDescription( const SInitSettings & _settings ){

#ifdef SEPARATE_SINGLE_SOURCE
#ifdef REMOTE_PROCESSING

    ISourcesProvider::SSourceParameters sourceParameters;
    if( _settings.serviceOfSourceProvider ){
        sourceParameters = _settings.serviceOfSourceProvider->getShmRtpStreamParameters( _settings.sourceUrl );
    }

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
    auto iter = OUTPUT_ENCODINGS_FROM_RTP.find( sourceParameters.rtpStreamEnconding );
    if( iter != OUTPUT_ENCODINGS_FROM_RTP.end() ){
        encodingFromRtp = iter->second;
    }
    else{
        VS_LOG_ERROR << "payloader element not found for encoding: [" << sourceParameters.rtpStreamEnconding << "]"
                  << endl;
        return string();
    }
    string source;
    source = ( boost::format( "shmsrc socket-path=%1% do-timestamp=true is-live=true name=shmsrc_raw"
                              " ! %2%"
                              " ! %3%"
                              )
               % PATH_LOCATOR.getVideoReceiverRtpPayloadSocket( _settings.sourceUrl )
               % sourceParameters.rtpStreamCaps
               % encodingFromRtp
               ).str();
    return source;
#else
    string source;
    const int segmentDurationSec = 1;

    source += ( boost::format( "intervideosrc channel=%1% "
                               "! x264enc speed-preset=ultrafast "
                               "! mpegtsmux "
                               "! hlssink name=hls playlist-location=%2% playlist-root=%3% max-files=%4% target-duration=%5% playlist-length=%6%"
                             )
                         % _settings.intervideosinkChannelName
                         % _settings.playlistDirLocationFullPath
                         % _settings.playlistWebLocationPath
                         % _settings.maxFiles
                         % segmentDurationSec
                         % _settings.playlistLength
                         ).str();
#endif
#else
    string source;

    if( _settings.sourceUrl.find( "rtsp://" ) != string::npos ){
        source += ( boost::format( "rtspsrc location= %1% "
                                   "! decodebin "
                                   "! x264enc "
                                   "! mpegtsmux "
                                   "! hlssink name=hls playlist-location=%2% playlist-root=%3% max-files=%4% target-duration=1 playlist-length=%5%"
                                 )
                             % _settings.sourceUrl
                             % _settings.playlistLocation
                             % _settings.playlistRoot
                             % _settings.maxFiles
                             % _settings.playlistLength
                             ).str();
    }
//    else if( _settings.sourceUrl.find( "file://" ) != string::npos ){
//        source = "filesrc location=";
//    }
//    else if( _settings.sourceUrl.find( "rtmp://" ) != string::npos ){
//        source = "rtmpsrc do-timestamp=true location=";
//    }
//    else if( _settings.sourceUrl.find( "udp://" ) != string::npos ){
//        source = "udpsrc location=";
//    }
//    else if( _settings.sourceUrl.find( "http://" ) != string::npos ){
//        source += ( boost::format( "rtspsrc location= %1% "
//                                   "! decodebin "
//                                   "! jpegenc "
//                                   "! mpegtsmux "
//                                   "! hlssink name=hls playlist-location=%2% playlist-root=%3% max-files=%4% target-duration=1 playlist-length=%5%"
//                                 )
//                             % _settings.sourceUrl
//                             % _settings.playlistLocation
//                             % _settings.playlistRoot
//                             % _settings.maxFiles
//                             % _settings.playlistLength
//                             ).str();
//    }
    else{
        LOG_ERROR << "unsupported protocol:"
                  << " " << _settings.sourceUrl
                  << endl;
        return string();
    }
#endif

    return source;
}
