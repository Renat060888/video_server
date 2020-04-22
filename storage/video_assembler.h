#ifndef VIDEO_ASSEMBLER_H
#define VIDEO_ASSEMBLER_H

#include <memory>
#include <mutex>
#include <unordered_map>

#include <gst/gst.h>

#include "common/common_types.h"

class VideoAssembler : INetworkObserver
{
public:    
    struct SInitSettings {
        PNetworkClient communicationWithVideoSource;
        common_types::TProcessingId correlatedProcessingId;
    };

    struct SAssembledVideo {
        SAssembledVideo()
            : forObjectId(0)
            , ready(false)
        {}
        common_types::TObjectId forObjectId;
        std::string location;
        std::string taskId;
        bool ready;
    };

    VideoAssembler();
    ~VideoAssembler();

    bool init( SInitSettings _settings );
    const SInitSettings & getSettings(){ return m_settings; }
    const std::string & getLastError(){ return m_lastError; }

    bool run( uint64_t _objreprObjectId );
    std::vector<SAssembledVideo *> getReadyVideos();

    bool run();

private:
    virtual void callbackNetworkRequest( PEnvironmentRequest _request ) override;

    static gboolean myBusCallback( GstBus * bus, GstMessage * message, gpointer uData );


    // data
    SInitSettings m_settings;
    std::string m_lastError;
    std::unordered_map<common_types::TObjectId, SAssembledVideo *> m_assembledVideosByObjectId;
    std::map<std::string, SAssembledVideo *> m_assembledVideosByTaskId;


    // service
    std::mutex m_muAssembledVideos;

};
using PVideoAssembler = std::shared_ptr<VideoAssembler>;

#endif // VIDEO_ASSEMBLER_H
