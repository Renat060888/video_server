#ifndef VISITOR_INTERNAL_EVENT_H
#define VISITOR_INTERNAL_EVENT_H

#include "common/common_types.h"

class StorageEngineFacade;

class VisitorInternalEvent : public common_types::IEventInternalVisitor
{
public:
    VisitorInternalEvent( StorageEngineFacade * _storageEngine );

    virtual void visit( const common_types::SAnalyzedObjectEvent * _record ) override;

private:

    // data
    StorageEngineFacade * m_storageEngine;
};

#endif // VISITOR_INTERNAL_EVENT_H
