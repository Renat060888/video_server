
#include "analytics_statistics_client.h"

using namespace std;
using namespace common_types;

AnalyticsStatisticsClient::AnalyticsStatisticsClient()
{

}


bool AnalyticsStatisticsClient::init( SInitSettings _settings ){



    return true;
}

void AnalyticsStatisticsClient::statEvent( const SEvent & _event ){

    _event.accept( & m_visitorEventStat );
}
