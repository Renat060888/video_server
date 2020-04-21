
#include "analytics_statistics_client.h"

using namespace std;

AnalyticsStatisticsClient::AnalyticsStatisticsClient()
{

}


bool AnalyticsStatisticsClient::init( SInitSettings _settings ){



    return true;
}

void AnalyticsStatisticsClient::statEvent( const SEvent & _event ){

    _event.accept( & m_visitorEventStat );
}
