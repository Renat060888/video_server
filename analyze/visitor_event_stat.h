#ifndef VISITOR_EVENT_STAT_H
#define VISITOR_EVENT_STAT_H

#include <assert.h>

#include <jsoncpp/json/reader.h>

#include "common/common_types.h"

class VisitorEventStat : public common_types::IEventVisitor
{
    // visitor-"owner"
    friend class AnalyticsStatisticsClient;
public:
    VisitorEventStat();

    virtual void visit( const common_types::SAnalyticEvent * _event ) override;
    virtual void visit( const common_types::SAnalyzeStateEvent * ) { assert( false && "analyze state not for statistics" ); }
    virtual void visit( const common_types::SArchiverStateEvent * ) { assert( false && "archiver state not for statistics" ); }
    virtual void visit( const common_types::SOnceMoreEvent * _event ) override;

private:
    Json::Reader m_jsonReader;
};

#endif // VISITOR_EVENT_STAT_H
