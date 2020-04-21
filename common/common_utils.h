#ifndef COMMON_UTILS_H
#define COMMON_UTILS_H

#include <microservice_common/common/ms_common_utils.h>

#include "common_types.h"

namespace common_utils {

// TODO: is it correct ?
using namespace common_types;

// --------------------------------------------------------------
// converters
// --------------------------------------------------------------
static std::string printPlayingStatus( EPlayingStatus _stat ){

    switch( _stat ){
    case EPlayingStatus::INITED : { return "INITED"; }
    case EPlayingStatus::CONTEXT_LOADED : { return "CONTEXT_LOADED"; }
    case EPlayingStatus::PLAYING : { return "PLAYING"; }
    case EPlayingStatus::ALL_STEPS_PASSED : { return "ALL_STEPS_PASSED"; }
    case EPlayingStatus::NOT_ENOUGH_STEPS : { return "NOT_ENOUGH_STEPS"; }
    case EPlayingStatus::PAUSED : { return "PAUSED"; }
    case EPlayingStatus::PLAY_FROM_POSITION : { return "PLAY_FROM_POSITION"; }
    case EPlayingStatus::STOPPED : { return "STOPPED"; }
    case EPlayingStatus::CRASHED : { return "CRASHED"; }
    case EPlayingStatus::CLOSE : { return "CLOSE"; }
    case EPlayingStatus::UNDEFINED : { return "UNDEFINED"; }
    }
    return "shit";
}

inline std::string convertAnalyzeStateToStr( EAnalyzeState _state ){

    switch( _state ){
    case EAnalyzeState::ACTIVE: return "ACTIVE";
    case EAnalyzeState::CRUSHED: return "CRUSHED";
    case EAnalyzeState::PREPARING: return "PREPARING";
    case EAnalyzeState::READY: return "READY";
    case EAnalyzeState::UNAVAILABLE: return "UNAVAILABLE";
    case EAnalyzeState::UNDEFINED: return "UNDEFINED";
    default: {
        assert( false && "unknown analyze state" );
    }
    }
}

inline std::string convertArchivingStateToStr( EArchivingState _state ){

    switch( _state ){
    case EArchivingState::ACTIVE: return "ACTIVE";
    case EArchivingState::CRUSHED: return "CRUSHED";
    case EArchivingState::PREPARING: return "PREPARING";
    case EArchivingState::READY: return "READY";
    case EArchivingState::UNAVAILABLE: return "UNAVAILABLE";
    case EArchivingState::UNDEFINED: return "UNDEFINED";
    default: {
        assert( false && "unknown archiving state" );
    }
    }
}

//inline video_server_protocol::EWho convertModuleTypeToProtobuf( ECascadeModuleRole _role ){

//    switch( _role ){
//    case ECascadeModuleRole::DETECTOR: return video_server_protocol::EWho::DETECTOR;
//    case ECascadeModuleRole::CLASSIFICATOR: return video_server_protocol::EWho::CLASSFICATOR;
//    case ECascadeModuleRole::IDENTIFICATOR: return video_server_protocol::EWho::IDENTIFICATOR;
//    case ECascadeModuleRole::REGULAR: return video_server_protocol::EWho::REGULAR;
//    default: {
//        assert( false && "unknown module role" );
//    }
//    }
//}

//inline ECascadeModuleRole convertModuleTypeFromProtobuf( video_server_protocol::EWho _role ){

//    switch( _role ){
//    case video_server_protocol::EWho::DETECTOR: return ECascadeModuleRole::DETECTOR;
//    case video_server_protocol::EWho::CLASSFICATOR: return ECascadeModuleRole::CLASSIFICATOR;
//    case video_server_protocol::EWho::IDENTIFICATOR: return ECascadeModuleRole::IDENTIFICATOR;
//    case video_server_protocol::EWho::REGULAR: return ECascadeModuleRole::REGULAR;
//    default: {
//        assert( false && "unknown module role" );
//    }
//    }
//}

inline std::string convertOutputEncodingTypeToStr( EOutputEncoding _encoding ){

    switch( _encoding ){
    case EOutputEncoding::H264: return "h264";
    case EOutputEncoding::JPEG: return "jpeg";
    default: {
        assert( false && "unknown output encoding role to str" );
    }
    }
}

inline EOutputEncoding convertOutputEncodingType( std::string _encoding ){

    if( "h264" == _encoding ){
        return EOutputEncoding::H264;
    }
    else if( "jpeg" == _encoding ){
        return EOutputEncoding::JPEG;
    }
    else{
        assert( false && "unknown output encoding role" );
    }
}

inline std::string convertSourceUrlToIntervideosinkChannel( std::string _sourceUrl ){

    _sourceUrl = "TODO: use this function or not ?";

    return std::string();
}

inline int32_t getBandwidthByResolution( int32_t _width, int32_t /*_height*/, EVideoQualitySimple _quality ){

    if( EVideoQualitySimple::HIGH == _quality ){
        switch( _width ){
        case 176 : { return 192000; }
        case 352 : { return 256000; }
        case 704 : { return 1024000; }
        case 320 : { return 256000; }
        case 640 : { return 768000; }

        case 800 : { return 1024000; }
        case 1024 : { return 4096000; }
        case 1280 : { return 4096000; }
        case 1920 : { return 16000000; }
        case 3840 : { return 32000000; }
        default : { return 0; }
        }
    }
    else if( EVideoQualitySimple::MIDDLE == _quality ){
        switch( _width ){
        case 176 : { return 128000; }
        case 352 : { return 192000; }
        case 704 : { return 768000; }
        case 320 : { return 192000; }
        case 640 : { return 512000; }

        case 800 : { return 768000; }
        case 1024 : { return 2048000; }
        case 1280 : { return 2048000; }
        case 1920 : { return 8192000; }
        case 3840 : { return 16000000; }
        default : { return 0; }
        }
    }
    else if( EVideoQualitySimple::LOW == _quality ){
        switch( _width ){
        case 176 : { return 96000; }
        case 352 : { return 128000; }
        case 704 : { return 512000; }
        case 320 : { return 128000; }
        case 640 : { return 256000; }

        case 800 : { return 512000; }
        case 1024 : { return 1024000; }
        case 1280 : { return 1024000; }
        case 1920 : { return 4096000; }
        case 3840 : { return 8192000; }
        default : { return 0; }
        }
    }
    else{
        assert( false && "incorrect quality simple type" );
    }
}

}

#endif // COMMON_UTILS_H
