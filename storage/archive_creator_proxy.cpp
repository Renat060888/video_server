
#include <microservice_common/system/logger.h>

#include "common/common_utils.h"
#include "archive_creator_proxy.h"

using namespace std;
using namespace common_types;

static constexpr const char * PLAYLIST_CLOSING_TAG = "#EXT-X-ENDLIST";
static constexpr const char * PRINT_HEADER = "ArchiverItf:";

//
void AArchiveCreator::checkOriginalPlaylistConsistent( const std::string & _originalPlaylistFullPath ){

    std::lock_guard<std::mutex> lock( m_muLiveStreamPlaylist );

    // check this last line to 'closing tag' equal
    const string lastLine = common_utils::getLastLine( _originalPlaylistFullPath );

    if( lastLine.find( PLAYLIST_CLOSING_TAG ) == string::npos ){
        std::ofstream originalPlaylistFile;

        VS_LOG_WARN << PRINT_HEADER
                    << " closing tag is absent in original playlist [" << _originalPlaylistFullPath << "]"
                    << " append it to the end"
                    << endl;

        originalPlaylistFile.open( _originalPlaylistFullPath, std::ios::app );
        if( ! originalPlaylistFile.is_open() ){
//            VS_LOG_WARN << "can't open original playlist [" << _originalPlaylistFullPath << "] for reading 1" << endl;
            return;
        }

        originalPlaylistFile << PLAYLIST_CLOSING_TAG;
        originalPlaylistFile.close();
    }
}

bool AArchiveCreator::createMasterPlaylist( const string & _saveLocation, const SInitSettings & _settings ){

    ISourcesProvider::SSourceParameters srcParams = _settings.serviceOfSourceProvider->getShmRtpStreamParameters( _settings.sourceUrl );

    // values
    const int32_t bandwidthHigh = common_utils::getBandwidthByResolution( srcParams.frameWidth, srcParams.frameHeight, EVideoQualitySimple::HIGH );
    if( 0 == bandwidthHigh ){
        VS_LOG_WARN << "ZERO bandwidth value for video source with"
                  << " W [" << srcParams.frameWidth << "]"
                  << " H [" << srcParams.frameHeight << "]"
                  << endl;
    }

    stringstream ss;
    ss << "#EXTM3U" << endl
       << "#EXT-X-VERSION:6" << endl
       << "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=%1%,CODECS=\"avc1.640028,mp4a.40.2\",RESOLUTION=%2%x%3%" << endl
       << "hi.m3u8" << endl; // low.m3u8

    const string masterPlaylistText = ( boost::format( ss.str().c_str() )
                                 % bandwidthHigh
                                 % srcParams.frameWidth
                                 % srcParams.frameHeight
                                ).str();

    // file
    const string MASTER_PLAYLIST_NAME = "master.m3u8";
    const string masterPlaylistFileFullPath = _saveLocation + "/" + MASTER_PLAYLIST_NAME;

    ofstream masterPlaylistFile( masterPlaylistFileFullPath );
    if( ! masterPlaylistFile.is_open() ){
        VS_LOG_ERROR << " can't open full path for file saving [" << masterPlaylistFileFullPath << "]" << endl;
        return false;
    }

    masterPlaylistFile << masterPlaylistText;
    masterPlaylistFile.close();

    return true;
}

std::string AArchiveCreator::createLiveStreamPlaylist( const std::string & _saveLocation,
                                                            const std::string & _urlLocation,
                                                            const SHlsMetadata & _meta,
                                                            const SInitSettings & _settings ){

    const string LIVE_STREAM_PLAYLIST_NAME = "archive.m3u8"; // live_stream.m3u8
    const string liveStreamPlaylistDirFullPath = _saveLocation + "/" + LIVE_STREAM_PLAYLIST_NAME;

    std::ofstream liveStreamPlaylistFile( liveStreamPlaylistDirFullPath, std::ios::out | std::ios::trunc );
    if( ! liveStreamPlaylistFile.is_open() ){
        VS_LOG_ERROR << "can't open live stream playlist [" << liveStreamPlaylistDirFullPath << "] for writing" << endl;
        return std::string();
    }
    liveStreamPlaylistFile.close();

    const string liveStreamPlaylistUrlFullPath = _urlLocation + "/" + LIVE_STREAM_PLAYLIST_NAME;
    if( ! m_objreprRepository.setArchiveLiveStreamPlaylist(_meta, liveStreamPlaylistUrlFullPath) ){
        VS_LOG_ERROR << "archive [" << _settings.sourceUrl << "] 'live stream playlist' set failed" << endl;
        return std::string();
    }

    return liveStreamPlaylistDirFullPath;
}

void AArchiveCreator::transferOriginalPlaylistToLiveStream( const std::string & _origFullPath, const std::string & _liveStreamFullPath ){

    std::lock_guard<std::mutex> lock( m_muLiveStreamPlaylist );

    // TODO: make more optimized

    //
    m_originalPlaylistFile.open( _origFullPath, std::ios::in );
    if( ! m_originalPlaylistFile.is_open() ){
//        VS_LOG_WARN << "can't open original playlist [" << _origFullPath << "] for reading 2" << endl;
        return;
    }

    const string originalPlaylistText = string( std::istreambuf_iterator<char>(m_originalPlaylistFile),
                                                std::istreambuf_iterator<char>() );
    m_originalPlaylistFile.close();

    //
    m_liveStreamPlaylistFile.open( _liveStreamFullPath, std::ios::out | std::ios::trunc );
    if( ! m_liveStreamPlaylistFile.is_open() ){
        VS_LOG_WARN << "can't open live stream playlist [" << _liveStreamFullPath << "] for reading" << endl;
        return;
    }

    m_liveStreamPlaylistFile << originalPlaylistText;            
    m_liveStreamPlaylistFile.close();

    // TODO: heavy operation, think about more effective code
    if( common_utils::getLastLine(_liveStreamFullPath) != PLAYLIST_CLOSING_TAG ){
        m_liveStreamPlaylistFile.open( _liveStreamFullPath, std::ios::app );
        m_liveStreamPlaylistFile << PLAYLIST_CLOSING_TAG;
        m_liveStreamPlaylistFile.close();
    }
}

//
ArchiveCreatorProxy::ArchiveCreatorProxy( AArchiveCreator * _impl )
    : m_impl(_impl)
{

}

ArchiveCreatorProxy::~ArchiveCreatorProxy()
{
    delete m_impl;
    m_impl = nullptr;
}

bool ArchiveCreatorProxy::start( const SInitSettings & _settings ){
    return m_impl->start(_settings);
}

void ArchiveCreatorProxy::stop() {
    return m_impl->stop();
}

void ArchiveCreatorProxy::tick() {
    return m_impl->tick();
}

bool ArchiveCreatorProxy::isConnectionLosted() const {
    return m_impl->isConnectionLosted();
}
const ArchiveCreatorProxy::SInitSettings & ArchiveCreatorProxy::getSettings() const {
    return m_impl->getSettings();
}

const ArchiveCreatorProxy::SCurrentState & ArchiveCreatorProxy::getCurrentState(){
    return m_impl->getCurrentState();
}

const std::string & ArchiveCreatorProxy::getLastError(){
    return m_impl->getLastError();
}
