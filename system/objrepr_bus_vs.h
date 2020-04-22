#ifndef OBJREPR_BUS_VS_H
#define OBJREPR_BUS_VS_H

#include <microservice_common/system/objrepr_bus.h>

class ObjreprBusVS : public ObjreprBus
{
public:
    static ObjreprBusVS & singleton(){
        static ObjreprBusVS instance;
        return instance;
    }

    // NOTE: stuff for derived classes ( video-server-object, player-object, etc... )

    bool registerVideoServerInGdm( const std::string & _rootObjectName, const std::string & _uniqueId );

    std::string getVideoServerObjreprMirrorName();
    common_types::TObjectId getVideoServerObjreprMirrorId();
    std::string getSensorUrl( common_types::TSensorId _sensorId );


private:
    ObjreprBusVS();
    ~ObjreprBusVS();

    // data
    std::string m_videoServerMirrorName; // TODO: сделать более элегантно, без сохранения состояния
    common_types::TObjectId m_videoServerMirrorId;



};
#define OBJREPR_BUS ObjreprBusVS::singleton()

#endif // OBJREPR_BUS_VS_H
