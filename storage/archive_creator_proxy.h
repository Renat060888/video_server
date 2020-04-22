#ifndef RECORDER_PROXY_H
#define RECORDER_PROXY_H

#include <string>
#include <mutex>
#include <memory>

#include "common/common_types.h"
#include "storage/objrepr_repository.h"

class ArchiveCreatorMemento {

};

// interface
class AArchiveCreator
{
public:
    virtual ~AArchiveCreator(){}

    struct SInitSettings {
        SInitSettings()
            : playlistLength(0)
            , maxFiles(0)
            , sensorId(0)
            , tempCrutchStartRecordTimeMillisec(0)
            , serviceOfSourceProvider(nullptr)
        {}
        // for local archiver
        std::string playlistDirLocationFullPath;
        int32_t playlistLength;
        int32_t maxFiles;        

        // common
        std::string sourceUrl; // TODO: move to SCurrentState
        common_types::TSensorId sensorId;
        common_types::TArchivingId archivingId;
        std::string archivingName;
        PNetworkClient communicationWithVideoSource;

        // TODO: must be removed
        int64_t tempCrutchStartRecordTimeMillisec;

        // services
        common_types::ISourcesProvider * serviceOfSourceProvider;
    };

    struct SCurrentState {
        SCurrentState()
            : initSettings(nullptr)
            , state(common_types::EArchivingState::UNDEFINED)
            , timeStartMillisec(0)
            , timeEndMillisec(0)
            , connectRetries(0)
        {}
        SInitSettings * initSettings; // TODO: terrible ?
        common_types::EArchivingState state;

        std::string playlistName;
        std::string sessionName;
        int64_t timeStartMillisec;
        int64_t timeEndMillisec;

        // TOOD: deprecated
        int32_t connectRetries;
    };

    virtual bool start( const SInitSettings & _settings ) = 0;
    virtual void stop() = 0;

    virtual bool isConnectionLosted() const = 0;
    virtual void tick() = 0;

    virtual const SInitSettings & getSettings() const = 0;
    virtual const SCurrentState & getCurrentState() = 0;
    virtual const std::string & getLastError() = 0;

    // TODO: pattern 'Memento' for state remember
    virtual ArchiveCreatorMemento getMemento(){}

protected:
    virtual bool getArchiveSessionInfo( int64_t & _startRecordTimeMillisec,
                                        std::string & _sessionName,
                                        std::string & _playlistName ){ return false; }

    void transferOriginalPlaylistToLiveStream( const std::string & _origFullPath, const std::string & _liveStreamFullPath );
    void checkOriginalPlaylistConsistent( const std::string & _originalPlaylistFullPath );
    bool createMasterPlaylist( const std::string & _saveLocation, const SInitSettings & _settings );
    std::string createLiveStreamPlaylist(   const std::string & _saveLocation,
                                            const std::string & _urlLocation,
                                            const common_types::SHlsMetadata & _meta,
                                            const SInitSettings & _settings );

    // TODO: may be as abstract service ?
    ObjreprRepository m_objreprRepository;
    std::mutex m_muLiveStreamPlaylist;
    std::ifstream m_originalPlaylistFile;
    std::ofstream m_liveStreamPlaylistFile;

private:


};



// proxy
class ArchiveCreatorProxy : public AArchiveCreator
{
public:
    ArchiveCreatorProxy( AArchiveCreator * _impl );
    ~ArchiveCreatorProxy();

    virtual bool start( const SInitSettings & _settings ) override;
    virtual void stop() override;

    virtual bool isConnectionLosted() const override;
    virtual void tick() override;

    virtual const SInitSettings & getSettings() const override;
    virtual const SCurrentState & getCurrentState() override;
    virtual const std::string & getLastError() override;

private:
    AArchiveCreator * m_impl;
};
using PArchiveCreatorProxy = std::shared_ptr<ArchiveCreatorProxy>;
using PArchiveCreatorProxyConst = std::shared_ptr<const ArchiveCreatorProxy>;

#endif // RECORDER_PROXY_H
