#ifndef PLAYER_MIRROR_H
#define PLAYER_MIRROR_H

#include "common/common_types.h"

class PlayerMirror : public common_types::IObjectsPlayer
{
public:
    struct SInitSettings {
        PNetworkClient network;
    };

    PlayerMirror();

    bool init( const SInitSettings & _settings );
    virtual void * getState() override;
    virtual const std::string & getLastError() override { return m_lastError; }

    virtual void start() override;
    virtual void pause() override;
    virtual void stop() override;
    virtual bool stepForward() override;
    virtual bool stepBackward() override;

    virtual bool setRange( const common_types::TTimeRangeMillisec & _range ) override;
    virtual void switchReverseMode( bool _reverse ) override;
    virtual void switchLoopMode( bool _loop ) override;
    virtual bool playFromPosition( int64_t _stepMillisec ) override;
    virtual bool updatePlayingData();

    virtual bool increasePlayingSpeed() override;
    virtual bool decreasePlayingSpeed() override;
    virtual void normalizePlayingSpeed() override;


private:

    // data
    std::string m_lastError;

};

#endif // PLAYER_MIRROR_H
