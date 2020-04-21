#ifndef VIDEO_RECEIVER_PROTOCOL_H
#define VIDEO_RECEIVER_PROTOCOL_H

#include <string>

#include "video_receiver_typedefs.h"

class VideoReceiverProtocol
{
public:       
    VideoReceiverProtocol();

    static VideoReceiverProtocol & singleton(){
        static VideoReceiverProtocol instance;
        return instance;
    }

    // requests
    std::string createSimpleCommand( CommandType type );
    std::string createOptionsCommand(std::string name, CommandOptions options );
    std::string createSeekCommand( uint64_t msec_time );
    std::string createRecordCommand( const std::string &path, uint32_t backward, uint32_t forward, std::string tag = std::string() );
    std::string createAddFilterCommand( const std::string &code, const std::string &name );
    std::string createDelFilterCommand( const std::string &name );
    std::string createAddAnalyserCommand( const std::string &code, const std::string &name );
    std::string createDelAnalyserCommand( const std::string &name );

    // responses
    AnswerType getAnswerType( const std::string & json );
    FrameOptions getFrameOptions( std::string json );
    RecordData getRecordData( std::string json );
    EventData getEventData( std::string json );
    ArchiveSession getArchiveData( std::string json );
    InfoData getInfoData( std::string json );
    AnalyticData getAnalyticData( std::string json );

private:

};

#endif // VIDEO_RECEIVER_PROTOCOL_H
