
#include <regex>

#include <jsoncpp/json/reader.h>
#include <microservice_common/system/logger.h>
#include <microservice_common/communication/network_splitter.h>

#include "system/system_environment_facade_vs.h"
#include "system/config_reader.h"
#include "system/path_locator.h"
#include "common/common_utils.h"
#include "communication/protocols/video_receiver_protocol.h"
#include "video_source_remote2.h"

using namespace std;
using namespace common_types;

static constexpr const char * PRINT_HEADER = "VideoSource2:";
static const std::string PROGRAM_NAME = "video-receiver";

VideoSourceRemote2::VideoSourceRemote2( IInternalCommunication * _internalCommunicationService )
    : IProcessObserver(2)
    , m_referenceCounter(0)
    , m_processHandle(nullptr)
    , m_streamStartEventArrived(false)
    , m_signalExist(false)
    , m_shutdownCalled(false)
    , m_attemptsExhausted(false)
    , m_attemptsCycleActive(false)
    , m_interruptAttempts(false)
    , m_internalCommunicationService(_internalCommunicationService)
{

}

VideoSourceRemote2::~VideoSourceRemote2(){

    shutdown();
}

void VideoSourceRemote2::callbackNetworkRequest( PEnvironmentRequest _request ){

    // filter "noise"
    if( _request->getIncomingMessage().empty() ||
        _request->getIncomingMessage().find("\"answer\":\"duration\"") != std::string::npos ){
        return;
    }

    const string json = _request->getIncomingMessage();
    const AnswerType answer = VideoReceiverProtocol::singleton().getAnswerType( json );

    switch( answer ){
    case AnswerType::AT_STREAMDATA: {
        VS_LOG_INFO << PRINT_HEADER
                    << " AT_STREAMDATA event from receiver"
//                    << " [" << json << "]"
                    << endl;

        m_callbackMsgStreamData = json;
        m_cvResponseEvent.notify_one();
        break;
    }
    case AnswerType::AT_ARCHIVEDATA: {
        VS_LOG_INFO << PRINT_HEADER
                    << " AT_ARCHIVEDATA event from receiver"
//                    << " [" << json << "]"
                    << endl;

        m_callbackMsgArchiveData = json;
        m_cvResponseEvent.notify_one();
        break;
    }
    case AnswerType::AT_EVENT : {
        const EventData ed = VideoReceiverProtocol::singleton().getEventData( json );

        if( EventType::ET_STREAM_START == ed.type ){
            VS_LOG_INFO << PRINT_HEADER
                        << " ET_STREAM_START event from receiver"
//                        << " [" << json << "]"
                        << endl;

            m_streamStartEventArrived = true;
            m_cvResponseEvent.notify_one();

            if( ! m_signalExist ){
                m_signalExist = true;

                for( ISourceObserver * observer : m_observers ){
                    observer->callbackVideoSourceConnection( m_signalExist, m_settings.source.sourceUrl );
                }
            }
        }
        else if( EventType::ET_STREAM_STOP == ed.type ){
            VS_LOG_INFO << PRINT_HEADER
                        << " ET_STREAM_STOP event from receiver"
                        // << " [" << json << "]"
                        << endl;

            if( m_signalExist ){
                m_signalExist = false;

                for( ISourceObserver * observer : m_observers ){
                    observer->callbackVideoSourceConnection( m_signalExist, m_settings.source.sourceUrl );
                }
            }
        }
        break;
    }
    case AnswerType::AT_INFO : {
        const InfoData id = VideoReceiverProtocol::singleton().getInfoData( json );
//        VS_LOG_INFO << PRINT_HEADER
//                    << " AT_INFO from receiver with [" << m_settings.source.sourceUrl << "]"
//                    << " : [" << id.text << "]"
//                    << endl;
        break;
    }
    case AnswerType::AT_Undef : {
        VS_LOG_WARN << PRINT_HEADER << " unknown answer [" << json << "]" << endl;
        break;
    }
    default : {
//        VS_LOG_WARN << PRINT_HEADER << " not yet processable answer type [" << json << "]" << endl;
        break;
    }
    }
}

void VideoSourceRemote2::callbackProcessCrashed( ProcessHandle * _handle ){

    const string sourceUrl = _handle->findArgValue( "url" );
    if( ! sourceUrl.empty() && sourceUrl == m_settings.source.sourceUrl ){

        VS_LOG_INFO << PRINT_HEADER
                    << " catched signal about process crash with source url [" << sourceUrl << "]"
                    << endl;

        // erase any mention about crashed process        
        m_remoteSourceCommunicator.reset();
        m_splittedNetworkForConnectedConsumers.reset();
        m_processHandle = nullptr;
        m_settings.systemEnvironment->serviceForWriteAheadLogging()->closeProcessEvent( _handle->getChildPid() );

        // notify
        if( m_signalExist ){
            m_signalExist = false;

            for( ISourceObserver * observer : m_observers ){
                observer->callbackVideoSourceConnection( m_signalExist, m_settings.source.sourceUrl );
            }
        }

        // async connection
        if( ! m_shutdownCalled
            && ! m_attemptsCycleActive
            && m_settings.connectRetriesTimeoutSec != IVideoSource::CONNECT_RETRIES_IMMEDIATELY
        ){
            m_futureCallbackConnectAttempts = std::async( std::launch::async,
                                                  & VideoSourceRemote2::makeConnectAttempt,
                                                  this,
                                                  m_settings );
        }
    }
}

void VideoSourceRemote2::attemptsCycle(){

    m_attemptsCycleActive = true;

    const int64_t attemptsBeginMilllisec = common_utils::getCurrentTimeMillisec();

    // attempts
    while( (common_utils::getCurrentTimeMillisec() - attemptsBeginMilllisec) <= (m_settings.connectRetriesTimeoutSec * 1000 )
           && ! m_signalExist
           && ! m_shutdownCalled
           && ! m_interruptAttempts ){

        if( makeConnectAttempt( m_settings ) ){
            break;
        }        
    }

    if( (common_utils::getCurrentTimeMillisec() - attemptsBeginMilllisec) > (m_settings.connectRetriesTimeoutSec * 1000 ) ){
        m_attemptsExhausted = true;
    }

    // print result of attempts
    if( ! m_signalExist ){
        if( m_interruptAttempts ){
            VS_LOG_INFO << PRINT_HEADER
                        << " exit from cycle of attempts due to INTERRUPT"
                        << endl;
        }

        if( m_attemptsExhausted ){
            VS_LOG_WARN << PRINT_HEADER
                        << " signal waiting time is EXHAUSTED"
                        << " was started [" << common_utils::timeMillisecToStr( attemptsBeginMilllisec ) << "]"
                        << " end [" << common_utils::timeMillisecToStr( common_utils::getCurrentTimeMillisec() ) << "]"
                        << " timeous sec [" << m_settings.connectRetriesTimeoutSec << "]"
                        << endl;
        }

        destroyRecources();
    }
    else{
        VS_LOG_INFO << PRINT_HEADER
                    << " exit from cycle of attempts with SIGNAL"
                    << endl;
    }

    m_attemptsCycleActive = false;
}

bool VideoSourceRemote2::makeConnectAttempt( const SInitSettings & _settings ){

    std::lock_guard<std::mutex> lock( m_mutexAttemptArea );

    const int64_t attemptsIntervalMillisec = 1000;
    std::this_thread::sleep_for( std::chrono::milliseconds(attemptsIntervalMillisec) );

    VS_LOG_INFO << endl << endl << endl
                << PRINT_HEADER
                << " connect ATTEMPT to URL [" << _settings.source.sourceUrl << "]"
                << endl;

    const bool rt = launchSourceProcess( _settings );
    if( ! rt ){
        return false;
    }

    const bool rt2 = establishCommunicationWithProcess( _settings.source.sourceUrl );
    if( ! rt2 ){
        return false;
    }

    const bool rt3 = waitForStreamStartEvent();
    if( ! rt3 ){
        return false;
    }
    else{
        return true;
    }
}

bool VideoSourceRemote2::connect( const SInitSettings & _settings ){

    // url validation
    if( ! isUrlValid(_settings.source.sourceUrl) ){
        return false;
    }

    // ref counting
    if( m_processHandle ){
        m_referenceCounter++;
        VS_LOG_WARN << PRINT_HEADER
                    << " [" << _settings.source.sourceUrl
                    << "] already connected. RefCount (after inc): " << m_referenceCounter
                    << endl;
        return true;
    }

    m_settings = _settings;

    // sync connection directly
    if( makeConnectAttempt(_settings) ){
        m_referenceCounter++;

        VS_LOG_INFO << PRINT_HEADER
                    << " RefCount for [" << _settings.source.sourceUrl << "]"
                    << " (after inc): " << m_referenceCounter
                    << endl;
        return true;
    }
    else{
        if( _settings.connectRetriesTimeoutSec == IVideoSource::CONNECT_RETRIES_IMMEDIATELY ){
            return false;
        }
        else{
            m_interruptAttempts = false;
            m_futureCycleConnectAttempts = std::async( std::launch::async, & VideoSourceRemote2::attemptsCycle, this );

            while( ! m_signalExist && ! m_attemptsExhausted && ! m_shutdownCalled && ! m_interruptAttempts ){
                // dummy
            }

            return m_signalExist;
        }
    }
}

void VideoSourceRemote2::disconnect(){

    // stop attempts if they exist
    if( m_attemptsCycleActive ){
        m_interruptAttempts = true;
    }

    // avoid initial state
    if( ! m_processHandle || 0 == m_referenceCounter ){
        VS_LOG_INFO << PRINT_HEADER
                    << " [" << m_settings.source.sourceUrl << "] disconnect not needed."
                    << " ProcessHandle [" << m_processHandle << "]"
                    << " RefCount [" << m_referenceCounter << "]"
                    << endl;
        return;
    }

    // check count
    m_referenceCounter--;
    if( ! m_shutdownCalled && m_referenceCounter > 0 ){
        VS_LOG_WARN << PRINT_HEADER
                    << " [" << m_settings.source.sourceUrl << "]"
                    << " still in use. Disconnect aborted. RefCount (after dec): " << m_referenceCounter
                    << endl;
        return;
    }

    // begin
    VS_LOG_INFO << PRINT_HEADER
                << " [" << m_settings.source.sourceUrl << "]"
                << " will be disconnect. RefCount (after dec): " << m_referenceCounter
                << " hot shutdown: " << (m_shutdownCalled ? "TRUE" : "FALSE")
                << endl;

    // soft stop of archiving on video-receiver side
    if( CONFIG_PARAMS.RECORD_ENABLE ){
        if( m_remoteSourceCommunicator ){
            const string msgToSend = VideoReceiverProtocol::singleton().createSimpleCommand( CommandType::CT_STOP );

            PEnvironmentRequest request = m_remoteSourceCommunicator->getRequestInstance();
            request->setOutcomingMessage( msgToSend );
        }
    }

    destroyRecources();
}

void VideoSourceRemote2::addObserver( ISourceObserver * _observer ){
    m_observers.push_back( _observer );
}

PNetworkClient VideoSourceRemote2::getCommunicator(){
    return m_splittedNetworkForConnectedConsumers;
}

int32_t VideoSourceRemote2::getReferenceCount(){
    return m_referenceCounter;
}

ISourcesProvider::SSourceParameters VideoSourceRemote2::getStreamParameters(){

    if( ! m_remoteSourceCommunicator ){
        VS_LOG_ERROR << PRINT_HEADER
                     << " get stream parameters failed, network client is absent"
                     << endl;
        return ISourcesProvider::SSourceParameters();
    }

    // NOTE: такой подход корректен только в случае когда время жизни ресивера совпадает со временем жизни VideoSourceRemote
    string response;
    m_callbackMsgStreamData.clear();

    // request
    const string msgToSend = VideoReceiverProtocol::singleton().createSimpleCommand( CommandType::CT_GET_STREAM_INFO );

    PEnvironmentRequest request = m_remoteSourceCommunicator->getRequestInstance();
    request->setOutcomingMessage( msgToSend );

    std::mutex lockMutex;
    std::unique_lock<std::mutex> cvLock( lockMutex );
    m_cvResponseEvent.wait( cvLock, [this](){ return ! m_callbackMsgStreamData.empty(); } );
    response = m_callbackMsgStreamData;

    // response
    Json::Reader reader;
    Json::Value parsedRecord;
    if( ! reader.parse( response.c_str(), parsedRecord, false ) ){
        VS_LOG_ERROR << PRINT_HEADER
                     << " parse failed of [1] Reason: [2] ["
                     << response << "] ["
                     << reader.getFormattedErrorMessages()  << "]"
                     << endl;

        return ISourcesProvider::SSourceParameters();
    }

    ISourcesProvider::SSourceParameters out;
    out.rtpStreamEnconding = parsedRecord["source_encoding"].asString();
    out.rtpStreamCaps = parsedRecord["rtp_caps"].asString();
    out.rawStreamCaps = parsedRecord["raw_caps"].asString();
    out.frameWidth = getWidthHeight( out.rawStreamCaps ).first;
    out.frameHeight = getWidthHeight( out.rawStreamCaps ).second;

    if( out.rtpStreamEnconding.empty() || out.rtpStreamCaps.empty() ){
        VS_LOG_WARN << PRINT_HEADER
                    << " from video-receiver ENCODING or RTP-CAPS is empty."
                    << " Response [" << response << "]"
                    << endl;
    }

    return out;
}

std::pair<int, int> VideoSourceRemote2::getWidthHeight( const std::string & _caps ){

    std::pair<int, int> out;

    // NOTE: lookbehind by ECMAScript standart

    std::regex rgx( "\\width=\\(int\\)([0-9]+)" );
    std::smatch match;

    if( std::regex_search(_caps.begin(), _caps.end(), match, rgx) ){
        out.first = std::stoi( match[ 1 ] );
    }

    std::regex rgx2( "\\height=\\(int\\)([0-9]+)" );
    std::smatch match2;

    if( std::regex_search(_caps.begin(), _caps.end(), match2, rgx2) ){
        out.second = std::stoi( match2[ 1 ] );
    }

    return out;
}

bool VideoSourceRemote2::isUrlValid( const std::string & _url ){

    static const vector<string> allowedProtocols = { "http://", "rtsp://" };

    if( _url.empty() ){
        stringstream ss;
        ss << "video source url string is empty";
        VS_LOG_INFO << PRINT_HEADER << " " << ss.str() << endl;
        m_lastError = ss.str();
        return false;
    }

    for( const string & prot : allowedProtocols ){
        if( _url.find(prot) != string::npos ){
            return true;
        }
    }

    string availableProtocolsStr;
    for( const string & prot : allowedProtocols ){
        availableProtocolsStr += "[";
        availableProtocolsStr += prot;
        availableProtocolsStr += "]";
    }

    stringstream ss;
    ss << "NO ONE allowed protocol in URL string is found [" << _url << "]"
       << " available: " << availableProtocolsStr;
    VS_LOG_ERROR << PRINT_HEADER << " " << ss.str() << endl;
    m_lastError = ss.str();
    return false;
}

bool VideoSourceRemote2::launchSourceProcess( const SInitSettings & _settings ){

    if( m_processHandle ){
        return true;
    }

    // т.к. ресивер применяется всегда, но писать должен только при команде "Архивация" или "Анализ"

    // TODO: what to do with this magic numbers/words ?
    int cacheSizeSec = 0;
    if( CONFIG_PARAMS.RECORD_ENABLE && CONFIG_PARAMS.RECORD_ENABLE_REMOTE_ARCHIVER ){
        cacheSizeSec = CONFIG_PARAMS.RECORD_NEW_DATA_BLOCK_INTERVAL_MIN * 60;
    }
    const float recordOverlay = 0.25;
    const string rawStreamFormat = "I420";
    const string archiveFormat = ( CONFIG_PARAMS.RECORD_ENABLE ? "hls" : "mp4" );
    const string cleanOnFinish = "false";
    const string verboseOutput = "false";
    const string sourceName = "TODO: do";

    // process
    std::vector<std::string> args;
    args.push_back( "--url=" + _settings.source.sourceUrl );
    args.push_back( "--source-name=" + sourceName );
    args.push_back( "--socket-control=" + PATH_LOCATOR.getVideoReceiverControlSocket(_settings.source.sourceUrl) );
    args.push_back( "--socket-shm=" + PATH_LOCATOR.getVideoReceiverRawPayloadSocket(_settings.source.sourceUrl) );
    args.push_back( "--socket-rtpshm=" + PATH_LOCATOR.getVideoReceiverRtpPayloadSocket( _settings.source.sourceUrl ) );
    args.push_back( "--max-record-overlay=" + std::to_string(recordOverlay) );

    args.push_back( "--cache-size=" + std::to_string(cacheSizeSec) );
    args.push_back( "--cache-dir=" + CONFIG_PARAMS.HLS_WEBSERVER_DIR_ROOT );
    args.push_back( "--cache-base-url=" + CONFIG_PARAMS.HLS_WEBSERVER_URL_ROOT );
    args.push_back( "--raw-stream-format=" + rawStreamFormat );
    args.push_back( "--archive-format=" + archiveFormat );
    args.push_back( "--clean-on-finish=" + cleanOnFinish );
    args.push_back( "--verbose=" + verboseOutput );

    PROCESS_LAUNCHER.addObserver( this, 0 );

    // NOTE: streaming will start while video-receiver launch
    const ProcessLauncher::SLaunchTask * launchTask = PROCESS_LAUNCHER.addLaunchTask( PROGRAM_NAME, args, CONFIG_PARAMS.baseParams.SYSTEM_CATCH_CHILD_PROCESSES_OUTPUT );
    while( ! launchTask->launched.load() ){
        // dummy
    }

    if( ! launchTask->handle || ! launchTask->handle->isRunning() ){
        VS_LOG_ERROR << PRINT_HEADER << " process launch failed [" << PROGRAM_NAME << "]" << endl;
        return false;
    }

    m_processHandle = launchTask->handle;

    SWALProcessEvent eventForWAL;
    eventForWAL.pid = m_processHandle->getChildPid();
    eventForWAL.programName = PROGRAM_NAME;
    eventForWAL.programArgs = args;
    m_settings.systemEnvironment->serviceForWriteAheadLogging()->openProcessEvent( eventForWAL );

    return true;
}

bool VideoSourceRemote2::establishCommunicationWithProcess( const std::string & _sourceUrl ){

    if( m_remoteSourceCommunicator ){
        return true;
    }

    m_remoteSourceCommunicator = m_internalCommunicationService->getSourceCommunicator( _sourceUrl );

    // connect to video-receiver
    int triesCount = 5;
    constexpr int connectTryIntervalMillisec = 1000;
    while( ! m_remoteSourceCommunicator && --triesCount > 0 ){
        std::this_thread::sleep_for( chrono::milliseconds(connectTryIntervalMillisec) );

        m_remoteSourceCommunicator = m_internalCommunicationService->getSourceCommunicator( _sourceUrl );
    }

    if( ! m_remoteSourceCommunicator ){
        VS_LOG_ERROR << PRINT_HEADER
                     << " connection with remote process failed for URL [" << _sourceUrl << "]"
                     << endl;
        return false;
    }

    PNetworkProvider provider = std::dynamic_pointer_cast<INetworkProvider>( m_remoteSourceCommunicator );
    provider->addObserver( this );

    // split for other consumers
    NetworkSplitter::SInitSettings settings;
    settings.m_realCommunicator = m_remoteSourceCommunicator;
    PNetworkSplitter splittedNetwork = std::make_shared<NetworkSplitter>( INetworkEntity::INVALID_CONN_ID );
    if( ! splittedNetwork->init(settings) ){
        VS_LOG_ERROR << PRINT_HEADER
                     << " splitted network create failed"
                     << endl;
        return false;
    }

    m_splittedNetworkForConnectedConsumers = splittedNetwork;

    VS_LOG_INFO << PRINT_HEADER
                << " connection with video-receiver for URL [" << _sourceUrl << "] established"
                << endl;
    return true;
}

bool VideoSourceRemote2::waitForStreamStartEvent(){

    if( m_signalExist ){
        return true;
    }

    // check for stream start event
    if( CONFIG_PARAMS.RECORD_ENABLE && ! m_streamStartEventArrived ){
        std::mutex lockMutex;
        std::unique_lock<std::mutex> cvLock( lockMutex );
        constexpr int timeoutSec = 5;
        m_cvResponseEvent.wait_for( cvLock,
                                                   chrono::seconds(timeoutSec),
                                                   [this](){ return m_streamStartEventArrived; } );

        if( ! m_streamStartEventArrived ){
            VS_LOG_WARN << PRINT_HEADER
                        << " stream start event not arrived in [" << timeoutSec << "] sec"
                        << endl;
        }
        else{
            m_streamStartEventArrived = false;
            return true;
        }
    }

    // event missed - demand it from client side
    const string streamCaps = getStreamParameters().rawStreamCaps;
    if( streamCaps.empty() ){
        VS_LOG_ERROR << PRINT_HEADER
                     << " raw stream caps is empty. Waiting 'stream start' event failed"
                     << endl;
        return false;
    }

    return true;
}

void VideoSourceRemote2::destroyRecources(){

    // close connection
    if( m_remoteSourceCommunicator ){
        PNetworkProvider provider = std::dynamic_pointer_cast<INetworkProvider>( m_remoteSourceCommunicator );
        provider->removeObserver( this );

        m_remoteSourceCommunicator.reset();
        m_splittedNetworkForConnectedConsumers.reset();
    }

    if( m_processHandle ){
        m_settings.systemEnvironment->serviceForWriteAheadLogging()->closeProcessEvent( m_processHandle->getChildPid() );
    }

    // terminate video-receiver process
    PROCESS_LAUNCHER.close( m_processHandle );
    PROCESS_LAUNCHER.removeObserver( this );

    PATH_LOCATOR.removeVideoReceiverSocketsEnvironment( m_settings.source.sourceUrl );
}

void VideoSourceRemote2::shutdown(){

    if( ! m_shutdownCalled ){
        m_shutdownCalled = true;

        disconnect();
    }
}

const VideoSourceRemote2::SInitSettings & VideoSourceRemote2::getSettings(){
    return m_settings;
}

const std::string & VideoSourceRemote2::getLastError(){
    return m_lastError;
}







