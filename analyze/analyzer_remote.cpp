
#include <cassert>
#include <thread>

#include <microservice_common/system/logger.h>

#include "system/config_reader.h"
#include "system/path_locator.h"
#include "common/common_utils.h"
#include "analyzer_remote.h"

using namespace std;
using namespace common_types;

AnalyzerRemote::AnalyzerRemote( IInternalCommunication * _internalCommunicationService, const std::string & _clientName )
    : IProcessObserver(1)
    , m_internalCommunicationService(_internalCommunicationService)
{    

}

AnalyzerRemote::~AnalyzerRemote(){

    // NOTE: DTOR also must be called for SharedPointer decrement ( or SP.reset() )

    stop();
}

void AnalyzerRemote::setLivePlaying( bool _live ){

    // TODO: do
}

void AnalyzerRemote::createConstRequests(){

    // "get accumulated events"
    {
        video_server_protocol::MessageModuleUniqueId uniqueId;
        uniqueId.set_pid( ::getpid() );
        uniqueId.set_ip_octets( common_utils::ipAddressToOctets(common_utils::getIpAddressStr()) );

        video_server_protocol::MessageHeader header;
        header.set_module_name( m_moduleName );
        header.mutable_id()->CopyFrom( uniqueId );

        video_server_protocol::MessageAnalyzerProxy message;
        message.mutable_header()->CopyFrom( header );
        message.set_msg_type( video_server_protocol::ESyncCommandType::SCT_GET_ACCUMULATED_EVENTS );

        m_protobufAnalyzerOut.set_sender( video_server_protocol::EWho::W_ANALYZER_PROXY );
        m_protobufAnalyzerOut.mutable_msg_proxy()->CopyFrom( message );
        m_constRequestGetAccumEvents = m_protobufAnalyzerOut.SerializeAsString();
    }

    //
}

void AnalyzerRemote::callbackNetworkRequest( PEnvironmentRequest _request ){

    const string & message = _request->getIncomingMessage();

    // TODO: solve this problem
    if( message.empty() ){
        return;
    }

    if( ! m_protobufAnalyzerIn.ParseFromString( message ) ){
        VS_LOG_ERROR << " AnalyzerRemote callback internal protocol parse fail [" << message << "]" << endl;
        return;
    }

    VS_LOG_TRACE << "=========================== AnalyzerRemote incoming message [" << m_protobufAnalyzerIn.DebugString() << "]" << endl;

    // check for correctness
    assert( video_server_protocol::EWho::W_ANALYZER_REAL == m_protobufAnalyzerIn.sender() &&
            "message not from Real Analyzer" );

    const video_server_protocol::MessageAnalyzerReal & parsedMsg = m_protobufAnalyzerIn.msg_real();

    // TODO: action

}

void AnalyzerRemote::callbackProcessCrashed( ProcessHandle * _handle ){


    // TODO: recreate process again
}

bool AnalyzerRemote::start( SInitSettings _settings ){

    std::lock_guard<std::mutex> lock( m_networkLock );

    assert( ! _settings.dpfProcessorPtr && "in REMOTE analyze mode 'dpfProcessorPtr' is useless, because executed in separate process" );

    m_settings = _settings;
    PROCESS_LAUNCHER.addObserver( this, 0 );
    m_moduleName = string("analyzer_proxy_for_sensor_id") + std::to_string( _settings.sensorId );

    // TODO: where to replace ?
    createConstRequests();

    // process
    std::vector<std::string> args;
    args.push_back( "--source-url=" + _settings.sourceUrl );
    args.push_back( "--control-socket=" + PATH_LOCATOR.getAnalyzerControlSocket( _settings.sourceUrl ) );

    const std::string program = "video_analyzer";
    m_processHandle = PROCESS_LAUNCHER.launch(program, args, CONFIG_PARAMS.SYSTEM_CATCH_CHILD_PROCESSES_OUTPUT, false);
    if( ! m_processHandle ){
        return false;
    }

    SWALProcessEvent eventForWAL;
    eventForWAL.pid = m_processHandle->getChildPid();
    eventForWAL.programName = program;
    eventForWAL.programArgs = args;
//    m_WAL.writeProcessLaunchEvent( eventForWAL );

    // TODO: smells like a crutch
    constexpr int waitProcessForkExec = 1;
    std::this_thread::sleep_for( chrono::seconds(waitProcessForkExec) );

    VS_LOG_INFO << "AnalyzerRemote get lock, ms: " << common_utils::getCurrentTimeMillisec() << endl;
    PROCESS_LAUNCHER.getLock( "/analyzer_launch_lock" );
    VS_LOG_INFO << "AnalyzerRemote release lock, ms: " << common_utils::getCurrentTimeMillisec() << endl;
    PROCESS_LAUNCHER.releaseLock( "/analyzer_launch_lock" );

    // network
    m_remoteAnalyzerCommunicator = m_internalCommunicationService->getAnalyticCommunicator( _settings.sourceUrl );

    PNetworkProvider provider = std::dynamic_pointer_cast<INetworkProvider>( m_remoteAnalyzerCommunicator );
    assert( provider );
    provider->addObserver( this );

    // send
    const string serializedStr = createStartMessage( _settings );
    PEnvironmentRequest request = m_remoteAnalyzerCommunicator->getRequestInstance();
    request->sendOutcomingMessage( serializedStr, true );

    // process reponse
    const string & response = request->getIncomingMessage();
    if( ! m_protobufAnalyzerIn.ParseFromString( response ) ){
        VS_LOG_ERROR << "internal protocol parse fail [" << response << "]" << endl;
        return false;
    }

    // check for correctness
    assert( video_server_protocol::EWho::W_ANALYZER_REAL == m_protobufAnalyzerIn.sender() &&
            "message not from Real Analyzer" );

    const video_server_protocol::MessageAnalyzerReal & parsedMsg = m_protobufAnalyzerIn.msg_real();

    assert( video_server_protocol::ESyncCommandType::SCT_UNDEFINED != parsedMsg.msg_type() &&
            "sync-message from Real Analyzer must be processed in appropriate request method" );

    // parse
    bool success = false;
    if( video_server_protocol::ESyncCommandType::SCT_START == parsedMsg.msg_type() ){

        const ::video_server_protocol::MessageResponseStart & startResponse = parsedMsg.msg_start();
        success = startResponse.success();

        if( ! success ){
            m_lastError = parsedMsg.header().last_error();
        }
    }

    return success;
}

void AnalyzerRemote::pause(){

    std::lock_guard<std::mutex> lock( m_networkLock );

    // send
    const string serializedStr = createPauseMessage();

    PEnvironmentRequest request = m_remoteAnalyzerCommunicator->getRequestInstance();
    request->sendOutcomingMessage( serializedStr, true );

    // process reponse
    const string & response = request->getIncomingMessage();
    if( ! m_protobufAnalyzerIn.ParseFromString( response ) ){
        VS_LOG_ERROR << " AnalyzerRemote stop internal protocol parse fail [" << response << "]" << endl;
        return;
    }

    // check for correctness
    assert( video_server_protocol::EWho::W_ANALYZER_REAL == m_protobufAnalyzerIn.sender() &&
            "message not from Real Analyzer" );

    const video_server_protocol::MessageAnalyzerReal & parsedMsg = m_protobufAnalyzerIn.msg_real();

    assert( video_server_protocol::ESyncCommandType::SCT_UNDEFINED != parsedMsg.msg_type() &&
            "sync-message from Real Analyzer must be processed in appropriate request method" );

    // parse
    if( video_server_protocol::ESyncCommandType::SCT_PAUSE == parsedMsg.msg_type() ){


    }
}

void AnalyzerRemote::resume(){

    std::lock_guard<std::mutex> lock( m_networkLock );

    // send
    const string serializedStr = createResumeMessage();

    PEnvironmentRequest request = m_remoteAnalyzerCommunicator->getRequestInstance();
    request->sendOutcomingMessage( serializedStr, true );

    // process reponse
    const string & response = request->getIncomingMessage();
    if( ! m_protobufAnalyzerIn.ParseFromString( response ) ){
        VS_LOG_ERROR << " AnalyzerRemote stop internal protocol parse fail [" << response << "]" << endl;
        return;
    }

    // check for correctness
    assert( video_server_protocol::EWho::W_ANALYZER_REAL == m_protobufAnalyzerIn.sender() &&
            "message not from Real Analyzer" );

    const video_server_protocol::MessageAnalyzerReal & parsedMsg = m_protobufAnalyzerIn.msg_real();

    assert( video_server_protocol::ESyncCommandType::SCT_UNDEFINED != parsedMsg.msg_type() &&
            "sync-message from Real Analyzer must be processed in appropriate request method" );

    // parse
    if( video_server_protocol::ESyncCommandType::SCT_RESUME == parsedMsg.msg_type() ){


    }
}

void AnalyzerRemote::stop(){

    if( ! m_processHandle ){
        return;
    }

    std::lock_guard<std::mutex> lock( m_networkLock );

    // send
    const string serializedStr = createStopMessage();
    PEnvironmentRequest request = m_remoteAnalyzerCommunicator->getRequestInstance();
    request->sendOutcomingMessage( serializedStr, true );

    // process reponse
    const string & response = request->getIncomingMessage();
    if( ! m_protobufAnalyzerIn.ParseFromString( response ) ){
        VS_LOG_ERROR << " AnalyzerRemote stop internal protocol parse fail [" << response << "]" << endl;
        return;
    }

    // check for correctness
    assert( video_server_protocol::EWho::W_ANALYZER_REAL == m_protobufAnalyzerIn.sender() &&
            "message not from Real Analyzer" );

    const video_server_protocol::MessageAnalyzerReal & parsedMsg = m_protobufAnalyzerIn.msg_real();

    assert( video_server_protocol::ESyncCommandType::SCT_UNDEFINED != parsedMsg.msg_type() &&
            "sync-message from Real Analyzer must be processed in appropriate request method" );

    // parse
    if( video_server_protocol::ESyncCommandType::SCT_STOP == parsedMsg.msg_type() ){


    }

    // terminate analyzer process    
    PROCESS_LAUNCHER.kill( m_processHandle );
    PROCESS_LAUNCHER.removeObserver( this );
//    m_WAL.writeProcessCloseEvent( m_processHandle->getChildPid() );

    //
    PATH_LOCATOR.removeAnalyzerControlSocket( m_settings.sourceUrl );
}

std::string AnalyzerRemote::createStartMessage( const SInitSettings & _settings ){

    m_protobufAnalyzerOut.Clear();

    // create
    video_server_protocol::MessageModuleUniqueId uniqueId;
    uniqueId.set_pid( ::getpid() );
    uniqueId.set_ip_octets( common_utils::ipAddressToOctets(common_utils::getIpAddressStr()) );

    video_server_protocol::MessageHeader header;
    header.set_module_name( m_moduleName );
    header.set_last_error( "" );
    header.mutable_id()->CopyFrom( uniqueId );

    video_server_protocol::ContainerInitSettings initSettings;
    initSettings.set_sensor_id( _settings.sensorId );
    initSettings.set_profile_id( _settings.profileId );
    initSettings.set_source_url( _settings.sourceUrl );
    initSettings.set_processing_id( _settings.processingId );
    initSettings.set_processing_name( _settings.processingName );
    initSettings.set_intervideosink_channel_name( _settings.intervideosinkChannelName );
    initSettings.set_shm_rtp_stream_caps( _settings.shmRtpStreamCaps );
    initSettings.set_shm_rtp_stream_encoding( _settings.shmRtpStreamEncoding );
    initSettings.set_videoreceiverrtppayloadsocket( _settings.VideoReceiverRtpPayloadSocket );
    initSettings.set_processing_flags( _settings.processingFlags );
    for( auto & valuePair : _settings.allowedDpfLabelObjreprClassinfo ){
        const string & dpf = valuePair.first;
        const uint64_t objrepr = valuePair.second;

        video_server_protocol::ContainerDpfObjreprMap protobufDpfObjreprMap;
        protobufDpfObjreprMap.set_dpf_label( dpf );
        protobufDpfObjreprMap.set_objrepr_classinfo_id( objrepr );

        initSettings.add_dpf_objrepr_map()->CopyFrom( protobufDpfObjreprMap );
    }

    for( const SPluginMetadata & plugin : _settings.plugins ){
        video_server_protocol::FilterPluginMetadata pluginToSend;
        pluginToSend.set_plugin_static_name( plugin.pluginStaticName );
        pluginToSend.set_gstreamer_element_name( plugin.GStreamerElementName );

        for( const SPluginParameter & param : plugin.params ){
            video_server_protocol::FilterPluginParam paramToSend;
            paramToSend.set_key( param.key );
            paramToSend.set_val( param.value );

            pluginToSend.add_params()->CopyFrom( paramToSend );
        }

        initSettings.add_plugins()->CopyFrom( pluginToSend );
    }

    video_server_protocol::MessageRequestStart requestStart;
    requestStart.mutable_settings()->CopyFrom( initSettings );

    video_server_protocol::MessageAnalyzerProxy message;
    message.mutable_header()->CopyFrom( header );
    message.set_msg_type( video_server_protocol::ESyncCommandType::SCT_START );
    message.mutable_msg_start()->CopyFrom( requestStart );

    m_protobufAnalyzerOut.set_sender( video_server_protocol::EWho::W_ANALYZER_PROXY );
    m_protobufAnalyzerOut.mutable_msg_proxy()->CopyFrom( message );

    return m_protobufAnalyzerOut.SerializeAsString();
}

std::string AnalyzerRemote::createPauseMessage(){

    m_protobufAnalyzerOut.Clear();

    // create
    video_server_protocol::MessageModuleUniqueId uniqueId;
    uniqueId.set_pid( ::getpid() );
    uniqueId.set_ip_octets( common_utils::ipAddressToOctets(common_utils::getIpAddressStr()) );

    video_server_protocol::MessageHeader header;
    header.set_module_name( m_moduleName );
    header.set_last_error( "" );
    header.mutable_id()->CopyFrom( uniqueId );

    video_server_protocol::MessageAnalyzerProxy message;
    message.mutable_header()->CopyFrom( header );
    message.set_msg_type( video_server_protocol::ESyncCommandType::SCT_PAUSE );

    m_protobufAnalyzerOut.set_sender( video_server_protocol::EWho::W_ANALYZER_PROXY );
    m_protobufAnalyzerOut.mutable_msg_proxy()->CopyFrom( message );

    return m_protobufAnalyzerOut.SerializeAsString();
}

std::string AnalyzerRemote::createResumeMessage(){

    m_protobufAnalyzerOut.Clear();

    // create
    video_server_protocol::MessageModuleUniqueId uniqueId;
    uniqueId.set_pid( ::getpid() );
    uniqueId.set_ip_octets( common_utils::ipAddressToOctets(common_utils::getIpAddressStr()) );

    video_server_protocol::MessageHeader header;
    header.set_module_name( m_moduleName );
    header.set_last_error( "" );
    header.mutable_id()->CopyFrom( uniqueId );

    video_server_protocol::MessageAnalyzerProxy message;
    message.mutable_header()->CopyFrom( header );
    message.set_msg_type( video_server_protocol::ESyncCommandType::SCT_RESUME );

    m_protobufAnalyzerOut.set_sender( video_server_protocol::EWho::W_ANALYZER_PROXY );
    m_protobufAnalyzerOut.mutable_msg_proxy()->CopyFrom( message );

    return m_protobufAnalyzerOut.SerializeAsString();
}

std::string AnalyzerRemote::createStopMessage(){

    m_protobufAnalyzerOut.Clear();

    // create
    video_server_protocol::MessageModuleUniqueId uniqueId;
    uniqueId.set_pid( ::getpid() );
    uniqueId.set_ip_octets( common_utils::ipAddressToOctets(common_utils::getIpAddressStr()) );

    video_server_protocol::MessageHeader header;
    header.set_module_name( m_moduleName );
    header.set_last_error( "" );
    header.mutable_id()->CopyFrom( uniqueId );

    video_server_protocol::MessageAnalyzerProxy message;
    message.mutable_header()->CopyFrom( header );
    message.set_msg_type( video_server_protocol::ESyncCommandType::SCT_STOP );

    m_protobufAnalyzerOut.set_sender( video_server_protocol::EWho::W_ANALYZER_PROXY );
    m_protobufAnalyzerOut.mutable_msg_proxy()->CopyFrom( message );

    return m_protobufAnalyzerOut.SerializeAsString();
}

static EAnalyzeState convertAnalyzeStateFromProtobuf( ::video_server_protocol::EAnalyzeState _state ){
    switch( _state ){
    case ::video_server_protocol::EAnalyzeState::AS_ACTIVE : return EAnalyzeState::ACTIVE;
    case ::video_server_protocol::EAnalyzeState::AS_PREPARING : return EAnalyzeState::PREPARING;
    case ::video_server_protocol::EAnalyzeState::AS_READY : return EAnalyzeState::READY;
    case ::video_server_protocol::EAnalyzeState::AS_CRUSHED : return EAnalyzeState::CRUSHED;
    case ::video_server_protocol::EAnalyzeState::AS_UNAVAILABLE : return EAnalyzeState::UNAVAILABLE;
    default : return EAnalyzeState::UNDEFINED;
    }
}

const AnalyzerRemote::SAnalyzeStatus & AnalyzerRemote::getStatus(){

    std::lock_guard<std::mutex> lock( m_networkLock );

    m_protobufAnalyzerOut.Clear();

    // create
    video_server_protocol::MessageModuleUniqueId uniqueId;
    uniqueId.set_pid( ::getpid() );
    uniqueId.set_ip_octets( common_utils::ipAddressToOctets(common_utils::getIpAddressStr()) );

    video_server_protocol::MessageHeader header;
    header.set_module_name( m_moduleName );
    header.mutable_id()->CopyFrom( uniqueId );

    video_server_protocol::MessageAnalyzerProxy message;
    message.mutable_header()->CopyFrom( header );
    message.set_msg_type( video_server_protocol::ESyncCommandType::SCT_GET_STATUS );

    m_protobufAnalyzerOut.set_sender( video_server_protocol::EWho::W_ANALYZER_PROXY );
    m_protobufAnalyzerOut.mutable_msg_proxy()->CopyFrom( message );


    // send
    const string serializedStr = m_protobufAnalyzerOut.SerializeAsString();

    PEnvironmentRequest request = m_remoteAnalyzerCommunicator->getRequestInstance();
    request->sendOutcomingMessage( serializedStr, true );


    // process reponse
    const string & response = request->getIncomingMessage();
    if( ! m_protobufAnalyzerIn.ParseFromString( response ) ){
        VS_LOG_ERROR << "AnalyzerRemote accum events internal protocol parse fail [" << response << "]" << endl;
        return m_status;
    }


    const ::video_server_protocol::MessageAnalyzerReal & msgReal = m_protobufAnalyzerIn.msg_real();
    assert( ::video_server_protocol::ESyncCommandType::SCT_GET_STATUS == msgReal.msg_type() );

    const ::video_server_protocol::MessageResponseGetStatus & statusMsg = msgReal.msg_get_status();
    const ::video_server_protocol::ContainerAnalyzeStatus & status = statusMsg.status();
    m_status.sensorId = status.sensor_id();
    m_status.analyzeState = convertAnalyzeStateFromProtobuf( status.state() );
    m_status.processingId = status.processing_id();
    m_status.processingName = status.processing_name();
    m_status.profileId = status.profile_id();

    return m_status;
}

std::vector<SAnalyticEvent> AnalyzerRemote::getAccumulatedEvents(){

    std::vector<SAnalyticEvent> out;

    if( m_remoteAnalyzerCommunicator ){
        std::lock_guard<std::mutex> lock( m_networkLock );

        // send
        PEnvironmentRequest request = m_remoteAnalyzerCommunicator->getRequestInstance();
        request->sendOutcomingMessage( m_constRequestGetAccumEvents, true );

        // process reponse
        const string & response = request->getIncomingMessage();
        if( ! m_protobufAnalyzerIn.ParseFromString( response ) ){
            VS_LOG_ERROR << "AnalyzerRemote accum events internal protocol parse fail [" << response << "]" << endl;
            return out;
        }


        const ::video_server_protocol::MessageAnalyzerReal & msgReal = m_protobufAnalyzerIn.msg_real();
        assert( ::video_server_protocol::ESyncCommandType::SCT_GET_ACCUMULATED_EVENTS == msgReal.msg_type() );

        const ::video_server_protocol::MessageResponseAnalyticEvent & accumEvents = msgReal.msg_get_accum_events();
        const google::protobuf::RepeatedPtrField< video_server_protocol::ContainerAnalyticEvent > & events = accumEvents.events();
        out.reserve( events.size() );
        for( int i = 0; i < events.size(); i++ ){
            const ::video_server_protocol::ContainerAnalyticEvent & event = events.Get( i );

            SAnalyticEvent eventToReceive;
            eventToReceive.pluginName = event.plugin_name();
            eventToReceive.type_plugin = event.plugin_type();
            eventToReceive.sensorId = event.sensor_id();
            eventToReceive.eventMessage = event.event_message();
            eventToReceive.processingId = event.processing_id();

            out.push_back( eventToReceive );
        }
    }

    return out;
}










