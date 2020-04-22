
#include <microservice_common/system/logger.h>

#include "storage_engine_facade.h"
#include "visitor_internal_event.h"

using namespace std;
using namespace common_types;

VisitorInternalEvent::VisitorInternalEvent( StorageEngineFacade * _storageEngine )
    : m_storageEngine(_storageEngine)
{

}

void VisitorInternalEvent::visit( const SAnalyzedObjectEvent * _record ){

    m_storageEngine->m_muVideoAsm.lock();
    auto iter = m_storageEngine->m_videoAssemblers.find( _record->processingId );
    if( iter != m_storageEngine->m_videoAssemblers.end() ){
        PVideoAssembler assembler = iter->second;

        for( const TObjectId objId : _record->objreprObjectId ){
            assembler->run( objId );
        }
    }
    else{
        VS_LOG_WARN << " internal event from unknown processing id [" << _record->processingId << "]" << endl;
    }
    m_storageEngine->m_muVideoAsm.unlock();
}
