#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <microservice_common/common/ms_common_types.h>
#include <microservice_common/communication/network_interface.h>

// ---------------------------------------------------------------------------
// forwards ( outside of any namespace )
// ---------------------------------------------------------------------------
class SourceManagerFacade;
class StorageEngineFacade;
class AnalyticManagerFacade;
class SystemEnvironmentFacadeVS;
class CommunicationGatewayFacadeVS;


namespace common_types {


// ---------------------------------------------------------------------------
// forwards ( inside namespace )
// ---------------------------------------------------------------------------
class IEventVisitor;
class IMetadataVisitor;
class IEventInternalVisitor;

// ---------------------------------------------------------------------------
// global typedefs
// ---------------------------------------------------------------------------
using TUrl = std::string;
using TProfileId = uint64_t;
using TProcessingId = std::string;
using TArchivingId = std::string;
using TAnalyzeObjectId = uint64_t;

// ---------------------------------------------------------------------------
// enums
// ---------------------------------------------------------------------------
enum class EOutputEncoding {
    H264,
    JPEG,
    UNDEFINED
};

enum class EPlayingStatus {
    INITED,
    CONTEXT_LOADED,
    PLAYING,
    ALL_STEPS_PASSED,
    NOT_ENOUGH_STEPS,
    PAUSED,
    STOPPED,
    PLAY_FROM_POSITION,
    CRASHED,
    CLOSE,
    UNDEFINED
};

// NOTE: must be equal in video_server_client
enum class EAnalyzeState {
    UNAVAILABLE,
    PREPARING,
    READY,
    ACTIVE,
    CRUSHED,
    UNDEFINED
};

// NOTE: must be equal in video_server_client
enum class EArchivingState {
    UNAVAILABLE,
    PREPARING,
    READY,
    ACTIVE,
    CRUSHED,
    UNDEFINED
};

enum class EVideoQualitySimple {
    HIGH,
    MIDDLE,
    LOW,
    UNDEFINED
};

// ---------------------------------------------------------------------------
// simple ADT
// ---------------------------------------------------------------------------
struct SVideoSource {
    SVideoSource()
        : latencyMillisec(1000)
        , outputEncoding(EOutputEncoding::UNDEFINED)

        , runArchiving(false)
        , tempCrutchStartRecordTimeMillisec(0)

        , sensorId(0)
    {}
    std::string sourceUrl;
    std::string userId;
    std::string userPassword;
    int latencyMillisec;
    EOutputEncoding outputEncoding;

    // ============================================= TODO: must be removed
    bool runArchiving;
    int64_t tempCrutchStartRecordTimeMillisec;
    std::string tempCrutchSessionName;
    std::string tempCrutchPlaylistName;
    // ============================================= TODO: must be removed

    // TODO: for archiver
    TSensorId sensorId;
    std::string archivingName;
};

struct SAnalyticFilter {
    SAnalyticFilter()
        : ctxId(0)
        , sensorId(0)
        , sessionId(0)
        , maxLogicStep(0)
        , minLogicStep(0)
    {}

    TContextId ctxId;
    TSensorId sensorId;

    TSessionNum sessionId;
    TLogicStep maxLogicStep;
    TLogicStep minLogicStep;
};

// TODO: where to move ?
// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
struct SDetectedObject {
    enum class EState : int32_t {
        ACTIVE,
        DESTROYED,
        ABSENT,
        UNDEFINED
    };

    TObjectId id;
    TSessionNum session;
    EState state;
    int64_t timestampMillisec;
    TLogicStep logicTime;
    double latDeg;
    double lonDeg;
    double heading;
};

struct SEventFromStorage /*: SEvent*/ {

    // TODO: ?
//    virtual void accept( IEventVisitor * _visitor ) const override;

    std::vector<SDetectedObject> objects;
};


// ---------------------------------------------------------------------------
// exchange ADT ( component <-> store, component <-> network, etc... )
// ---------------------------------------------------------------------------
// events
struct SEvent {
    virtual ~SEvent(){}

    virtual void accept( IEventVisitor * _visitor ) const = 0;
};

struct SAnalyticEvent : SEvent {

    virtual void accept( IEventVisitor * _visitor ) const override;

    TSensorId sensorId;
    TContextId ctxId;
    TProcessingId processingId;

    std::string eventMessage;
    std::string pluginName;

    // TODO: what for?
    std::string type_plugin;
};

struct SAnalyzeStateEvent : SEvent {

    virtual void accept( IEventVisitor * _visitor ) const override;

    TSensorId sensorId;
    std::string processingId;
    EAnalyzeState analyzeState;
    std::string message;
};

struct SArchiverStateEvent : SEvent {

    virtual void accept( IEventVisitor * _visitor ) const override;

    TSensorId sensorId;
    std::string archivingId;
    EArchivingState archivingState;
    std::string message;
};

struct SOnceMoreEvent : SEvent {

    virtual void accept( IEventVisitor * _visitor ) const override;

    std::string json;
    std::string name_plugin;
    std::string type_plugin;
    std::string name_source;

};

// metadata
struct SMetadata {
    virtual ~SMetadata(){}

    virtual void accept( IMetadataVisitor * _visitor ) const = 0;
};

struct SHlsMetadata : SMetadata {
    SHlsMetadata()
        : timeStartMillisec(0)
        , timeEndMillisec(0)
        , sensorId(0)
    {}

    virtual void accept( IMetadataVisitor * _visitor ) const override;

    std::string tag;
    std::string sourceUrl;
    std::string playlistDirLocationFullPath;
    int64_t timeStartMillisec;
    int64_t timeEndMillisec;
    TSensorId sensorId;

    // ============================================= TODO: must be removed
    std::string playlistWebLocationFullPath;
    // ============================================= TODO: must be removed
};

struct SBookmark : SMetadata {

    virtual void accept( IMetadataVisitor * _visitor ) const override;

    int32_t archiveId;
    std::string sourceUrl;
    std::string label;
    int64_t timePointSec;
    // ?
};

struct SOnceMoreMetadata : SMetadata {

    virtual void accept( IMetadataVisitor * _visitor ) const override;

    std::string tag;
    std::string sourceUrl;
    std::string playlistPath;
    int64_t timeStartMillisec;
    int64_t timeEndMillisec;
};

// internal event
struct SInternalEvent {
    virtual ~SInternalEvent(){}

    virtual void accept( IEventInternalVisitor * _visitor ) const = 0;
};

struct SAnalyzedObjectEvent : SInternalEvent {

    virtual void accept( IEventInternalVisitor * _visitor ) const override;

    TProcessingId processingId;
    std::vector<TAnalyzeObjectId> objreprObjectId;
};

// ---------------------------------------------------------------------------
// types deduction
// ---------------------------------------------------------------------------
class IEventVisitor {
public:
    virtual ~IEventVisitor(){}

    virtual void visit( const SAnalyticEvent * _event ) = 0;
    virtual void visit( const SAnalyzeStateEvent * _event ) = 0;
    virtual void visit( const SArchiverStateEvent * _event ) = 0;
    virtual void visit( const SOnceMoreEvent * _event ) = 0;

private:

};

class IEventInternalVisitor {
public:
    virtual ~IEventInternalVisitor(){}

    virtual void visit( const SAnalyzedObjectEvent * _record ) = 0;

private:

};

// ---------------------------------------------------------------------------
// service interfaces
// ---------------------------------------------------------------------------
// analyze itf
class IObjectsPlayer {
public:
    struct SBaseState {
        SBaseState()
            : playerPid(0)
            , playingStatus(EPlayingStatus::UNDEFINED)
            , openedContextId(0)
        {}
        TPid playerPid;
        EPlayingStatus playingStatus;
        TContextId openedContextId;
        std::string lastError;
    };

    virtual ~IObjectsPlayer(){}

    virtual void * getState() = 0;
    virtual const std::string & getLastError() = 0;

    virtual void start() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;
    virtual bool stepForward() = 0;
    virtual bool stepBackward() = 0;

    virtual bool setRange( const TTimeRangeMillisec & _range ) = 0;
    virtual void switchReverseMode( bool _reverse ) = 0;
    virtual void switchLoopMode( bool _loop ) = 0;
    virtual bool playFromPosition( int64_t _stepMillisec ) = 0;
    virtual bool updatePlayingData() = 0;

    virtual bool increasePlayingSpeed() = 0;
    virtual bool decreasePlayingSpeed() = 0;
    virtual void normalizePlayingSpeed() = 0;
};

// communication itf
class IInternalCommunication {
public:
    virtual ~IInternalCommunication(){}

    virtual PNetworkClient getAnalyticCommunicator( std::string _clientName ) = 0;
    virtual PNetworkClient getSourceCommunicator( std::string _clientName ) = 0;
    virtual PNetworkClient getArchiveCommunicator( std::string _clientName ) = 0;
    virtual PNetworkClient getServiceCommunicator() = 0;

    virtual PNetworkClient getEventRetranslateCommunicator( std::string _clientName ) = 0;
    virtual PNetworkClient getPlayerCommunicator( std::string _clientName ) = 0;
};

class IExternalCommunication {
public:
    virtual ~IExternalCommunication(){}

    virtual PNetworkClient getConnection( const INetworkEntity::TConnectionId _connId ) = 0;
    virtual PNetworkClient getFileDownloader() = 0;


};

class IEventNotifier {
public:
    virtual ~IEventNotifier(){}

    virtual void sendEvent( const SEvent & _event ) = 0;
};

class IEventRetranslator {
public:
    virtual ~IEventRetranslator(){}

    virtual void retranslateEvent( const SEvent & _event ) = 0;
};

// storage itf
class IEventStore {
public:
    virtual ~IEventStore(){}

    virtual void writeEvent( const SEvent & _event ) = 0;
    virtual SEventFromStorage readEvents( const SAnalyticFilter & _filter ) = 0;
    virtual void deleteEvent( const SEvent & _event ) = 0;
};

class IEventProcessor {
public:
    virtual ~IEventProcessor(){}

    virtual bool addEventProcessor( const std::string _sourceUrl, const TProcessingId & _procId ) = 0;
    virtual bool removeEventProcessor( const TProcessingId & _procId ) = 0;
    virtual void processEvent( const SInternalEvent & _event ) = 0;
};

class IRecordingMetadataStore {
public:
    virtual ~IRecordingMetadataStore(){}

    virtual void writeMetadata( const SMetadata & _metadata ) = 0;
    virtual std::vector<SHlsMetadata> readMetadatas() = 0;
    virtual void deleteMetadata( const SMetadata & _metadata ) = 0;
};

class IVideoStreamStore {
public:
    virtual ~IVideoStreamStore(){}

    virtual bool startRecording( SVideoSource _source ) = 0;
    virtual bool stopRecording( SVideoSource _source ) = 0;
};

class IRecordsProvider {
public:
    virtual ~IRecordsProvider(){}

};

// source itf
class ISourceObserver {
public:
    virtual ~ISourceObserver(){}

    virtual void callbackVideoSourceConnection( bool _estab, const std::string & _sourceUrl ) = 0;
};

class ISourcesProvider {
public:
    struct SSourceParameters {
        SSourceParameters()
            : frameWidth(0)
            , frameHeight(0)
        {}
        std::string rtpStreamEnconding;
        std::string rtpStreamCaps;
        std::string rawStreamCaps;
        int frameWidth;
        int frameHeight;
    };

    virtual ~ISourcesProvider(){}

    virtual std::vector<SVideoSource> getSourcesMedadata() = 0;
    virtual std::string getLastError() = 0;

    // NOTE: source interface for WHO connect ( see also IVideoSource )
    virtual bool connectSource( SVideoSource _source ) = 0;
    virtual bool disconnectSource( const std::string & _sourceUrl  ) = 0;
    virtual void addSourceObserver( const std::string & _sourceUrl, ISourceObserver * _observer ) = 0;
    virtual PNetworkClient getCommunicatorWithSource( const std::string & _sourceUrl ) = 0;
    virtual ISourcesProvider::SSourceParameters getShmRtpStreamParameters( const std::string & _sourceUrl ) = 0;

};

class ISourceRetranslation {
public:
    virtual ~ISourceRetranslation(){}

    virtual bool launchRetranslation( SVideoSource _source ) = 0;
    virtual bool stopRetranslation( const SVideoSource & _source ) = 0;
};




// ---------------------------------------------------------------------------
// service locator
// ---------------------------------------------------------------------------
struct SIncomingCommandServices : SIncomingCommandGlobalServices {
    SIncomingCommandServices()
        : systemEnvironment(nullptr)
        , sourceManager(nullptr)
        , storageEngine(nullptr)
        , communicationGateway(nullptr)
    {}

    SystemEnvironmentFacadeVS * systemEnvironment;
    SourceManagerFacade * sourceManager;
    StorageEngineFacade * storageEngine;
    AnalyticManagerFacade * analyticManager;
    CommunicationGatewayFacadeVS * communicationGateway;
    std::function<void()> signalShutdownServer;
};

}

#endif // COMMON_TYPES_H
