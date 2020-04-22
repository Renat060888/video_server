
#include <objrepr/reprServer.h>
#include <microservice_common/system/logger.h>

#include "system/config_reader.h"
#include "common/common_utils.h"
#include "objrepr_bus_vs.h"

using namespace std;
using namespace common_types;

static constexpr const char * PRINT_HEADER = "ObjreprBUS:";
static const std::string VIDEO_SERVER_INSTANCE_ATTR_ID = "Идентификатор";
static const std::string VIDEO_SERVER_INSTANCE_ATTR_ADDRESS = "Адрес";
static const std::string VIDEO_SERVER_DYNATTR_ONLINE = "online";

static bool setOnline( objrepr::SpatialObjectPtr _videoServerOrPlayer, bool _online ){

    //
    objrepr::DynamicAttributeMapPtr dynamicAttrs = _videoServerOrPlayer->dynamicAttrMap();
    if( ! dynamicAttrs->getHold() ){
        VS_LOG_ERROR << PRINT_HEADER << "objrepr gdm-video-server instance dynamic attr GetHold() failed"
                     << endl;
        return false;
    }

    objrepr::DynAttributePtr dynAttrOnline = dynamicAttrs->getAttr( VIDEO_SERVER_DYNATTR_ONLINE.c_str() );
    if( ! dynAttrOnline ){
        VS_LOG_ERROR << PRINT_HEADER << "objrepr gdm-video-server instance has no attr [" << VIDEO_SERVER_DYNATTR_ONLINE << "]"
                  << endl;
        return false;
    }

    objrepr::DynBooleanAttributePtr onlineAttr = boost::dynamic_pointer_cast<objrepr::DynBooleanAttribute>( dynAttrOnline );
    if( ! onlineAttr->setValue( _online ) ){
        VS_LOG_ERROR << PRINT_HEADER << "set dynamic attr [" << VIDEO_SERVER_DYNATTR_ONLINE << "] is failed"
                  << endl;
        return false;
    }

    constexpr int timeoutSec = 1;
    if( ! onlineAttr->push( timeoutSec ) ){
        VS_LOG_ERROR << PRINT_HEADER << "push async dynamic attr [" << VIDEO_SERVER_DYNATTR_ONLINE << "] is failed"
                  << endl;
        return false;
    }

//    if( ! onlineAttr->approve() ){
//        VS_LOG_ERROR << PRINT_HEADER << "approve dynamic attr [" << VIDEO_SERVER_DYNATTR_ONLINE << "]"
//                    << " is failed"
//                    << endl;
//        return false;
//    }

    return true;

    // (via PhM) set flag online
    {
        objrepr::PhysModelPtr physicModel = _videoServerOrPlayer->classinfo()->phm();
        if( ! physicModel ){
            VS_LOG_ERROR << PRINT_HEADER << "objrepr gdm-video-server instance has no physic model"
                      << endl;
            return false;
        }

        if( ! physicModel->instanceAttrMap()->getHold() ){
            VS_LOG_ERROR << PRINT_HEADER << "objrepr gdm-video-server instance physic model's get holder failed"
                      << endl;
            return false;
        }

        objrepr::DynBooleanAttributePtr onlineAttr = boost::dynamic_pointer_cast<objrepr::DynBooleanAttribute>( physicModel->instanceAttrMap()->getAttr(VIDEO_SERVER_DYNATTR_ONLINE.c_str()) );
        if( ! onlineAttr ){
            VS_LOG_ERROR << PRINT_HEADER << "objrepr gdm-video-server instance has no attr [" << VIDEO_SERVER_DYNATTR_ONLINE << "]"
                      << " in physic model"
                      << endl;
            return false;
        }

        if( ! onlineAttr->setValue( _online ) ){
            VS_LOG_ERROR << PRINT_HEADER << "set dynamic attr [" << VIDEO_SERVER_DYNATTR_ONLINE << "] is failed"
                      << endl;
            return false;
        }

        if( ! onlineAttr->approve() ){
            VS_LOG_ERROR << PRINT_HEADER << "approve dynamic attr [" << VIDEO_SERVER_DYNATTR_ONLINE << "]"
                      << " is failed"
                      << endl;
            return false;
        }

        return true;
    }
}

static objrepr::SpatialObjectPtr getVideoServersContainer( const std::string & _rootObjectName ){

    objrepr::SpatialObjectManager * objManager = objrepr::RepresentationServer::instance()->objectManager();
    objrepr::GlobalDataManager * globalManager = objrepr::RepresentationServer::instance()->globalManager();

    // find or create new root container
    objrepr::SpatialObjectPtr videoServersContainer;
    vector<objrepr::ClassinfoPtr> classes = globalManager->classinfoListByCat( "container" );
    for( objrepr::ClassinfoPtr & currentClassinfo : classes ){

        if( std::string(currentClassinfo->name()) == _rootObjectName ){

            vector<objrepr::SpatialObjectPtr> classinfoObjects =  objManager->getObjectsByClassinfo( currentClassinfo->id() );
            // use existing container
            if( ! classinfoObjects.empty() ){
                if( classinfoObjects.size() > 1 ){
                    VS_LOG_ERROR << PRINT_HEADER << "MORE THAN ONE objrepr gdm-video-objects-container instances is detected"
                              << endl;
                    return nullptr;
                }
                else{
                    videoServersContainer = classinfoObjects.front();
                }
            }
            // create new container
            else{
                videoServersContainer = objManager->addObject( currentClassinfo->id() );
                if( ! videoServersContainer ){
                    VS_LOG_CRITICAL << PRINT_HEADER << "objrepr gdm-video-objects-container instance creation failed"
                                 << endl;
                    return nullptr;
                }

                const string VIDEO_SERVER_CONTAINER_NAME = "video servers";
                videoServersContainer->setName(VIDEO_SERVER_CONTAINER_NAME.c_str());
                if( ! objManager->commitObject(videoServersContainer->id()) ){
                    VS_LOG_CRITICAL << PRINT_HEADER << "objrepr gdm-video-objects-container instance commit failed"
                                 << endl;
                    return nullptr;
                }

                VS_LOG_INFO << PRINT_HEADER << "GDM: create new objrepr gdm-video-objects-container instance, with name [" << VIDEO_SERVER_CONTAINER_NAME << "]"
                         << endl;
            }
        }
    }

    return videoServersContainer;
}

static objrepr::SpatialObjectPtr getThisVideoServerObject(  const std::string & _rootObjectName,
                                                            const std::string & _uniqueId ){

    std::string VIDEO_SERVER_INSTANCE_CLASSINFO_NAME;
    if( CONFIG_PARAMS.baseParams.SYSTEM_SERVER_FEATURES & (int16_t)EServerFeatures::ANALYZE ){
        VIDEO_SERVER_INSTANCE_CLASSINFO_NAME = "Сервер обработки видео";
    }
    else if( CONFIG_PARAMS.baseParams.SYSTEM_SERVER_FEATURES & (int16_t)EServerFeatures::ARCHIVING ){
        VIDEO_SERVER_INSTANCE_CLASSINFO_NAME = "Сервер хранения видео";
    }
    else{
        // TODO: do
    }

    // container
    const std::string VIDEO_SERVER_FOR_PROCESSING_INSTANCE_CLASSINFO_NAME = "Сервер обработки видео";

    objrepr::SpatialObjectPtr videoServersContainer = getVideoServersContainer( _rootObjectName );
    if( ! videoServersContainer ){
        VS_LOG_CRITICAL << PRINT_HEADER << "objrepr gdm-video-objects-container classinfo not found [" << _rootObjectName << "]"
                     << endl;
        return nullptr;
    }

    objrepr::SpatialObjectManager * objManager = objrepr::RepresentationServer::instance()->objectManager();
    objrepr::GlobalDataManager * globalManager = objrepr::RepresentationServer::instance()->globalManager();

    // classinfo for video-server-instance
    objrepr::ClassinfoPtr videoServerClassinfo;
    vector<objrepr::ClassinfoPtr> classes = globalManager->classinfoListByCat( "container" );
    for( objrepr::ClassinfoPtr & currentClassinfo : classes ){
        if( std::string(currentClassinfo->name()) == VIDEO_SERVER_FOR_PROCESSING_INSTANCE_CLASSINFO_NAME ){
            videoServerClassinfo = currentClassinfo;
        }
    }

    if( ! videoServerClassinfo ){
        VS_LOG_CRITICAL << PRINT_HEADER << "objrepr gdm-video-server classinfo not found [" << VIDEO_SERVER_FOR_PROCESSING_INSTANCE_CLASSINFO_NAME << "]"
                     << endl;
        return nullptr;
    }

    // find youself
    objrepr::SpatialObjectPtr thisVideoServer;
    const std::vector<objrepr::SpatialObjectPtr> videoServers = objManager->getObjectsByParent( videoServersContainer->id() );
    for( objrepr::SpatialObjectPtr object : videoServers ){
        if( std::string(object->classinfo()->name()) == videoServerClassinfo->name() ){

            objrepr::StringAttributePtr idAttr = boost::dynamic_pointer_cast<objrepr::StringAttribute>( object->attrMap()->getAttr(VIDEO_SERVER_INSTANCE_ATTR_ID.c_str()) );
            if( ! idAttr ){
                VS_LOG_CRITICAL << PRINT_HEADER << "objrepr gdm-video-server instance attr not found [" << VIDEO_SERVER_INSTANCE_ATTR_ID << "]"
                             << endl;
                return nullptr;
            }

            if( idAttr->valueAsString() == _uniqueId ){
                thisVideoServer = object;

                objrepr::StringAttributePtr addressAttr = boost::dynamic_pointer_cast<objrepr::StringAttribute>( thisVideoServer->attrMap()->getAttr(VIDEO_SERVER_INSTANCE_ATTR_ADDRESS.c_str()) );
                if( ! addressAttr ){
                    VS_LOG_CRITICAL << PRINT_HEADER << "objrepr gdm-video-server instance attr not found [" << VIDEO_SERVER_INSTANCE_ATTR_ADDRESS << "]"
                                 << endl;
                    return nullptr;
                }

                VS_LOG_INFO << PRINT_HEADER << "GDM: this video server instance found, name [" << thisVideoServer->name()
                         << "] id attr [" << idAttr->valueAsString()
                         << "] address attr [" << addressAttr->valueAsString() << "]"
                         << endl;
                break;
            }
        }
    }

    if( thisVideoServer ){
        return thisVideoServer;
    }

    // or create new
    thisVideoServer = objManager->addObject( videoServerClassinfo->id(), videoServersContainer->id() );
    if( ! thisVideoServer ){
        VS_LOG_ERROR << PRINT_HEADER << "objrepr gdm-video-server instance creation failed"
                  << endl;
        return nullptr;
    }

    thisVideoServer->setName( _uniqueId.c_str() );

    // set UniqueId new instance
    objrepr::StringAttributePtr idAttr = boost::dynamic_pointer_cast<objrepr::StringAttribute>( thisVideoServer->attrMap()->getAttr(VIDEO_SERVER_INSTANCE_ATTR_ID.c_str()) );
    if( ! idAttr ){
        VS_LOG_CRITICAL << PRINT_HEADER << "objrepr gdm-video-server instance attr not found [" << VIDEO_SERVER_INSTANCE_ATTR_ID << "]"
                     << endl;
        return nullptr;
    }

    idAttr->setValue( _uniqueId.c_str() );

    if( ! objManager->commitObject(thisVideoServer->id()) ){
        VS_LOG_ERROR << PRINT_HEADER << "objrepr gdm-video-server instance commit failed"
                  << endl;
        return nullptr;
    }

    VS_LOG_INFO << PRINT_HEADER << "GDM: create NEW video server instance, name [" << thisVideoServer->name()
             << "] id [" << idAttr->valueAsString() << "]"
             << endl;

    return thisVideoServer;
}

ObjreprBusVS::ObjreprBusVS()
{

}

ObjreprBusVS::~ObjreprBusVS()
{

}

bool ObjreprBusVS::registerVideoServerInGdm( const std::string & _rootObjectName, const std::string & _uniqueId ){

    objrepr::SpatialObjectManager * objManager = objrepr::RepresentationServer::instance()->objectManager();

    objrepr::SpatialObjectPtr thisVideoServer = getThisVideoServerObject( _rootObjectName, _uniqueId );
    if( ! thisVideoServer ){
        VS_LOG_CRITICAL << PRINT_HEADER << " gdm-video-server instance getting failed by root object name [" << _rootObjectName << "]"
                     << " and unique id [" << _uniqueId << "]"
                     << endl;
        return false;
    }

    // update attrs ( except unique id )
    objrepr::StringAttributePtr addressAttr = boost::dynamic_pointer_cast<objrepr::StringAttribute>( thisVideoServer->attrMap()->getAttr(VIDEO_SERVER_INSTANCE_ATTR_ADDRESS.c_str()) );
    if( ! addressAttr ){
        VS_LOG_CRITICAL << PRINT_HEADER << " gdm-video-server instance attr not found: " << VIDEO_SERVER_INSTANCE_ATTR_ADDRESS
                     << endl;
        return false;
    }

    const string address = common_utils::getIpAddressStr() + ":" + std::to_string(CONFIG_PARAMS.baseParams.COMMUNICATION_WEBSOCKET_SERVER_PORT);
    addressAttr->setValue( address.c_str() );

    // commit
    if( ! objManager->commitObject(thisVideoServer->id()) ){
        VS_LOG_ERROR << PRINT_HEADER << " gdm-video-server instance commit failed"
                  << endl;
        return false;
    }

    // this server is online
    setOnline( thisVideoServer, true );

    m_videoServerMirrorName = thisVideoServer->name();
    m_videoServerMirrorId = thisVideoServer->id();
    return true;
}

TObjectId ObjreprBusVS::getVideoServerObjreprMirrorId(){
    return m_videoServerMirrorId;
}

std::string ObjreprBusVS::getVideoServerObjreprMirrorName(){
    return m_videoServerMirrorName;
}

std::string ObjreprBusVS::getSensorUrl( TSensorId _sensorId ){

    // TODO: check for objrepr initness

    objrepr::SpatialObjectManager * objManager = objrepr::RepresentationServer::instance()->objectManager();

    objrepr::SpatialObjectPtr cameraObj = objManager->getObject( _sensorId );
    if( ! cameraObj ){
        VS_LOG_ERROR << PRINT_HEADER << " not found objrepr object for sensor [" << _sensorId << "]" << endl;
        return string();
    }

    objrepr::SensorObjectPtr camera = boost::dynamic_pointer_cast<objrepr::SensorObject>( cameraObj );
    if( ! camera ){
        VS_LOG_ERROR << PRINT_HEADER << " failed to cast objrepr object for sensor [" << _sensorId << "]" << endl;
        return string();
    }

    return camera->dataUrl();
}



