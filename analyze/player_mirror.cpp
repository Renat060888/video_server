
#include <microservice_common/system/logger.h>

#include "player_mirror.h"

using namespace std;
using namespace common_types;

PlayerMirror::PlayerMirror()
{

}

bool PlayerMirror::init( const SInitSettings & _settings ){




    return true;
}

void * PlayerMirror::getState(){

    // TODO: most likely it would be necessary
}

void PlayerMirror::start(){

    // TODO: after live mode is switched off
}

void PlayerMirror::pause(){

    // TODO: after live mode is switched on
}

void PlayerMirror::stop(){
    assert( false && "mirror not intended for this operation" );
}

bool PlayerMirror::stepForward(){
    assert( false && "mirror not intended for this operation" );
}

bool PlayerMirror::stepBackward(){
    assert( false && "mirror not intended for this operation" );
}

bool PlayerMirror::setRange( const TTimeRangeMillisec & _range ){
    assert( false && "mirror not intended for this operation" );
}

void PlayerMirror::switchReverseMode( bool _reverse ){
    assert( false && "mirror not intended for this operation" );
}

void PlayerMirror::switchLoopMode( bool _loop ){
    assert( false && "mirror not intended for this operation" );
}

bool PlayerMirror::playFromPosition( int64_t _stepMillisec ){
    assert( false && "mirror not intended for this operation" );
}

bool PlayerMirror::increasePlayingSpeed(){
    assert( false && "mirror not intended for this operation" );
}

bool PlayerMirror::decreasePlayingSpeed(){
    assert( false && "mirror not intended for this operation" );
}

void PlayerMirror::normalizePlayingSpeed(){
    assert( false && "mirror not intended for this operation" );
}

bool PlayerMirror::updatePlayingData(){

    // TODO: while processing is active
}




