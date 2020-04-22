#ifndef VISITOR_EVENT_H
#define VISITOR_EVENT_H

#include "common/common_types.h"
#include "database_manager.h"
#include "objrepr_repository.h"


class EventWriteVisitor : public common_types::IEventVisitor {
    // visitor-"owner"
    friend class StorageEngineFacade;
public:
    virtual ~EventWriteVisitor(){}

    virtual void visit( const common_types::SAnalyticEvent * _event ) override;
    virtual void visit( const common_types::SAnalyzeStateEvent * ) { assert( false && "analyze state not writable as event" ); }
    virtual void visit( const common_types::SArchiverStateEvent * ) { assert( false && "archiver state not writable as event" ); }
    virtual void visit( const common_types::SOnceMoreEvent * _event ) override;

private:
    ObjreprRepository * m_objrepr;
    DatabaseManager * m_database;

};

class EventDeleteVisitor : public common_types::IEventVisitor {
    // visitor-"owner"
    friend class StorageEngineFacade;
public:
    virtual ~EventDeleteVisitor(){}

    virtual void visit( const common_types::SAnalyticEvent * _event ) override;
    virtual void visit( const common_types::SAnalyzeStateEvent * ) { assert( false && "analyze state not deletable as event" ); }
    virtual void visit( const common_types::SArchiverStateEvent * ) { assert( false && "archiver state not deletable as event" ); }
    virtual void visit( const common_types::SOnceMoreEvent * _event ) override;

private:
    ObjreprRepository * m_objrepr;
    DatabaseManager * m_database;

};

#endif // VISITOR_EVENT_H
