
#include "video_source_proxy.h"

using namespace std;
using namespace common_types;

VideoSourceProxy::VideoSourceProxy( IVideoSource * _impl )
    : m_impl(_impl)
{

}

VideoSourceProxy::~VideoSourceProxy()
{
    // TODO: who will be destroy a real video source ? ( now this doing by VideoSourceProxy )
    delete m_impl;
    m_impl = nullptr;
}

const VideoSourceProxy::SInitSettings & VideoSourceProxy::getSettings(){
    return m_impl->getSettings();
}

const std::string & VideoSourceProxy::getLastError(){
    return m_impl->getLastError();
}

void VideoSourceProxy::addObserver( ISourceObserver * _observer ){
    return m_impl->addObserver( _observer );
}

PNetworkClient VideoSourceProxy::getCommunicator(){
    return m_impl->getCommunicator();
}

bool VideoSourceProxy::connect( const SInitSettings & _settings ){
    return m_impl->connect(_settings);
}

void VideoSourceProxy::disconnect(){
    return m_impl->disconnect();
}

int32_t VideoSourceProxy::getReferenceCount(){
    return m_impl->getReferenceCount();
}

ISourcesProvider::SSourceParameters VideoSourceProxy::getStreamParameters(){
    return m_impl->getStreamParameters();
}
