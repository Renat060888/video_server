#ifndef PATH_LOCATOR_H
#define PATH_LOCATOR_H

#include <string>
#include <map>

class PathLocator
{
public:
    struct SVideoReceiverOptions {
        std::string controlSocketFullPath;
        std::string rawPayloadSocketFullPath;
        std::string rtpPayloadSocketFullPath;
        std::string cacheDirFullPath;
    };

    struct SAnalyzerOptions {
        std::string controlSocketFullPath;
    };

    static PathLocator & singleton(){
        static PathLocator instance;
        return instance;
    }

    void removePreviousSessionSocketFiles();

    const std::string getUniqueLockFile();
    const std::string getShellImitationDomainSocket();

    const std::string getAnalyzerControlSocket( const std::string & _sourceUrl );
    void removeAnalyzerControlSocket( const std::string & _sourceUrl );

    const std::string getVideoReceiverControlSocket( const std::string & _sourceUrl );
    const std::string getVideoReceiverRawPayloadSocket( const std::string & _sourceUrl );
    const std::string getVideoReceiverRtpPayloadSocket( const std::string & _sourceUrl );
    const std::string getVideoReceiverCacheDir( const std::string & _sourceUrl );
    void removeVideoReceiverSocketsEnvironment( const std::string & _sourceUrl );


private:
    PathLocator();
    ~PathLocator();

    PathLocator( const PathLocator & _inst ) = delete;
    PathLocator & operator=( const PathLocator & _inst ) = delete;

    SVideoReceiverOptions createVideoReceiverOptions( const std::string & _sourceUrl );
    SAnalyzerOptions createAnalyzerOptions( const std::string & _sourceUrl );

    std::map<std::string, SVideoReceiverOptions> m_videoReceiverOptions;
    std::map<std::string, SAnalyzerOptions> m_analyzerOptions;
};
#define PATH_LOCATOR PathLocator::singleton()

#endif // PATH_LOCATOR_H
