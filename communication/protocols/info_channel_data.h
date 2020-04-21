#ifndef INFO_CHANNEL_DATA_H
#define INFO_CHANNEL_DATA_H

#include "yas/yas_compressor.h"
#include "yas/yas_defines.h"
#include "yas/enum.h"

namespace info_channel_data {

DECLARE_ENUM( MessageType, MT_DATA, MT_REQUEST )

struct DetectionMessage
{
    YAS_FRIEND( DetectionMessage );

    MessageType getMessageType(){ return message_type; }

protected:
    void setType( MessageType type ){ message_type = type; }
    MessageType message_type = MessageType::Undefine;
};
YAS_DECLARE_STRUCT( DetectionMessage, message_type )

//------------------------

struct DetectedObject
{
    //! Координаты объекта
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t w = 0;
    uint32_t h = 0;

    //! Скорость объекта (м/c или пикс/с)
    double speed = 0;

    //! Время, мс
    uint64_t timestamp = 0;

    //------------------

    uint32_t centerX()
    {
        return x + w/2;
    }
    uint32_t centerY()
    {
        return y + h/2;
    }
};
YAS_DECLARE_STRUCT( DetectedObject, x, y, w, h, speed, timestamp )
typedef std::vector< DetectedObject > DetectedObjects;

DECLARE_ENUM( DetectedFrameState, DFS_DETECTION_SKIP, DFS_DETECTION_APPLY, DFS_DETECTED )

struct DetectedFrame: public DetectionMessage
{
    YAS_FRIEND( DetectedFrame );

    DetectedFrame(){ setType( MessageType::MT_DATA ); }

    DetectedObjects objects; //! Список _сопровождаемых_ объектов на кадре

    uint64_t processing_index = 0; //! Индекс кадра в обработке
    //! Флаг, указывающий на то, что на этом кадре запускался детектор и/или были найдены объекты
    DetectedFrameState state = DetectedFrameState::Undefine;
    int64_t time_delta = 0; //! Время до следующего кадра, мс
    int64_t time_result_delay = 0; //! Время задержки выдачи результата, мс
};
YAS_DECLARE_STRUCT( DetectedFrame, objects, processing_index, state, time_delta, time_result_delay, message_type )

//---------------------------------------

DECLARE_ENUM( DetectionRequestType, DRT_START_SEND_INFO )
struct DetectionRequest: public DetectionMessage
{
    YAS_FRIEND( DetectionRequest );

    DetectionRequest(){ setType( MessageType::MT_REQUEST ); }

    DetectionRequestType type = DetectionRequestType::Undefine;
};
YAS_DECLARE_STRUCT( DetectionRequest, type, message_type )

}

#endif // INFO_CHANNEL_DATA_H
