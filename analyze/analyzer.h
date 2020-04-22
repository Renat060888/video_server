#ifndef ANALYZER_H
#define ANALYZER_H

#include <string>
#include <mutex>
#include <memory>
#include <thread>

#include <gst/gst.h>
#include <gst/app/gstappsink.h>

#include "analyzer_proxy.h"

class AnalyzerLocal : public IAnalyzer
{
    friend GstFlowReturn callbackEventFromSink( GstElement * elm, char * str, gpointer _data );
    friend GstFlowReturn callbackServiceFromSink( GstElement * _element, char * _elementMessage, gpointer _data );
    friend gboolean callbackGstSourceMessage( GstBus * bus, GstMessage * message, gpointer user_data );
public:    
    AnalyzerLocal();
    ~AnalyzerLocal() override;

    bool start( SInitSettings _settings ) override;
    void resume() override;
    void pause() override;
    void stop() override;

    const SAnalyzeStatus & getStatus() override { return m_status; }
    const SInitSettings & getSettings() override { return m_settings; }
    const std::string & getLastError() override { return m_lastError; }

    std::vector<SAnalyticEvent> getAccumulatedEvents() override;
    std::vector<SAnalyticEvent> getAccumulatedInstantEvents() override;

    void setLivePlaying( bool _live ) override;

private:
    static GstFlowReturn callbackEventFromSink( GstElement * elm, char * str, gpointer _data );
    static GstFlowReturn callbackInstantEventFromSink( GstElement * elm, char * str, gpointer _data );
    static GstFlowReturn callbackServiceFromSink( GstElement * _element, char * _elementMessage, gpointer _data );
    static gboolean callbackGstSourceMessage( GstBus * bus, GstMessage * message, gpointer user_data );

    std::string definePipelineDescription( const SInitSettings & _settings );
    std::string createHeadOfLaunchString( const SInitSettings & _settings );
    std::vector<std::string> getCustomPluginsLaunchString( const std::vector<common_types::SPluginMetadata> & _pluginsMetadata );


    // data
    std::vector<SAnalyticEvent> m_accumulatedEvents;
    std::vector<SAnalyticEvent> m_accumulatedInstantEvents;
    std::string m_lastError;
    SInitSettings m_settings;
    SAnalyzeStatus m_status;

    std::string m_errorFromPlugin;
    bool m_pluginGetReadyFailed;

    // service
    GMainLoop * m_glibMainLoop;
    std::thread * m_threadGLibMainLoop;
    GstElement * m_gstPipeline;
    std::mutex m_mutexEvent;    
};
using PAnalyzerLocal = std::shared_ptr<AnalyzerLocal>;

#endif // ANALYZER_H
