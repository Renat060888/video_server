
#include <thread>

#include <jsoncpp/json/reader.h>
#include <microservice_common/system/logger.h>
#include <microservice_common/communication/network_splitter.h>

#include "communication/protocols/video_receiver_protocol.h"
#include "system/config_reader.h"
#include "system/path_locator.h"
#include "video_source_remote.h"

// TODO: for crutch
#include "storage/video_recorder.h"

static constexpr const char * PRINT_HEADER = "VideoSource:";
static const std::string PROGRAM_NAME = "video-receiver";

using namespace std;
using namespace common_types;

VideoSourceRemote::VideoSourceRemote( IInternalCommunication * _internalCommunicationService, const std::string & _clientName )
    : IProcessObserver(2)
    , m_referenceCounter(0)
    , m_internalCommunicationService(_internalCommunicationService)
    , m_processHandle(nullptr)
    , m_signalLosted(false)
    , m_correlatedProcessCrashed(false)
    , m_connectCallFinished(false)
    , m_disconnectCalled(false)
    , m_requestInited(false)
    , m_shutdown(false)
{

}

VideoSourceRemote::~VideoSourceRemote(){

    // NOTE: DTOR also must be called for SharedPointer decrement
    VS_LOG_TRACE << PRINT_HEADER << " [" << m_settings.source.sourceUrl << "] DTOR" << endl;

    disconnect();
}

const VideoSourceRemote::SInitSettings & VideoSourceRemote::getSettings(){
    return m_settings;
}

const std::string & VideoSourceRemote::getLastError(){
    return m_lastError;
}

void VideoSourceRemote::addObserver( ISourceObserver * _observer ){
    m_observers.push_back( _observer );
}

PNetworkClient VideoSourceRemote::getCommunicator(){
//    return m_remoteSourceCommunicator;
    return m_networkForConnectedConsumers;
}

bool VideoSourceRemote::launchSourceProcess( const SInitSettings & _settings ){

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

    // process
    std::vector<std::string> args;
    args.push_back( "--url=" + _settings.source.sourceUrl );
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


    // forward -> archive directory template to receiver ( + root dir, root web addr )
    // backward -> time, session name, playlist name


    // NOTE: streaming will start while video-receiver launch    
    const ProcessLauncher::SLaunchTask * launchTask = PROCESS_LAUNCHER.addLaunchTask( PROGRAM_NAME, args, CONFIG_PARAMS.baseParams.SYSTEM_CATCH_CHILD_PROCESSES_OUTPUT );
    while( ! launchTask->launched.load() ){
        // wait for process launch
    }
    m_processHandle = launchTask->handle;

    // TODO: existing handle doesn't mean that process alive
    if( ! m_processHandle ){
        return false;
    }

    SWALProcessEvent eventForWAL;
    eventForWAL.pid = m_processHandle->getChildPid();
    eventForWAL.programName = PROGRAM_NAME;
    eventForWAL.programArgs = args;
    m_walForDump.openProcessEvent( eventForWAL );

    // pseudo-sync with receiver process
    constexpr int waitProcessLaunchMillisec = 100;
    std::this_thread::sleep_for( chrono::milliseconds(waitProcessLaunchMillisec) );

    return true;
}

bool VideoSourceRemote::establishCommunicationWithProcess( const std::string & _sourceUrl ){

    // main link
    if( m_remoteSourceCommunicator ){
        PNetworkProvider provider = std::dynamic_pointer_cast<INetworkProvider>( m_remoteSourceCommunicator );
        provider->removeObserver( this );
    }

    m_remoteSourceCommunicator = m_internalCommunicationService->getSourceCommunicator( _sourceUrl );

    // connect to video-receiver
    int triesCount = 5;
    constexpr int connectTryIntervalMillisec = 1000;
    while( ! m_remoteSourceCommunicator && --triesCount > 0 ){

        VS_LOG_WARN << PRINT_HEADER
                    << " (!) try once more to connect with video-receiver after ["
                    << connectTryIntervalMillisec << "] millisec "
                    << endl;
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

    m_networkForConnectedConsumers = splittedNetwork;

    return true;
}

bool VideoSourceRemote::waitForStreamStartEvent(){

    // check for stream start event
    if( CONFIG_PARAMS.RECORD_ENABLE && ! m_streamStartEventArrived ){
        std::mutex lockMutex;
        std::unique_lock<std::mutex> cvLock( lockMutex );
        constexpr int timeoutSec = 5;
        m_cvArchiveSessionParametersWait.wait_for( cvLock,
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

bool VideoSourceRemote::makeConnectAttempts(){

    if( ! m_correlatedProcessCrashed ){
        VS_LOG_WARN << PRINT_HEADER
                    << " connect attempt to [" << m_settings.source.sourceUrl << "] aborted, because process did NOT crashed"
                    << endl;
        return true;
    }

    constexpr int connectAttemptIntervalSec = 3;
    bool timeouted = false;

    // TODO: do
//    m_settings.connectRetriesTimeoutSec

    VS_LOG_INFO << PRINT_HEADER
                << " make connect attempts to [" << m_settings.source.sourceUrl << "]"
                << endl;
    while( m_correlatedProcessCrashed && ! m_disconnectCalled && ! timeouted ){

        attemptToConnect();
        this_thread::sleep_for( chrono::seconds(connectAttemptIntervalSec) );
    }
    VS_LOG_INFO << PRINT_HEADER
                << " exit from connect attempts to [" << m_settings.source.sourceUrl << "]"
                << endl;

    //
    if( ! m_correlatedProcessCrashed ){
        return true;
    }
    else{
        return false;
    }
}

void VideoSourceRemote::attemptToConnect(){

    if( ! m_correlatedProcessCrashed ){
        VS_LOG_WARN << PRINT_HEADER
                    << " connect attempt to [" << m_settings.source.sourceUrl << "] aborted, because process did NOT crashed"
                    << endl;
        return;
    }
    VS_LOG_TRACE << PRINT_HEADER
                << " >> connect attempt to [" << m_settings.source.sourceUrl << "]"
                << endl;

    //
    {
        std::lock_guard<std::mutex> lock( m_muConnectAttempts );
        m_correlatedProcessCrashed = false;

        if( ! launchSourceProcess(m_settings) || m_correlatedProcessCrashed ){
            return;
        }

        if( ! establishCommunicationWithProcess(m_settings.source.sourceUrl) || m_correlatedProcessCrashed ){
            return;
        }

        if( ! waitForStreamStartEvent() || m_correlatedProcessCrashed ){
            return;
        }
    }

    //
    for( ISourceObserver * observer : m_observers ){
        observer->callbackVideoSourceConnection( true, m_settings.source.sourceUrl );
    }
}

bool VideoSourceRemote::connect( const SInitSettings & _settings ){

    // TODO: check url for popular prefixes: http, rtsp, etc...

    if( m_processHandle ){
        m_referenceCounter++;
        VS_LOG_WARN << PRINT_HEADER
                    << " [" << _settings.source.sourceUrl
                    << "] already connected. RefCount: " << m_referenceCounter
                    << endl;
        return true;
    }

    VS_LOG_INFO << PRINT_HEADER
                << " [" << _settings.source.sourceUrl
                << "] try to connect. RefCount: " << m_referenceCounter
                << endl;

    m_disconnectCalled = false;

    m_settings = _settings;
    PROCESS_LAUNCHER.addObserver( this, 0 );

    //
    if( ! launchSourceProcess(_settings) ){
        return false;
    }

    //
    if( ! establishCommunicationWithProcess(_settings.source.sourceUrl) ){
        if( ! makeConnectAttempts() ){
            return false;
        }
        else{
            m_referenceCounter++;
            m_connectCallFinished = true;
            VS_LOG_INFO << PRINT_HEADER
                        << " connected to [" << _settings.source.sourceUrl
                        << "]. RefCount: " << m_referenceCounter
                        << endl;
            return true;
        }
    }

    //
    if( ! waitForStreamStartEvent() ){
        if( ! makeConnectAttempts() ){
            return false;
        }
        else{
            m_referenceCounter++;
            m_connectCallFinished = true;
            VS_LOG_INFO << PRINT_HEADER
                        << " connected to [" << _settings.source.sourceUrl
                        << "]. RefCount: " << m_referenceCounter
                        << endl;
            return true;
        }
    }

    //
    m_referenceCounter++;
    m_connectCallFinished = true;
    VS_LOG_INFO << PRINT_HEADER
                << " connected to [" << _settings.source.sourceUrl
                << "]. RefCount: " << m_referenceCounter
                << endl;
    return true;
}

void VideoSourceRemote::disconnect(){

    VS_LOG_INFO << PRINT_HEADER
                << " [" << m_settings.source.sourceUrl
                << "] begin try to disconnect... RefCount: " << m_referenceCounter
                << endl;

    std::lock_guard<std::mutex> lock( m_muConnectAttempts );

    m_disconnectCalled = true;

    if( ! m_processHandle || 0 == m_referenceCounter ){
        VS_LOG_INFO << PRINT_HEADER
                    << "  [" << m_settings.source.sourceUrl << "] disconnect not needed."
                    << " ProcessHandle [" << m_processHandle << "]"
                    << " RefCount [" << m_referenceCounter << "]"
                    << endl;
        return;
    }

    m_referenceCounter--;
    if( m_referenceCounter > 0 ){
        VS_LOG_WARN << PRINT_HEADER
                    << " [" << m_settings.source.sourceUrl
                    << "] still in use. Disconnect aborted. RefCount: " << m_referenceCounter
                    << endl;
        return;
    }

    VS_LOG_INFO << PRINT_HEADER
                << " [" << m_settings.source.sourceUrl
                << "] will be disconnect. RefCount: " << m_referenceCounter
                << endl;

    // soft stop of archiving
    if( CONFIG_PARAMS.RECORD_ENABLE ){
        const string msgToSend = VideoReceiverProtocol::singleton().createSimpleCommand( CommandType::CT_STOP );

        PEnvironmentRequest request = m_remoteSourceCommunicator->getRequestInstance();
        request->setOutcomingMessage( msgToSend );

        // дать время ресиверу корректно остановиться
        this_thread::sleep_for( chrono::milliseconds(200) );
    }

    //
    PNetworkProvider provider = std::dynamic_pointer_cast<INetworkProvider>( m_remoteSourceCommunicator );
    provider->removeObserver( this );

    // terminate video-receiver process
    PROCESS_LAUNCHER.close( m_processHandle );
    PROCESS_LAUNCHER.removeObserver( this );
    m_walForDump.closeProcessEvent( m_processHandle->getChildPid() );

    //
    PATH_LOCATOR.removeVideoReceiverSocketsEnvironment( m_settings.source.sourceUrl );

    m_connectCallFinished = false;
}

void VideoSourceRemote::getArchiveParameters(){

    // NOTE: такой подход корректен только в случае когда время жизни ресивера совпадает со временем жизни VideoSourceRemote
    string response;
    if( m_callbackMsgArchiveData.empty() ){
        // request
        const string msgToSend = VideoReceiverProtocol::singleton().createSimpleCommand( CommandType::CT_GET_ARCHIVE_INFO );
        PEnvironmentRequest request = m_remoteSourceCommunicator->getRequestInstance();
        request->setOutcomingMessage( msgToSend );

        // wait from callback method
        std::mutex lockMutex;
        std::unique_lock<std::mutex> cvLock( lockMutex );
        m_cvResponseWait.wait( cvLock, [this](){ return ! m_callbackMsgArchiveData.empty(); } );
        response = m_callbackMsgArchiveData;
    }
    else{
        response = m_callbackMsgArchiveData;
    }

    // parse
    const ArchiveSession archSess = VideoReceiverProtocol::singleton().getArchiveData( response );

    if( archSess.timeBeginMillisec != 0 ){
        CrutchForCommonResources::singleton().getForModify( m_settings.source.sourceUrl ).tempCrutchStartRecordTimeMillisec =
                archSess.timeBeginMillisec + TIME_ZONE_MILLISEC; // TODO: ugly hack
        CrutchForCommonResources::singleton().getForModify( m_settings.source.sourceUrl ).tempCrutchSessionName =
                archSess.sessionName;
        CrutchForCommonResources::singleton().getForModify( m_settings.source.sourceUrl ).tempCrutchPlaylistName =
                archSess.playlistName;
    }
}

ISourcesProvider::SSourceParameters VideoSourceRemote::getStreamParameters(){

    std::lock_guard<std::mutex> lock( m_muPublicThreadSafety );

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
    m_cvResponseWait.wait( cvLock, [this](){ return ! m_callbackMsgStreamData.empty(); } );
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
    out.frameWidth = m_sourceParametersFromStreamEvent.frameWidth;
    out.frameHeight = m_sourceParametersFromStreamEvent.frameHeight;

    if( out.rtpStreamEnconding.empty() || out.rtpStreamCaps.empty() ){
        VS_LOG_WARN << PRINT_HEADER
                    << " from video-receiver ENCODING or RTP-CAPS is empty."
                    << " Response [" << response << "]"
                    << endl;
    }

    return out;
}

int32_t VideoSourceRemote::getReferenceCount(){
    return m_referenceCounter;
}

void VideoSourceRemote::callbackProcessCrashed( ProcessHandle * _handle ){

    const string sourceUrl = _handle->findArgValue( "url" );

    if( ! sourceUrl.empty() && sourceUrl == m_settings.source.sourceUrl ){
        VS_LOG_INFO << PRINT_HEADER
                    << " catched signal about process crash with source url: [" << sourceUrl << "]"
                    << endl;

        m_correlatedProcessCrashed = true;

        // clear previous resources
        if( m_remoteSourceCommunicator ){
            PNetworkProvider provider = std::dynamic_pointer_cast<INetworkProvider>( m_remoteSourceCommunicator );
            provider->removeObserver( this );

            m_remoteSourceCommunicator.reset();
        }

        m_sourceParametersFromStreamEvent.frameWidth = 0;
        m_sourceParametersFromStreamEvent.frameHeight = 0;

        for( ISourceObserver * observer : m_observers ){
            observer->callbackVideoSourceConnection( false, m_settings.source.sourceUrl );
        }

        // NOTE: only if process crashed AFTER correct start ( while start checks inside connect() in cyclic mode )
        if( ! m_disconnectCalled && m_connectCallFinished ){
            attemptToConnect();
        }
    }
    else{
        VS_LOG_WARN << PRINT_HEADER
                    << " catched signal about process crash, but either source URL not found or"
                    << " URL mismatch: [" << sourceUrl << "] and [" << m_settings.source.sourceUrl << "]"
                    << endl;
    }
}

void VideoSourceRemote::callbackNetworkRequest( PEnvironmentRequest _request ){

    if( _request->getIncomingMessage().empty() ){
        return;
    }

    if( _request->getIncomingMessage().find("\"answer\":\"duration\"") != std::string::npos ){
        return;
    }

    const string json = _request->getIncomingMessage();
    const AnswerType answer = VideoReceiverProtocol::singleton().getAnswerType( json );    

    switch( answer ){
    case AnswerType::AT_STREAMDATA: {
        VS_LOG_INFO << PRINT_HEADER
                    << " AT_STREAMDATA event from receiver"
                    // << " [" << json << "]"
                    << endl;

        m_callbackMsgStreamData = json;
        m_cvResponseWait.notify_one();
        break;
    }
    case AnswerType::AT_ARCHIVEDATA: {
        VS_LOG_INFO << PRINT_HEADER
                    << " AT_ARCHIVEDATA event from receiver"
                    // << " [" << json << "]"
                    << endl;

        m_callbackMsgArchiveData = json;
        m_cvResponseWait.notify_one();
        break;
    }
    case AnswerType::AT_PLUGINSDATA: {
        break;
    }
    case AnswerType::AT_DURATION : {
        FrameOptions fo = VideoReceiverProtocol::singleton().getFrameOptions( json );
        break;
    }
    case AnswerType::AT_RESPONSE : {
        break;
    }
    case AnswerType::AT_RECORD : {
        RecordData rd = VideoReceiverProtocol::singleton().getRecordData( json );
        break;
    }
    case AnswerType::AT_EVENT : {
        const EventData ed = VideoReceiverProtocol::singleton().getEventData( json );

        if( EventType::ET_STREAM_START == ed.type ){
            VS_LOG_INFO << PRINT_HEADER
                        << " ET_STREAM_START event from receiver"
                        // << " [" << json << "]"
                        << endl;

            m_sourceParametersFromStreamEvent.frameWidth = ed.options.width;
            m_sourceParametersFromStreamEvent.frameHeight = ed.options.height;

            if( ed.archiveSession.timeBeginMillisec != 0 ){
                CrutchForCommonResources::singleton().getForModify( m_settings.source.sourceUrl ).tempCrutchStartRecordTimeMillisec =
                        ed.archiveSession.timeBeginMillisec + TIME_ZONE_MILLISEC; // TODO: ugly hack
                CrutchForCommonResources::singleton().getForModify( m_settings.source.sourceUrl ).tempCrutchSessionName =
                        ed.archiveSession.sessionName;
                CrutchForCommonResources::singleton().getForModify( m_settings.source.sourceUrl ).tempCrutchPlaylistName =
                        ed.archiveSession.playlistName;
            }
            m_streamStartEventArrived = true;
            m_cvArchiveSessionParametersWait.notify_one();

            // так как ET_STREAM_START приходит и в процессе штатного запуска
            if( m_signalLosted ){
                m_signalLosted = false;

                for( ISourceObserver * observer : m_observers ){
                    observer->callbackVideoSourceConnection( true, m_settings.source.sourceUrl );
                }
            }
        }
        else if( EventType::ET_STREAM_STOP == ed.type ){
            VS_LOG_INFO << PRINT_HEADER
                        << " ET_STREAM_STOP event from receiver"
                        // << " [" << json << "]"
                        << endl;

            m_sourceParametersFromStreamEvent.frameWidth = 0;
            m_sourceParametersFromStreamEvent.frameHeight = 0;

            m_signalLosted = true;
            for( ISourceObserver * observer : m_observers ){
                observer->callbackVideoSourceConnection( false, m_settings.source.sourceUrl );
            }

            // TODO: timeout for source waiting ?
        }

        break;
    }
    case AnswerType::AT_INFO : {
        InfoData id = VideoReceiverProtocol::singleton().getInfoData( json );
        VS_LOG_INFO << PRINT_HEADER
                    << " info data from receiver with [" << m_settings.source.sourceUrl << "]"
                    << " : [" << id.text << "]"
                    << endl;

        break;
    }
    case AnswerType::AT_ANALYTIC : {
        AnalyticData ad = VideoReceiverProtocol::singleton().getAnalyticData( json );
        break;
    }
    case AnswerType::AT_Undef : {
        VS_LOG_WARN << PRINT_HEADER << " unknown answer [" << json << "]" << endl;
        break;
    }
    default : {
        break;
    }
    }
}

