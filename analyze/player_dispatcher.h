#ifndef PLAYER_DISPATCHER_H
#define PLAYER_DISPATCHER_H

#include <unordered_map>

#include <boost/signals2.hpp>

#include "common/common_types.h"

class PlayerDispatcher
{
    friend class CommandPlayerState;
public:
    struct SInitSettings {
        common_types::IInternalCommunication * internalCommunicationService;
    };

    struct SPlayerDescriptor {
        SPlayerDescriptor()
            : player(nullptr)
            , lastPingMillisec(0)
            , useCounter(0)
            , alive(false)
        {}
        common_types::IObjectsPlayer * player;
        int64_t lastPingMillisec;
        int32_t useCounter;
        bool alive;
    };

    PlayerDispatcher();

    bool init( SInitSettings _settings );
    const std::string & getLastError(){ return m_lastError; }

    common_types::IObjectsPlayer * getInstance( common_types::TContextId _ctxId );
    common_types::IObjectsPlayer * getInstance( common_types::TPid _pid );
    void releaseInstance( common_types::IObjectsPlayer * & _player );

    boost::signals2::signal<void(common_types::TPid)> m_signalPlayerAppeared;
    boost::signals2::signal<void(common_types::TPid)> m_signalPlayerDisppeared;


private:
    void addPlayerState( const common_types::IObjectsPlayer::SBaseState & _state );


    // data
    std::string m_lastError;
    std::unordered_map<common_types::TContextId, SPlayerDescriptor> m_players;
    SInitSettings m_settings;

    // service



};

#endif // PLAYER_DISPATCHER_H
