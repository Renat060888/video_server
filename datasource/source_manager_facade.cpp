
#include <microservice_common/system/logger.h>

#include "system/config_reader.h"
#include "system/objrepr_bus_vs.h"
#include "source_manager_facade.h"
#include "video_source.h"
#include "video_source_remote2.h"

using namespace std;
using namespace common_types;

static constexpr const char * PRINT_HEADER = "SourceMgr:";

SourceManagerFacade::SourceManagerFacade() :
    m_shutdownCalled(false),
    m_rtspSourcePortCounter( CONFIG_PARAMS.RTSP_SERVER_INITIAL_PORT )
{

}

SourceManagerFacade::~SourceManagerFacade()
{
    if( ! m_shutdownCalled ){
        shutdown();
    }
}

int32_t SourceManagerFacade::getRTSPSourcePort(){

    int32_t out = m_rtspSourcePortCounter;
    m_rtspSourcePortCounter++;
    return out;
}

ISourcesProvider * SourceManagerFacade::getServiceOfSourceProvider(){
    return this;
}

ISourceRetranslation * SourceManagerFacade::getServiceOfSourceRetranslation(){
    return this;
}

bool SourceManagerFacade::init( SInitSettings _settings ){

    m_settings = _settings;



    VS_LOG_INFO << PRINT_HEADER << " init success" << endl;
    return true;
}

void SourceManagerFacade::threadSourcesMaintenance(){


    // TODO: ?
}

void SourceManagerFacade::shutdown(){

    VS_LOG_INFO << PRINT_HEADER << " begin shutdown" << endl;

    m_shutdownCalled = true;
    m_retranslators.clear();

    VS_LOG_INFO << PRINT_HEADER << " shutdown success" << endl;
}

std::string SourceManagerFacade::getLastError(){
    return "SourceManager error: " + m_lastError;
}

void SourceManagerFacade::callbackVideoSourceConnection( bool _estab, const std::string & _sourceUrl ){

    // dump retranslators params
    if( ! _estab ){
        // TODO: cyclic video source destroy
//        SVideoSource source;
//        source.sourceUrl = _sourceUrl;
//        stopRetranslation( source );
        for( auto iter = m_retranslators.begin(); iter != m_retranslators.end(); ){
            PVideoRetranslator rtsp = ( * iter );
            if( rtsp->getSettings().sourceUrl == _sourceUrl ){
                m_sourceParametersFromDiedRetranslators.insert( {_sourceUrl, rtsp->getSettings().copyOfSourceParameters} );
                iter = m_retranslators.erase( iter );
            }
            else{
                ++iter;
            }
        }
    }
    // restore retranslators with params
    else{
        for( PVideoRetranslator & rtsp : m_retranslators ){
            if( rtsp->getSettings().sourceUrl == _sourceUrl ){
                VS_LOG_WARN << PRINT_HEADER
                         << " retranslator with such source  [" << _sourceUrl << "] already active."
                         << " Restore cancelled"
                         << endl;
                return;
            }
        }

        VS_LOG_WARN << PRINT_HEADER
                 << " restore retranslator with a source [" << _sourceUrl << "]"
                 << endl;

        const SVideoSource source = m_sourceParametersFromDiedRetranslators[ _sourceUrl ];
        if( ! createRetranslator(source) ){
            return;
        }
    }
}

std::vector<SVideoSource> SourceManagerFacade::getSourcesMedadata(){

    // TODO: ?

    return std::vector<SVideoSource>();
}

bool SourceManagerFacade::launchRetranslation( SVideoSource _source ){

    _source.sourceUrl = OBJREPR_BUS.getSensorUrl( _source.sensorId );

    // init source
    if( ! connectSource(_source) ){
        return false;
    }

    // watch for the source status
    addSourceObserver(_source.sourceUrl, this);

    if( ! createRetranslator(_source) ){
        return false;
    }

    return true;
}

bool SourceManagerFacade::createRetranslator( const SVideoSource & _source ){

    // find free udp ports
    int freeUdpPort = CONFIG_PARAMS.RTSP_SERVER_OUTGOING_UDP_PORT_RANGE_BEGIN;
    for( ;
         freeUdpPort <= CONFIG_PARAMS.RTSP_SERVER_OUTGOING_UDP_PORT_RANGE_END;
         freeUdpPort += 2 ){

        bool found = false;
        for( const PVideoRetranslator & rtspRetranslator : m_retranslators ){
            const VideoRetranslator::SInitSettings & settings = rtspRetranslator->getSettings();

            if( freeUdpPort == settings.outcomingUdpPacketsRangeBegin )  {
                found = true;
            }
        }

        if( ! found ){
            break;
        }
    }

    const int32_t freeUdpPortBegin = freeUdpPort;
    const int32_t freeUdpPortEnd = ++freeUdpPort;

    // check for exist
    for( PVideoRetranslator rtsp : m_retranslators ){
        if( rtsp->getSettings().sourceUrl == _source.sourceUrl ){
            stringstream ss;
            ss << "connect fail, such source alredy exist: " << _source.sourceUrl;
            VS_LOG_ERROR << ss.str() << endl;
            m_lastError = ss.str();
            return false;
        }
    }

    // shm info
    PVideoSourceProxy videoSourceForShmInfo;
    m_sourcesLock.lock();
    for( auto iter = m_videoSources.begin(); iter != m_videoSources.end(); ++iter ){
        PVideoSourceProxy videoSource = ( * iter );

        if( videoSource->getSettings().source.sourceUrl == _source.sourceUrl ){
            videoSourceForShmInfo = videoSource;
        }
    }
    m_sourcesLock.unlock();

    // connect
    vector<string> interfacesIp;
    if( ! CONFIG_PARAMS.RTSP_SERVER_LISTEN_INTERFACES_IP.empty() ){
        interfacesIp = CONFIG_PARAMS.RTSP_SERVER_LISTEN_INTERFACES_IP;
    }
    else{
        interfacesIp.push_back( VideoRetranslator::DEFAULT_LISTEN_ITF );
    }

    //
    ISourcesProvider::SSourceParameters sourceParameters;
    if( videoSourceForShmInfo ){
        sourceParameters = videoSourceForShmInfo->getStreamParameters();
    }

    //
    for( const string & itf : interfacesIp ){

        VideoRetranslator::SInitSettings settings;
        settings.rtspPostfix = CONFIG_PARAMS.RTSP_SERVER_POSTFIX;
        settings.outcomingUdpPacketsRangeBegin = freeUdpPortBegin;
        settings.outcomingUdpPacketsRangeEnd = freeUdpPortEnd;
        settings.serverPort = getRTSPSourcePort();
        settings.sourceUrl = _source.sourceUrl;
        settings.sensorId = _source.sensorId;
        settings.listenIp = itf;
        settings.userId = _source.userId;
        settings.userPassword = _source.userPassword;
        settings.latencyMillisec = _source.latencyMillisec;
        settings.outputEncoding = _source.outputEncoding;
        settings.intervideosinkChannelName = _source.sourceUrl;
        settings.shmRtpStreamEncoding = sourceParameters.rtpStreamEnconding;
        settings.shmRtpStreamCaps = sourceParameters.rtpStreamCaps;
        settings.shmRawStreamCaps = sourceParameters.rawStreamCaps;
        settings.monitorPipeline = false;
        settings.copyOfSourceParameters = _source;

        PVideoRetranslator retranslator = std::make_shared<VideoRetranslator>();
        if( ! retranslator->start(settings) ){
            m_lastError = retranslator->getLastError();
            continue;
        }

        m_retranslators.push_back( retranslator );
    }

    return true;
}

bool SourceManagerFacade::stopRetranslation( const SVideoSource & _source ){

    SVideoSource sourceCopy = _source;
    sourceCopy.sourceUrl = OBJREPR_BUS.getSensorUrl( _source.sensorId );

    // deinit source
    if( ! disconnectSource(sourceCopy.sourceUrl) ){
        return false;
    }

    //
    bool found = false;
    for( auto iter = m_retranslators.begin(); iter != m_retranslators.end(); ){
        PVideoRetranslator rtsp = ( * iter );
        if( rtsp->getSettings().sourceUrl == sourceCopy.sourceUrl ){
            iter = m_retranslators.erase( iter );
            found = true;
        }
        else{
            ++iter;
        }
    }

    if( found ){
        return true;
    }

    stringstream ss;
    ss << PRINT_HEADER << " retranslation stop failed, such source not found [" << sourceCopy.sourceUrl << "]";
    VS_LOG_ERROR << ss.str() << endl;
    m_lastError = ss.str();

    return false;
}

void SourceManagerFacade::addSourceObserver( const std::string & _sourceUrl, ISourceObserver * _observer ){

    std::lock_guard<std::mutex> lock( m_sourcesLock );

    for( PVideoSourceProxy videoSource : m_videoSources ){
        if( videoSource->getSettings().source.sourceUrl == _sourceUrl ){
            videoSource->addObserver( _observer );
        }
    }
}

PNetworkClient SourceManagerFacade::getCommunicatorWithSource( const std::string & _sourceUrl ){

    std::lock_guard<std::mutex> lock( m_sourcesLock );

    for( PVideoSourceProxy videoSource : m_videoSources ){
        if( videoSource->getSettings().source.sourceUrl == _sourceUrl ){
            return videoSource->getCommunicator();
        }
    }

    return nullptr;
}

ISourcesProvider::SSourceParameters SourceManagerFacade::getShmRtpStreamParameters( const std::string & _sourceUrl ){

    std::lock_guard<std::mutex> lock( m_sourcesLock );

    for( PVideoSourceProxy videoSource : m_videoSources ){
        if( videoSource->getSettings().source.sourceUrl == _sourceUrl ){
            return videoSource->getStreamParameters();
        }
    }

    return ISourcesProvider::SSourceParameters();
}

bool SourceManagerFacade::connectSource( SVideoSource _source ){

    if( ! enterToConnectQueue(_source) ){
        return false;
    }

#ifndef SEPARATE_SINGLE_SOURCE
    return true;
#endif

    // already exist
    m_sourcesLock.lock();
    for( PVideoSourceProxy videoSource : m_videoSources ){
        if( videoSource->getSettings().source.sourceUrl == _source.sourceUrl ){
            // TODO: smells like a little crutch
            VideoSourceProxy::SInitSettings settings = videoSource->getSettings();
            videoSource->connect(settings); // for "reference count"

            exitFromConnectQueue( _source, true );
            m_sourcesLock.unlock();
            return true;
        }
    }
    m_sourcesLock.unlock();

    // create new
    // TODO: custom parameters for particular source type must be setted by another way
    VideoSourceProxy::SInitSettings settings;
    settings.source.sourceUrl = _source.sourceUrl;
    settings.source.latencyMillisec = _source.latencyMillisec;
    settings.source.userId = _source.userId;
    settings.source.userPassword = _source.userPassword;
    settings.source.runArchiving = _source.runArchiving;
    settings.connectRetriesTimeoutSec = CONFIG_PARAMS.baseParams.SYSTEM_CONNECT_RETRIES * CONFIG_PARAMS.baseParams.SYSTEM_CONNECT_RETRY_PERIOD_SEC;
    settings.systemEnvironment = m_settings.serviceLocator.systemEnvironment;

#ifdef REMOTE_PROCESSING
    VideoSourceRemote2 * remote = new VideoSourceRemote2( m_settings.serviceLocator.internalCommunication );
    PVideoSourceProxy videoSource = std::make_shared<VideoSourceProxy>( remote );
#else
    VideoSourceLocal * local = new VideoSourceLocal();
    PVideoSourceProxy videoSource = std::make_shared<VideoSourceProxy>( local );
#endif

    m_videoSourcesInTransitionState.insert( {_source.sourceUrl, videoSource} );

    if( ! videoSource->connect(settings) ){
        m_lastError = videoSource->getLastError();
        return false;
    }

    m_sourcesLock.lock();
    m_videoSources.push_back( videoSource );
    m_videoSourcesInTransitionState.erase( _source.sourceUrl );
    m_sourcesLock.unlock();

    // TODO: temp 1 second for video-receiver full readyness
    this_thread::sleep_for( chrono::milliseconds(1000) );

    exitFromConnectQueue( _source, true );
    return true;
}

bool SourceManagerFacade::enterToConnectQueue( const SVideoSource & _source ){

    if( _source.sourceUrl.empty() ){
        stringstream ss;
        ss << PRINT_HEADER << " source url is EMPTY. Connect aborted";
        m_lastError = ss.str();
        VS_LOG_ERROR << ss.str() << endl;
        return false;
    }

    return true;

    // queue for multiple connectings to the same source
    SConsumerDescr * cd = nullptr;

    m_muConsumersQueue.lock();
    auto iter = m_consumersWaitingForConnect.find( _source.sensorId );
    if( iter != m_consumersWaitingForConnect.end() ){
        std::queue<SConsumerDescr *> & sensorConsumers = iter->second;

        cd = new SConsumerDescr();
        cd->sensorId = _source.sensorId;

        sensorConsumers.push( cd );
    }
    else{
        std::queue<SConsumerDescr *> sensorConsumers;

        cd = new SConsumerDescr();
        cd->sensorId = _source.sensorId;
        cd->allowToConnect = true;

        sensorConsumers.push( cd );
        m_consumersWaitingForConnect.insert( {_source.sensorId, sensorConsumers} );
    }
    m_muConsumersQueue.unlock();

    // wait for own time ( + timeout ? )
    while( ! cd->allowToConnect && ! cd->abortConnection ){
        // dummy
    }

    if( cd->abortConnection ){
        return false;
    }

    return true;
}

void SourceManagerFacade::exitFromConnectQueue( const SVideoSource & _source, bool _success ){

    m_muConsumersQueue.lock();
    auto iter = m_consumersWaitingForConnect.find( _source.sensorId );
    if( iter != m_consumersWaitingForConnect.end() ){
        std::queue<SConsumerDescr *> & sensorConsumers = iter->second;
        if( ! sensorConsumers.empty() ){
            // "give a road" for next consumer
            if( _success ){
                SConsumerDescr * cd = sensorConsumers.front();
                sensorConsumers.pop();
                delete cd;

                SConsumerDescr * nextCd = sensorConsumers.front();
                nextCd->allowToConnect = true;
            }
            // nofity all camera's consumers about fail
            else{
                while( ! sensorConsumers.empty() ){
                    SConsumerDescr * consumer = sensorConsumers.front();
                    consumer->abortConnection = true;
                    sensorConsumers.pop();
                }
            }
        }
    }
    m_muConsumersQueue.unlock();
}

bool SourceManagerFacade::disconnectSource( const std::string & _sourceUrl ){

    if( _sourceUrl.empty() ){
        stringstream ss;
        ss << PRINT_HEADER << " source url is EMPTY. Disconnect aborted";
        m_lastError = ss.str();
        VS_LOG_ERROR << ss.str() << endl;
        return false;
    }

    // disconnect from normal sources
    m_sourcesLock.lock();
    for( auto iter = m_videoSources.begin(); iter != m_videoSources.end(); ){
        PVideoSourceProxy videoSource = ( * iter );

        if( videoSource->getSettings().source.sourceUrl == _sourceUrl ){
            videoSource->disconnect(); // for "reference count"

            if( 0 == videoSource->getReferenceCount() ){
                iter = m_videoSources.erase( iter );
            }
            else{
                ++iter;
            }
        }
        else{
            ++iter;
        }
    }
    m_sourcesLock.unlock();

    // abort connection for stacked sources
    auto iter = m_videoSourcesInTransitionState.find( _sourceUrl );
    if( iter != m_videoSourcesInTransitionState.end() ){
        PVideoSourceProxy videoSource = iter->second;

        exitFromConnectQueue( videoSource->getSettings().source, false );

        videoSource->disconnect();
        m_videoSourcesInTransitionState.erase( _sourceUrl );
    }

    return true;
}

std::vector<VideoRetranslator::SCurrentState> SourceManagerFacade::getRTSPStates(){

    std::vector<VideoRetranslator::SCurrentState> out;

    for( PVideoRetranslator rtsp : m_retranslators ){
        out.push_back( rtsp->getState() );
    }

    return out;
}

std::vector<PConstVideoRetranslator> SourceManagerFacade::getRTSPSources() const {

    std::vector<PConstVideoRetranslator> out;

    for( PVideoRetranslator rtsp : m_retranslators ){
        out.push_back( rtsp );
    }

    return out;
}







