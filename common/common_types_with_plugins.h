#ifndef COMMON_TYPES_WITH_PLUGINS_H
#define COMMON_TYPES_WITH_PLUGINS_H

#include <unordered_map>

#include <objrepr/spatialObject.h>

#include "common_types.h"

namespace common_types_with_plugins {

// NOTE: must be equal to GstDPFMotionDetect::SPointerContainer
enum class EAnalyzerFlags : int32_t {
    ENABLE_IMAGE_ATTR = 0x00000001,
    ENABLE_VIDEO_ATTR = 0x00000002,
    ENABLE_RECTS_ON_TEST_SCREEN = 0x00000004,
    ENABLE_OPTIC_STREAM = 0x00000008,

    ENABLE_ABSENT_STATE_AT_OBJECT_DISAPPEAR = 0x00000010,
    RESERVED4 = 0x00000020,
    RESERVED5 = 0x00000040,
    RESERVED6 = 0x00000080,

    // ...
};

struct SPointerContainer {
    SPointerContainer()
        : sensorId(0)
        , sessionNum(0)
        , processingFlags(false)
        , dpfProcessorPtr(nullptr)
        , usedVideocardIdx(0)
        , zoneX(0)
        , zoneY(0)
        , zoneW(0)
        , zoneH(0)
        , maxObjectsForBestImageTracking(0)
        , livePlaying(true)
    {}
    std::string profilePath;
    common_types::TProcessingId processingId;
    uint64_t sensorId;
    common_types::TSessionNum sessionNum;
    int32_t processingFlags;
    void * dpfProcessorPtr;
    int8_t usedVideocardIdx;
    int zoneX;
    int zoneY;
    int zoneW;
    int zoneH;
    std::map<std::string, uint64_t> allowedDpfLabelObjreprClassinfo;
    int32_t maxObjectsForBestImageTracking;
    bool livePlaying;
    std::unordered_map<common_types::TObjectId, objrepr::SpatialObjectPtr> currentObjects;

    bool operator ==( const SPointerContainer & _rhs ) const {
        return ( profilePath == _rhs.profilePath &&
                 processingId == _rhs.processingId &&
                 sensorId == _rhs.sensorId &&
                 sessionNum == _rhs.sessionNum &&
                 processingFlags == _rhs.processingFlags &&
                 dpfProcessorPtr == _rhs.dpfProcessorPtr &&
                 usedVideocardIdx == _rhs.usedVideocardIdx &&
                 zoneX == _rhs.zoneX &&
                 zoneY == _rhs.zoneY &&
                 zoneW == _rhs.zoneW &&
                 zoneH == _rhs.zoneH &&
                 allowedDpfLabelObjreprClassinfo == _rhs.allowedDpfLabelObjreprClassinfo &&
                 maxObjectsForBestImageTracking == _rhs.maxObjectsForBestImageTracking &&
                 livePlaying == _rhs.livePlaying &&
                 currentObjects == _rhs.currentObjects
               );
    }
};
// NOTE: must be equal to GstDPFMotionDetect::SPointerContainer

}

#endif // COMMON_TYPES_WITH_PLUGINS_H
