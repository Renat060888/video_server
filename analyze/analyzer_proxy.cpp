
#include "analyzer_proxy.h"

using namespace std;
using namespace common_types;

AnalyzerProxy::AnalyzerProxy( IAnalyzer * _impl )
    : m_impl(_impl)
{

}

AnalyzerProxy::~AnalyzerProxy()
{
    delete m_impl;
    m_impl = nullptr;
}

bool AnalyzerProxy::start( SInitSettings _settings ){
   return m_impl->start(_settings);
}

void AnalyzerProxy::resume(){
    return m_impl->resume();
}

void AnalyzerProxy::pause(){
    return m_impl->pause();
}

void AnalyzerProxy::stop(){
    return m_impl->stop();
}

void AnalyzerProxy::setLivePlaying( bool _live ){
    return m_impl->setLivePlaying( _live );
}

const AnalyzerProxy::SAnalyzeStatus & AnalyzerProxy::getStatus(){
    return m_impl->getStatus();
}

const AnalyzerProxy::SInitSettings & AnalyzerProxy::getSettings(){
    return m_impl->getSettings();
}

const std::string & AnalyzerProxy::getLastError(){
    return m_impl->getLastError();
}

std::vector<SAnalyticEvent> AnalyzerProxy::getAccumulatedEvents(){
    return m_impl->getAccumulatedEvents();
}

std::vector<SAnalyticEvent> AnalyzerProxy::getAccumulatedInstantEvents(){
    return m_impl->getAccumulatedInstantEvents();
}
