#ifndef SYNC_NOTIFIER_H
#define SYNC_NOTIFIER_H

#include "common/common_types.h"

class SyncNotifier : public common_types::IEventNotifier
        , public common_types::IEventVisitor
        , public common_types::IAccumulatedEventsProvider
{
public:
    SyncNotifier();

    virtual void sendEvent( const common_types::SEvent & _event ) override;


private:
    virtual void visit( const common_types::SAnalyticEvent * _event ) override;
    virtual void visit( const common_types::SAnalyzeStateEvent * _event ) override;
    virtual void visit( const common_types::SArchiverStateEvent * _event ) override;
    virtual void visit( const common_types::SOnceMoreEvent * _event ) override;

    virtual void read( std::vector<common_types::SAnalyticEvent> & _events ) override;
    virtual void read( std::vector<common_types::SAnalyzeStateEvent> & _events ) override;
    virtual void read( std::vector<common_types::SArchiverStateEvent> & _events ) override;

    // data
    std::vector<common_types::SAnalyticEvent> m_analyticEvents;
    std::vector<common_types::SAnalyzeStateEvent> m_analyzerStateEvents;
    std::vector<common_types::SArchiverStateEvent> m_archiverStateEvents;
};

#endif // SYNC_NOTIFIER_H
