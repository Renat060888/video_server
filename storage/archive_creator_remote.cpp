
#include <cassert>

#include <jsoncpp/json/reader.h>
#include <boost/format.hpp>
#include <microservice_common/system/logger.h>

#include "system/path_locator.h"
#include "system/config_reader.h"
#include "communication/protocols/video_receiver_protocol.h"
#include "common/common_utils.h"
#include "archive_creator_remote.h"

using namespace std;
using namespace common_types;

static constexpr const char * PLAYLIST_CLOSING_TAG = "#EXT-X-ENDLIST";
static constexpr const char * PRINT_HEADER = "Archiver:";

ArchiveCreatorRemote::ArchiveCreatorRemote( IInternalCommunication * _internalCommunicationService, const std::string & _clientName )
    : IProcessObserver(1)
    , m_connectionLosted(false)
    , m_closed(false)
    , m_requestInited(false)
{
    // NOTE: process already created by VideoSource
    // ( retranslator & archiver & analyzer & ... uses the same video-receiver )
}

ArchiveCreatorRemote::~ArchiveCreatorRemote(){

    VS_LOG_TRACE << PRINT_HEADER << " [" << m_settings.sourceUrl << "] DTOR" << endl;

    stop();
}

bool ArchiveCreatorRemote::start( const SInitSettings & _settings ){

    // TODO: may be replace ObjreprRepository update to VideoRecorder ?
    m_settings = _settings;
    m_currentState.initSettings = & m_settings;

    // send command to receiver for recording parameters
    PNetworkProvider provider = std::dynamic_pointer_cast<INetworkProvider>( _settings.communicationWithVideoSource );
    provider->addObserver( this );

    if( ! getArchiveSessionInfo(m_currentState.timeStartMillisec, m_currentState.sessionName, m_currentState.playlistName) ){
        return false;
    }
    m_currentState.timeStartMillisec += TIME_ZONE_MILLISEC;

    // objrepr object
    SHlsMetadata meta;
    meta.sourceUrl = _settings.sourceUrl;
    meta.sensorId = _settings.sensorId;
    meta.timeStartMillisec = m_currentState.timeStartMillisec;
    meta.timeEndMillisec = 0;
    meta.playlistWebLocationFullPath = CONFIG_PARAMS.HLS_WEBSERVER_URL_ROOT + "/" + m_currentState.sessionName + "/" + m_playlistName;

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

void ArchiveCreatorRemote::stop(){

    if( m_closed ){
        return;
    }

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

    // soft stop of archiving
    // TODO: video source can be used by other consumers
//    if( CONFIG_PARAMS.RECORD_ENABLE ){
//        const string msgToSend = VideoReceiverProtocol::singleton().createSimpleCommand( CommandType::CT_STOP );

//        PEnvironmentRequest request = m_settings.communicationWithVideoSource->getRequestInstance();
//        request->setOutcomingMessage( msgToSend, true );
//    }

    // network
    PNetworkProvider provider = std::dynamic_pointer_cast<INetworkProvider>( m_settings.communicationWithVideoSource );
    provider->removeObserver( this );

    // final update of live stream playlist
    AArchiveCreator::transferOriginalPlaylistToLiveStream( m_originalPlaylistFullPath, m_liveStreamPlaylistDirFullPath );

    //
    AArchiveCreator::checkOriginalPlaylistConsistent( m_originalPlaylistFullPath );

    m_closed = true;
    m_currentState.state = EArchivingState::READY;

    // NOTE: recording will stop with video-receiver terminate
    VS_LOG_INFO << PRINT_HEADER
                << " archive close [" << m_settings.sourceUrl << "]"
                << endl;
}

bool ArchiveCreatorRemote::getArchiveSessionInfo( int64_t & _startRecordTimeMillisec,
                                                  std::string & _sessionName,
                                                  std::string & _playlistName ){

    m_requestInited.store( true );

    const string msgToSend = VideoReceiverProtocol::singleton().createSimpleCommand( CommandType::CT_GET_ARCHIVE_INFO );
    PEnvironmentRequest request = m_settings.communicationWithVideoSource->getRequestInstance();
    request->setOutcomingMessage( msgToSend );

    // TODO: wait with timeout ( for the safety )

    // wait from callback method
    std::mutex lockMutex;
    std::unique_lock<std::mutex> cvLock( lockMutex );
    m_cvResponseWait.wait( cvLock, [this](){ return ! m_callbackMsgArchiveData.empty(); } );
    const string response = m_callbackMsgArchiveData;
    m_callbackMsgArchiveData.clear();

    m_requestInited.store( false );

    // parse
    const ArchiveSession archSess = VideoReceiverProtocol::singleton().getArchiveData( response );
    VS_LOG_INFO << "CT_GET_ARCHIVE_INFO from receiver [" << response << "]"
             << endl;

    _startRecordTimeMillisec = archSess.timeBeginMillisec;
    _sessionName = archSess.sessionName;
    _playlistName = archSess.playlistName;        

    return true;
}

void ArchiveCreatorRemote::tick(){

    AArchiveCreator::transferOriginalPlaylistToLiveStream( m_originalPlaylistFullPath, m_liveStreamPlaylistDirFullPath );
}

void ArchiveCreatorRemote::callbackProcessCrashed( ProcessHandle * _handle ){

    const string sourceUrl = _handle->findArgValue( "url" );

    if( ! sourceUrl.empty() && sourceUrl == m_settings.sourceUrl ){
        VS_LOG_INFO << "archiver catched signal about process crash with source url: [" << sourceUrl
                 << "] Update archive related to the same URL"
                 << endl;
        stop();
        m_connectionLosted = true;
        m_currentState.state = EArchivingState::CRUSHED;
    }
    else{
        VS_LOG_WARN << "remote archiver catched signal about process crash, but either source URL not found or"
                 << " URL mismatch: [" << sourceUrl << "] and [" << m_settings.sourceUrl << "]"
                 << endl;
    }
}

void ArchiveCreatorRemote::callbackNetworkRequest( PEnvironmentRequest _request ){

    if( ! m_requestInited.load() ){
        return;
    }

    if( _request->getIncomingMessage().empty() ){
        return;
    }

    if( _request->getIncomingMessage().find("duration") != std::string::npos ){
        return;
    }

    const string json = _request->getIncomingMessage();

    const AnswerType answer = VideoReceiverProtocol::singleton().getAnswerType( json );
    switch( answer ){
    case AnswerType::AT_DURATION : {
        FrameOptions frame_opt = VideoReceiverProtocol::singleton().getFrameOptions( json );
        break;
    }
    case AnswerType::AT_RESPONSE : {
        break;
    }
    case AnswerType::AT_ARCHIVEDATA: {
        m_callbackMsgArchiveData = json;
        m_cvResponseWait.notify_one();
        break;
    }
    case AnswerType::AT_RECORD : {
        RecordData rd = VideoReceiverProtocol::singleton().getRecordData( json );
        break;
    }
    case AnswerType::AT_EVENT : {
        EventData ed = VideoReceiverProtocol::singleton().getEventData( json );

        // TODO: ?
//        if( EventType::ET_STREAM_STOP == ed.type ){
//            LOG_INFO << "archive creator on [" << m_settings.sourceUrl << "] catched connection-lost signal. Close this one" << endl;
//            stop();
//            m_connectionLosted = true;
//            m_currentState.state = EArchivingState::CRUSHED;
//        }

        break;
    }
    case AnswerType::AT_INFO : {
        InfoData id = VideoReceiverProtocol::singleton().getInfoData( json );
        break;
    }
    case AnswerType::AT_ANALYTIC : {
        AnalyticData ad = VideoReceiverProtocol::singleton().getAnalyticData( json );
        break;
    }
    default : {
        break;
    }
    }
}

bool ArchiveCreatorRemote::isConnectionLosted() const {

    return m_connectionLosted;
}

const ArchiveCreatorRemote::SInitSettings & ArchiveCreatorRemote::getSettings() const {

    return m_settings;
}

const ArchiveCreatorRemote::SCurrentState & ArchiveCreatorRemote::getCurrentState() {

    return m_currentState;
}

const std::string & ArchiveCreatorRemote::getLastError() {
    // dummy ( video-receiver )
    return m_lastError;
}
