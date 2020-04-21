#ifndef CONFIG_READER_H
#define CONFIG_READER_H

#include <microservice_common/system/a_config_reader.h>

class ConfigReader : public AConfigReader
{
public:
    struct SConfigParameters {
        AConfigReader::SConfigParameters baseParams;

        std::string GSTREAMER_CUSTOM_PLUGINS_PATH;

        int32_t RTSP_SERVER_INITIAL_PORT;
        int32_t RTSP_SERVER_OUTGOING_UDP_PORT_RANGE_BEGIN;
        int32_t RTSP_SERVER_OUTGOING_UDP_PORT_RANGE_END;
        std::string RTSP_SERVER_POSTFIX;
        std::vector<std::string> RTSP_SERVER_LISTEN_INTERFACES_IP;

        std::string HLS_WEBSERVER_URL_ROOT;
        std::string HLS_WEBSERVER_DIR_ROOT;
        std::string HLS_CHUNK_PATH_TEMPLATE;
        std::string HLS_PLAYLIST_NAME;
        int32_t HLS_MAX_FILE_COUNT;

        bool RECORD_ENABLE;
        bool RECORD_ENABLE_STORE_POLICY;
        int64_t RECORD_METADATA_UPDATE_INTERVAL_SEC;
        int32_t RECORD_NEW_DATA_BLOCK_INTERVAL_MIN;
        int16_t RECORD_DATA_OUTDATE_AFTER_HOUR;
        bool RECORD_ENABLE_RETRANSLATION;
        bool RECORD_ENABLE_REMOTE_ARCHIVER;

        bool PROCESSING_ENABLE_IMAGE_ATTRS;
        bool PROCESSING_ENABLE_VIDEO_ATTRS;
        bool PROCESSING_ENABLE_RECTS_ON_TEST_SCREEN;
        int32_t PROCESSING_MAX_PROCESSINGS_PER_VIDEOCARD;
        bool PROCESSING_ENABLE_OPTIC_STREAM;
        int32_t PROCESSING_MAX_OBJECTS_FOR_BEST_IMAGE_TRACKING;
        bool PROCESSING_ENABLE_ABSENT_STATE_AT_OBJECT_DISAPPEAR;
        bool PROCESSING_RETRANSLATE_EVENTS;
        bool PROCESSING_STORE_EVENTS;
        double PROCESSING_FACE_ANALYZER_MAX_DISTANCE;
    };

    static ConfigReader & singleton(){
        static ConfigReader instance;
        return instance;
    }

    const SConfigParameters & get(){ return m_parameters; }


private:
    ConfigReader();
    ~ConfigReader(){}

    ConfigReader( const ConfigReader & _inst ) = delete;
    ConfigReader & operator=( const ConfigReader & _inst ) = delete;

    virtual bool initDerive( const SIninSettings & _settings ) override;
    virtual bool parse( const std::string & _content ) override;
    virtual bool createCommandsFromConfig( const std::string & _content ) override;
    virtual std::string getConfigExampleDerive() override;


    // data
    SConfigParameters m_parameters;

};
#define CONFIG_READER ConfigReader::singleton()
#define CONFIG_PARAMS ConfigReader::singleton().get()

#endif // CONFIG_READER_H
