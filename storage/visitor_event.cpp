
#include "visitor_event.h"

using namespace std;
using namespace common_types;

// write
void EventWriteVisitor::visit( const SAnalyticEvent * _event ){

    m_database->writeAnalyticEvent( * _event );
    return;

    // TODO: need or not ?
    m_objrepr->writeAnalytic( * _event );
}

void EventWriteVisitor::visit( const SOnceMoreEvent * _event ){

}


// delete
void EventDeleteVisitor::visit( const SAnalyticEvent * _event ){

}

void EventDeleteVisitor::visit( const SOnceMoreEvent * _event ){

}
