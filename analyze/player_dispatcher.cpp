
#include <microservice_common/system/logger.h>

#include "common/common_utils.h"
#include "player_mirror.h"
#include "player_dispatcher.h"

using namespace std;
using namespace common_types;

static constexpr const char * PRINT_HEADER = "PlayerDispatch:";

PlayerDispatcher::PlayerDispatcher()
{

}

bool PlayerDispatcher::init( SInitSettings _settings ){

    m_settings = _settings;

    return true;
}

IObjectsPlayer * PlayerDispatcher::getInstance( TContextId _ctxId ){

    auto iter = m_players.find( _ctxId );
    if( iter != m_players.end() ){
        SPlayerDescriptor & _traversedPlayer = iter->second;

        if( _traversedPlayer.alive ){
            _traversedPlayer.useCounter++;
            return _traversedPlayer.player;
        }
        else{
            VS_LOG_ERROR << PRINT_HEADER << " player for ctx " << _ctxId << " is not alive" << endl;
            return nullptr;
        }
    }
    else{
        VS_LOG_ERROR << PRINT_HEADER << " player for ctx " << _ctxId << " is not exist" << endl;
        return nullptr;
    }
}

void PlayerDispatcher::releaseInstance( IObjectsPlayer * & _player ){

    if( ! _player ){
        return;
    }

    for( auto & valuePair : m_players ){
        SPlayerDescriptor & _traversedPlayer = valuePair.second;

        if( _traversedPlayer.player == _player ){
            if( _traversedPlayer.useCounter > 0 ){
                _traversedPlayer.useCounter--;
                _player = nullptr;
            }
        }
    }
}

// NOTE: from player 'ping'
void PlayerDispatcher::addPlayerState( const IObjectsPlayer::SBaseState & _state ){

    // TODO: check if two player with the same context id

    auto iter = m_players.find( _state.openedContextId );
    if( iter != m_players.end() ){

        // TODO: update state




    }
    else{
        //
        const string playerName = "";

        PlayerMirror::SInitSettings settings;
        settings.network = m_settings.internalCommunicationService->getPlayerCommunicator( playerName );

        PlayerMirror * temp = new PlayerMirror();
        if( ! temp->init(settings) ){
            VS_LOG_ERROR << PRINT_HEADER << " failed to create player for ctx " << _state.openedContextId << endl;
            delete temp;
        }
        else{
            //
            SPlayerDescriptor descr;
            descr.alive = true;
            descr.lastPingMillisec = common_utils::getCurrentTimeMillisec();
            descr.player = temp;

            m_players.insert( {_state.openedContextId, descr} );
        }
    }
}




