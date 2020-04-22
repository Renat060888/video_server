
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <microservice_common/common/ms_common_utils.h>

#include "common/common_types.h"
#include "config_reader.h"

using namespace std;
using namespace common_types;

static constexpr const char * PRINT_HEADER = "ConfigReader:";

ConfigReader::ConfigReader()
{

}


bool ConfigReader::initDerive( const SIninSettings & _settings ){

    m_parameters.baseParams = AConfigReader::m_parameters;
    return true;
}

bool ConfigReader::parse( const std::string & _content ){

    // parse base part
    boost::property_tree::ptree config;

    istringstream contentStream( _content );
    try{
        boost::property_tree::json_parser::read_json( contentStream, config );
    }
    catch( boost::property_tree::json_parser::json_parser_error & _ex ){
        PRELOG_ERR << ::PRINT_HEADER << " parse failed of [" << _content << "]" << endl
             << "Reason: [" << _ex.what() << "]" << endl;
        return false;
    }

    // get values
    boost::property_tree::ptree gstreamer = config.get_child("gstreamer");
    m_parameters.GSTREAMER_CUSTOM_PLUGINS_PATH = setParameterNew<std::string>( gstreamer, "custom_plugins_path", string("/tmp") );


    boost::property_tree::ptree RTSP = config.get_child("rtsp_server");
    m_parameters.RTSP_SERVER_INITIAL_PORT = setParameterNew<int32_t>( RTSP, "initial_port", 4567 );
    m_parameters.RTSP_SERVER_OUTGOING_UDP_PORT_RANGE_BEGIN = setParameterNew<int32_t>( RTSP, "stream_udp_port_range_begin", 0 );
    m_parameters.RTSP_SERVER_OUTGOING_UDP_PORT_RANGE_END = setParameterNew<int32_t>( RTSP, "stream_udp_port_range_end", 0 );
    m_parameters.RTSP_SERVER_POSTFIX = setParameterNew<std::string>( RTSP, "postfix", string("video_server_stream") );
    boost::property_tree::ptree interfacesIp = RTSP.get_child("listen_interfaces_ip");
    for( auto iter = interfacesIp.begin(); iter != interfacesIp.end(); ++iter ){
        boost::property_tree::ptree arrEl = iter->second;

        try {
        if( arrEl.get<bool>("enable") ){
            m_parameters.RTSP_SERVER_LISTEN_INTERFACES_IP.push_back( arrEl.get<std::string>("address") );
        }
        } catch( boost::exception & _ex ){
            continue;
        }
    }


    boost::property_tree::ptree HLS = config.get_child("hls_generation");
    m_parameters.HLS_WEBSERVER_URL_ROOT = setParameterNew<std::string>( HLS, "webserver_root", string("http://lenin:8444") );
    m_parameters.HLS_WEBSERVER_DIR_ROOT = setParameterNew<std::string>( HLS, "webserver_dir", string("/srv/video_server_archives/") );
    m_parameters.HLS_CHUNK_PATH_TEMPLATE = setParameterNew<std::string>( HLS, "chunk_path_template", string("frag_%05d.ts") );
    m_parameters.HLS_PLAYLIST_NAME = setParameterNew<std::string>( HLS, "playlist_name", string("list.m3u8") );
    m_parameters.HLS_MAX_FILE_COUNT = setParameterNew<int32_t>( HLS, "max_file_count", 500 );


    if( ! common_utils::isDirectory(m_parameters.HLS_WEBSERVER_DIR_ROOT) ){
        PRELOG_ERR << ::PRINT_HEADER << " cannot open webserver dir [" << m_parameters.HLS_WEBSERVER_DIR_ROOT << "]" << endl;
        return false;
    }


    boost::property_tree::ptree videoRecord = config.get_child("video_record");
    m_parameters.RECORD_ENABLE = setParameterNew<bool>( videoRecord, "enable", false );
    m_parameters.RECORD_ENABLE_STORE_POLICY = setParameterNew<bool>( videoRecord, "enable_store_policy", false );
    m_parameters.RECORD_METADATA_UPDATE_INTERVAL_SEC = setParameterNew<int32_t>( videoRecord, "metadata_update_interval_sec", 20 );
    m_parameters.RECORD_DATA_OUTDATE_AFTER_HOUR = setParameterNew<int32_t>( videoRecord, "data_outdate_after_hour", 2 );
    const int32_t newDataBlockIntervalHour = setParameterNew<int32_t>( videoRecord, "new_data_block_interval_hour", 1 );
    const int32_t newDataBlockIntervalMin = setParameterNew<int32_t>( videoRecord, "new_data_block_interval_min", 0 );
    m_parameters.RECORD_NEW_DATA_BLOCK_INTERVAL_MIN = newDataBlockIntervalMin + ( newDataBlockIntervalHour * 60 );
    m_parameters.RECORD_ENABLE_RETRANSLATION = setParameterNew<bool>( videoRecord, "enable_retranslation", false );
    m_parameters.RECORD_ENABLE_REMOTE_ARCHIVER = setParameterNew<bool>( videoRecord, "enable_remote_archiver", false );


    boost::property_tree::ptree videoProcessing = config.get_child("video_processing");
    m_parameters.PROCESSING_ENABLE_IMAGE_ATTRS = setParameterNew<bool>( videoProcessing, "enable_image_attributes", false );
    m_parameters.PROCESSING_ENABLE_VIDEO_ATTRS = setParameterNew<bool>( videoProcessing, "enable_video_attributes", false );
    m_parameters.PROCESSING_ENABLE_RECTS_ON_TEST_SCREEN = setParameterNew<bool>( videoProcessing, "enable_rects_on_test_screen", false );
    m_parameters.PROCESSING_MAX_PROCESSINGS_PER_VIDEOCARD = setParameterNew<int32_t>( videoProcessing, "max_processings_per_videocard", 4 );
    m_parameters.PROCESSING_ENABLE_OPTIC_STREAM = setParameterNew<bool>( videoProcessing, "enable_optic_stream", false );
    m_parameters.PROCESSING_MAX_OBJECTS_FOR_BEST_IMAGE_TRACKING = setParameterNew<int32_t>( videoRecord, "max_objects_for_best_image_tracking", 50 );
    m_parameters.PROCESSING_ENABLE_ABSENT_STATE_AT_OBJECT_DISAPPEAR = setParameterNew<bool>( videoProcessing, "enable_absent_state_at_object_disappear", false );
    m_parameters.PROCESSING_RETRANSLATE_EVENTS = setParameterNew<bool>( videoProcessing, "retranslate_events", false );
    m_parameters.PROCESSING_STORE_EVENTS = setParameterNew<bool>( videoProcessing, "store_events", false );
    m_parameters.PROCESSING_FACE_ANALYZER_MAX_DISTANCE = setParameterNew<float>( videoProcessing, "face_analyzer_max_distance", 1.0 );

    return true;
}

bool ConfigReader::createCommandsFromConfig( const std::string & _content ){

    // parse whole content
    boost::property_tree::ptree config;

    istringstream contentStream( _content );
    try{
        boost::property_tree::json_parser::read_json( contentStream, config );
    }
    catch( boost::property_tree::json_parser::json_parser_error & _ex ){
        PRELOG_ERR << ::PRINT_HEADER << " parse failed of [" << _content << "]" << endl
             << "Reason: [" << _ex.what() << "]" << endl;
        return false;
    }

    // create commands from commands section
    boost::property_tree::ptree commands = config.get_child("commands");


    boost::property_tree::ptree initialSources = commands.get_child("retranslate");
    for( auto iter = initialSources.begin(); iter != initialSources.end(); ++iter ){
        boost::property_tree::ptree arrElement = iter->second;

        if( arrElement.get<bool>("enable") ){

            if( arrElement.get<std::string>("url").find("rtsp://") == string::npos &&
                arrElement.get<std::string>("url").find("http://") == string::npos ){
                PRELOG_ERR << "ConfigReader: unknown protocol [" << arrElement.get<std::string>("url") << "] ( availalbe: rtsp, http ) " << endl;
                return false;
            }

            if( arrElement.get<std::string>("output_encoding").find("h264") == string::npos &&
                arrElement.get<std::string>("output_encoding").find("jpeg") == string::npos ){
                PRELOG_ERR << ::PRINT_HEADER << " unknown encoding [" << arrElement.get<std::string>("output_encoding") << "] ( available: h264, jpeg )" << endl;
                return false;
            }

            ostringstream oss;
            boost::property_tree::json_parser::write_json( oss, arrElement );
            const string connectionParams = oss.str();

            RequestFromConfigPtr request = std::make_shared<RequestFromConfig>();
            request->m_incomingMessage = AConfigReader::m_commandConvertor->getCommandFromConfigFile( connectionParams );
            m_parameters.baseParams.INITIAL_REQUESTS.push_back( request );
        }
    }


    boost::property_tree::ptree initialAnalyzes = commands.get_child("analyze");
    for( auto iter = initialAnalyzes.begin(); iter != initialAnalyzes.end(); ++iter ){
        boost::property_tree::ptree arrElement = iter->second;

        if( arrElement.get<bool>("enable") ){
            const TSensorId sensorId = arrElement.get<int64_t>("sensor_id");
            const TProfileId profileId = arrElement.get<int64_t>("profile_id");

            ostringstream oss;
            boost::property_tree::json_parser::write_json( oss, arrElement );
            const string analyzeParams = oss.str();

            RequestFromConfigPtr request = std::make_shared<RequestFromConfig>();
            request->m_incomingMessage = AConfigReader::m_commandConvertor->getCommandFromConfigFile( analyzeParams );
            m_parameters.baseParams.INITIAL_REQUESTS.push_back( request );
        }
    }

    boost::property_tree::ptree initialPerformanceHall = commands.get_child("archiving");
    for( auto iter = initialPerformanceHall.begin(); iter != initialPerformanceHall.end(); ++iter ){
        boost::property_tree::ptree arrElement = iter->second;

        if( arrElement.get<bool>("enable") ){
            const TSensorId sensorId = arrElement.get<int64_t>("sensor_id");

            ostringstream oss;
            boost::property_tree::json_parser::write_json( oss, arrElement );
            const string archivingParams = oss.str();

            RequestFromConfigPtr request = std::make_shared<RequestFromConfig>();
            request->m_incomingMessage = AConfigReader::m_commandConvertor->getCommandFromConfigFile( archivingParams );
            m_parameters.baseParams.INITIAL_REQUESTS.push_back( request );
        }
    }

    return true;
}

std::string ConfigReader::getConfigExampleDerive(){

    assert( false && "TODO: do" );
    return std::string();
}
