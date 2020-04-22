#ifndef SYSTEM_ENVIRONMENT_FACADE_VS_H
#define SYSTEM_ENVIRONMENT_FACADE_VS_H

#include <unordered_map>

#include <microservice_common/system/system_environment_facade.h>

class SystemEnvironmentFacadeVS : public SystemEnvironmentFacade
{
public:
    struct SInitSettings : SystemEnvironmentFacade::SInitSettings {
        SInitSettings()
            : registerInGdm(false)
        {}
        bool registerInGdm;
    };

    struct SState {
        SInitSettings settings;
        std::string lastError;
    };

    SystemEnvironmentFacadeVS();
    ~SystemEnvironmentFacadeVS();

    bool init( const SInitSettings & _settings );
    const SState & getState(){ return m_state; }

    // TODO: at the moment this class exist for only OBJREPR_BUS initialization


protected:
    virtual bool initDerive(){ return true; }


private:

    // data
    SState m_state;






};

#endif // SYSTEM_ENVIRONMENT_FACADE_VS_H
