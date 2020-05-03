
#include "sync_notifier.h"

using namespace std;

SyncNotifier::SyncNotifier()
{

}

void SyncNotifier::sendEvent( const common_types::SEvent & _event ){
    _event.accept( this );
}

void SyncNotifier::visit( const common_types::SAnalyticEvent * _event ){
    m_analyticEvents.push_back( * _event );
}

void SyncNotifier::visit( const common_types::SAnalyzeStateEvent * _event ){
    m_analyzerStateEvents.push_back( * _event );
}

void SyncNotifier::visit( const common_types::SArchiverStateEvent * _event ){
    m_archiverStateEvents.push_back( * _event );
}

void SyncNotifier::visit( const common_types::SOnceMoreEvent * _event ){

    // TODO: do
}

void SyncNotifier::read( std::vector<common_types::SAnalyticEvent> & _events ){
    _events.swap( m_analyticEvents );
}

void SyncNotifier::read( std::vector<common_types::SAnalyzeStateEvent> & _events ){
    _events.swap( m_analyzerStateEvents );
}

void SyncNotifier::read( std::vector<common_types::SArchiverStateEvent> & _events ){
    _events.swap( m_archiverStateEvents );
}
