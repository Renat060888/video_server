
#include <boost/filesystem.hpp>
#include <microservice_common/system/logger.h>

#include "common/common_utils.h"
#include "system/config_reader.h"
#include "path_locator.h"

using namespace std;

static constexpr const char * PID_FILE_NAME = "/tmp/video_server_unique_lock_file.pid";
static constexpr const char * SHELL_IMITATION_DOMAIN_SOCKET_PATH = "/tmp/video_server_shell_interface.socket";
static constexpr const char * VIDEO_SERVER_IPC_BASE_PATH = "/tmp/";
static constexpr const char * VIDEO_SERVER_IPC_FILE_PREFIX = "video_server_ipc_";


// TODO: remove
static const string g_tempPFN = PID_FILE_NAME;
static const string g_tempDSP = SHELL_IMITATION_DOMAIN_SOCKET_PATH;

PathLocator::PathLocator()
{

}

PathLocator::~PathLocator()
{

}

void PathLocator::removePreviousSessionSocketFiles(){

    // TODO: not best place for this operation
    VS_LOG_INFO << "PathLocator: remove all previous session socket files..." << endl;

    try {
        // single file
        boost::filesystem::path filePath( SHELL_IMITATION_DOMAIN_SOCKET_PATH );
        if( boost::filesystem::exists(filePath) ){
            VS_LOG_INFO << "PathLocator: to remove [" << SHELL_IMITATION_DOMAIN_SOCKET_PATH << "]" << endl;
            boost::filesystem::remove( filePath );
        }

        // by pattern - analyzer
        {
            // by pattern
            const string targetPath = "/tmp/";
            const boost::regex filter1( "video_server_analyzer_.*\." );

            boost::filesystem::directory_iterator endIter;
            for( boost::filesystem::directory_iterator iter( targetPath ); iter != endIter; ++iter ){

                boost::smatch what;
                if( ! boost::regex_match( iter->path().filename().string(), what, filter1) ){
                    continue;
                }

                VS_LOG_INFO << "PathLocator: to remove [" << iter->path().filename().string() << "]" << endl;
                boost::filesystem::remove( iter->path() );
            }
        }

        // by pattern - video receiver
        {
            const string targetPath = "/tmp/";
            const boost::regex filter1( "video_server_vr_.*\." );

            boost::filesystem::directory_iterator endIter;
            for( boost::filesystem::directory_iterator iter( targetPath ); iter != endIter; ++iter ){

                boost::smatch what;
                if( ! boost::regex_match( iter->path().filename().string(), what, filter1) ){
                    continue;
                }

                VS_LOG_INFO << "PathLocator: to remove [" << iter->path().filename().string() << "]" << endl;
                boost::filesystem::remove( iter->path() );
            }
        }
    } catch( const boost::filesystem::filesystem_error & _err ){
        VS_LOG_WARN << "boost exception, WHAT: " << _err.what() << " CODE MSG: " << _err.code().message() << endl;
    }

    VS_LOG_INFO << "PathLocator: ...removing complete" << endl;
}

const std::string PathLocator::getUniqueLockFile(){
    return g_tempPFN;
}

const std::string PathLocator::getShellImitationDomainSocket(){
    return g_tempDSP;
}

const std::string PathLocator::getAnalyzerControlSocket( const std::string & _sourceUrl ){

    auto iter = m_analyzerOptions.find( _sourceUrl );
    if( iter != m_analyzerOptions.end() ){
        SAnalyzerOptions & opt = iter->second;
        return opt.controlSocketFullPath;
    }
    else{
        SAnalyzerOptions opt = createAnalyzerOptions( _sourceUrl );
        m_analyzerOptions.insert( {_sourceUrl, opt} );
        return opt.controlSocketFullPath;
    }
}

void PathLocator::removeAnalyzerControlSocket( const std::string & _sourceUrl ){

    auto iter = m_analyzerOptions.find( _sourceUrl );
    if( iter != m_analyzerOptions.end() ){
        SAnalyzerOptions & opt = iter->second;

        boost::filesystem::path filePath( opt.controlSocketFullPath );
        if( boost::filesystem::exists(filePath) ){
            VS_LOG_INFO << "PathLocator: to remove [" << opt.controlSocketFullPath << "]" << endl;
            boost::filesystem::remove( filePath );
        }

        m_analyzerOptions.erase( iter );
    }
    else{
        VS_LOG_INFO << "PathLocator: nothing to remove by [" << _sourceUrl << "] (analyzer)" << endl;
    }
}

const std::string PathLocator::getVideoReceiverControlSocket( const std::string & _sourceUrl ){

    auto iter = m_videoReceiverOptions.find( _sourceUrl );
    if( iter != m_videoReceiverOptions.end() ){
        SVideoReceiverOptions & opt = iter->second;
        return opt.controlSocketFullPath;
    }
    else{
        SVideoReceiverOptions opt = createVideoReceiverOptions( _sourceUrl );
        m_videoReceiverOptions.insert( {_sourceUrl, opt} );
        return opt.controlSocketFullPath;
    }
}

const std::string PathLocator::getVideoReceiverRawPayloadSocket( const std::string & _sourceUrl ){

    auto iter = m_videoReceiverOptions.find( _sourceUrl );
    if( iter != m_videoReceiverOptions.end() ){
        SVideoReceiverOptions & opt = iter->second;
        return opt.rawPayloadSocketFullPath;
    }
    else{
        SVideoReceiverOptions opt = createVideoReceiverOptions( _sourceUrl );
        m_videoReceiverOptions.insert( {_sourceUrl, opt} );
        return opt.rawPayloadSocketFullPath;
    }
}

const std::string PathLocator::getVideoReceiverRtpPayloadSocket( const std::string & _sourceUrl ){

    auto iter = m_videoReceiverOptions.find( _sourceUrl );
    if( iter != m_videoReceiverOptions.end() ){
        SVideoReceiverOptions & opt = iter->second;
        return opt.rtpPayloadSocketFullPath;
    }
    else{
        SVideoReceiverOptions opt = createVideoReceiverOptions( _sourceUrl );
        m_videoReceiverOptions.insert( {_sourceUrl, opt} );
        return opt.rtpPayloadSocketFullPath;
    }
}

const std::string PathLocator::getVideoReceiverCacheDir( const std::string & _sourceUrl ){

    auto iter = m_videoReceiverOptions.find( _sourceUrl );
    if( iter != m_videoReceiverOptions.end() ){
        SVideoReceiverOptions & opt = iter->second;
        return opt.cacheDirFullPath;
    }
    else{
        SVideoReceiverOptions opt = createVideoReceiverOptions( _sourceUrl );
        m_videoReceiverOptions.insert( {_sourceUrl, opt} );
        return opt.cacheDirFullPath;
    }
}

void PathLocator::removeVideoReceiverSocketsEnvironment( const std::string & _sourceUrl ){

    auto iter = m_videoReceiverOptions.find( _sourceUrl );
    if( iter != m_videoReceiverOptions.end() ){
        SVideoReceiverOptions & opt = iter->second;

        boost::filesystem::path filePath( opt.controlSocketFullPath );
        if( boost::filesystem::exists(filePath) ){
            unlink( opt.controlSocketFullPath.c_str() );
            VS_LOG_INFO << "PathLocator: to remove [" << opt.controlSocketFullPath << "]" << endl;
            boost::filesystem::remove( filePath );
        }

        boost::filesystem::path filePath2( opt.rawPayloadSocketFullPath );
        if( boost::filesystem::exists(filePath2) ){
            unlink( opt.rawPayloadSocketFullPath.c_str() );
            VS_LOG_INFO << "PathLocator: to remove [" << opt.rawPayloadSocketFullPath << "]" << endl;
            boost::filesystem::remove( filePath2 );
        }

        boost::filesystem::path filePath3( opt.rtpPayloadSocketFullPath );
        if( boost::filesystem::exists(filePath3) ){
            unlink( opt.rtpPayloadSocketFullPath.c_str() );
            VS_LOG_INFO << "PathLocator: to remove [" << opt.rtpPayloadSocketFullPath  << "]" << endl;
            boost::filesystem::remove( filePath3 );
        }

        m_videoReceiverOptions.erase( iter );
    }
    else{
        VS_LOG_INFO << "PathLocator: nothing to remove by [" << _sourceUrl << "] (video receiver)" << endl;
    }
}

PathLocator::SAnalyzerOptions PathLocator::createAnalyzerOptions( const std::string & _sourceUrl ){

    SAnalyzerOptions out;

    static int32_t magicIdGenerator = 0;
    magicIdGenerator++;

    string infix;
    const string::size_type doubleSlashPos = _sourceUrl.find("://");
    // by address
    if( doubleSlashPos != string::npos ){
        const string withoutDoubleSlash = _sourceUrl.substr( doubleSlashPos + 3, _sourceUrl.size() - doubleSlashPos );
        const string::size_type nextSlashPos = withoutDoubleSlash.find("/");
        const string hostPort = withoutDoubleSlash.substr( 0, nextSlashPos );
        infix = hostPort;
    }
    // simple by time
    else{
        // TODO: для разных процессов не самая лучшая идея
        infix = std::to_string( common_utils::getCurrentTimeSec() );
    }

    out.controlSocketFullPath = "/tmp/video_server_analyzer_control_sock_" + infix + "_" + to_string( magicIdGenerator );

    return out;
}

PathLocator::SVideoReceiverOptions PathLocator::createVideoReceiverOptions( const std::string & _sourceUrl ){

    SVideoReceiverOptions out;

    static int32_t magicIdGenerator = 0;
    magicIdGenerator++;

    string infix;
    const string::size_type doubleSlashPos = _sourceUrl.find("://");
    // by address
//    if( doubleSlashPos != string::npos ){
//        const string withoutDoubleSlash = _sourceUrl.substr( doubleSlashPos + 3, _sourceUrl.size() - doubleSlashPos );
//        const string::size_type nextSlashPos = withoutDoubleSlash.find("/");
//        const string hostPort = withoutDoubleSlash.substr( 0, nextSlashPos );
//        infix = hostPort;
//    }
    // simple by time
//    else{
        // TODO: для разных процессов не самая лучшая идея
        infix = std::to_string( common_utils::getCurrentTimeSec() );
//    }

    out.controlSocketFullPath = "/tmp/video_server_vr_control_sock_" + infix + "_" + to_string( magicIdGenerator );
    out.rawPayloadSocketFullPath = "/tmp/video_server_vr_raw_payload_sock_" + infix + "_" + to_string( magicIdGenerator );
    out.rtpPayloadSocketFullPath = "/tmp/video_server_vr_rtp_payload_sock_" + infix + "_" + to_string( magicIdGenerator );

    // TODO: ?
    out.cacheDirFullPath = CONFIG_PARAMS.HLS_WEBSERVER_DIR_ROOT;
//    out.cacheDirFullPath = string("/home/renat/5-resources/video_server/vr/") + to_string( magicIdGenerator );

    return out;
}











