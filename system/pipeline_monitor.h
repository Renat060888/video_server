#ifndef PIPELINE_MONITOR_H
#define PIPELINE_MONITOR_H

#include <string>
#include <thread>
#include <vector>

#include <gst/gst.h>

class PipelineMonitor
{
public:
    enum class EPadType {
        SINK,
        SRC,
        UNDEFINED
    };

    struct SInitSettings {
        SInitSettings()
            : listenBuffers(false)
            , listenEvents(false)
            , listenMessages(false)
            , listenQueries(false)
            , padType(EPadType::UNDEFINED)
            , element(nullptr)
        {}
        bool listenBuffers;
        bool listenEvents;
        bool listenMessages;
        bool listenQueries;

        EPadType padType;

        GstElement * element;
        std::string pipeline;
        std::vector<std::string> elementsToListen;
    };

    PipelineMonitor();
    ~PipelineMonitor();

    bool launch( SInitSettings _settings );
    void stop();

private:
    static GstPadProbeReturn callbackBufferListener( GstPad * pad, GstPadProbeInfo * info, gpointer user_data );
    static GstPadProbeReturn callbackEventListener( GstPad * pad, GstPadProbeInfo * info, gpointer user_data );
    static GstPadProbeReturn callbackQueryListener( GstPad * pad, GstPadProbeInfo * info, gpointer user_data );
    static gboolean callbackBusMessage( GstBus * bus, GstMessage * message, gpointer user_data );

    bool connectToElement( const SInitSettings & _settings, GstElement * _element );
    bool connectToPipeline( const SInitSettings & _settings, const std::string & _pipeline, const std::vector<std::string> & _elementsToListen );

    // data
    std::string m_lastError;
    SInitSettings m_settings;


    // service
    GMainLoop * m_glibMainLoop;
    std::thread * m_threadGLibMainLoop;
    GstElement * m_gstPipeline;
};

#endif // PIPELINE_MONITOR_H
