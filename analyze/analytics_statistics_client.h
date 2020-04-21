#ifndef ANALYTICS_STATISTICS_CLIENT_H
#define ANALYTICS_STATISTICS_CLIENT_H

#include "visitor_event_stat.h"
#include "common/common_types.h"

class AnalyticsStatisticsClient
{
public:
    struct SInitSettings {
        SInitSettings()
        {}
    };

    AnalyticsStatisticsClient();

    bool init( SInitSettings _settings );

    void statEvent( const SEvent & _event );

private:    
    // data
    SInitSettings m_settings;

    // service
    VisitorEventStat m_visitorEventStat;
};

#endif // ANALYTICS_STATISTICS_CLIENT_H
