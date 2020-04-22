#ifndef RTSP_TESTER_H
#define RTSP_TESTER_H

#include <string>
#include <thread>

#include <gst/gst.h>

class RTSPTester
{
public:
    RTSPTester();
    ~RTSPTester();

    bool test( const std::string & _sourceUrl );
    const std::string & getLastError() { return m_lastError; }

private:
    static gboolean callback_gst_source_message( GstBus * bus, GstMessage * message, gpointer user_data );
    std::string definePipelineDescription( const std::string & _sourceUrl );

    GstElement * m_gstPipeline;
    GMainLoop * m_gLoop;
    std::thread * m_threadGstLoop;
    std::string m_lastError;
};

#endif // RTSP_TESTER_H

