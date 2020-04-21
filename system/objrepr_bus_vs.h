#ifndef OBJREPR_BUS_VS_H
#define OBJREPR_BUS_VS_H

#include <microservice_common/system/objrepr_bus.h>

class ObjreprBusVS : public ObjreprBus
{
public:
    ObjreprBusVS();

    // NOTE: stuff for derived classes ( video-server-object, player-object, etc... )



private:





};
#define OBJREPR_BUS ObjreprBusVS::singleton()

#endif // OBJREPR_BUS_VS_H
