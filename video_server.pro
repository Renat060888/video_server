ROOT_DIR=../

TEMPLATE = app
TARGET = video_server

include($${ROOT_DIR}pri/common.pri)

CONFIG += link_pkgconfig
PKGCONFIG += glib-2.0 gstreamer-1.0 gstreamer-app-1.0 opencv
CONFIG -= qt
#CONFIG += release

QMAKE_CXXFLAGS += -Wno-unused-parameter
QMAKE_CXXFLAGS += -Wno-unused-variable

# TODO: add defines to logger, system monitor, restbed webserver, database, etc...
DEFINES += \
    SWITCH_LOGGER_SIMPLE \
#    SWITCH_LOGGER_ASTRA \
#    OBJREPR_LIBRARY_EXIST \
    CURRENT_VERSION \
    SEPARATE_SINGLE_SOURCE \
    REMOTE_PROCESSING \
    SPLINES_DO_NOT_USE_GENERIC_CONTAINER \
#    ENABLE_CONF_DETECTOR_SERVICE \
#    DEBIAN_9 \
#    ASTRA_15 \

# NOTE: paths for dev environment ( all projects sources in one dir )
INCLUDEPATH += \
    /usr/include/libgtop-2.0 \
    /usr/include/libmongoc-1.0 \
    /usr/include/libbson-1.0 \
    $${ROOT_DIR}/microservice_common/ \

LIBS += \
    -lpthread \
    -lmongoc-1.0 \
    -lbson-1.0 \
    -lboost_regex \
    -lboost_system \
    -lboost_filesystem \
    -lboost_program_options \ # TODO: wtf?
    -ljsoncpp \
    -lmicroservice_common \

contains( DEFINES, OBJREPR_LIBRARY_EXIST ){
    message("connect 'unilog' and 'objrepr' libraries")
LIBS += \
    -lunilog \
    -lobjrepr
}

SOURCES += \
    main.cpp \
    communication/command_factory.cpp \
    communication/communication_gateway_facade_vs.cpp \
    communication/unified_command_convertor_vs.cpp \
    storage/archive_creator.cpp \
    storage/archive_creator_proxy.cpp \
    storage/archive_creator_remote.cpp \
    storage/database_manager.cpp \
    storage/objrepr_repository.cpp \
    storage/video_assembler.cpp \
    storage/video_recorder.cpp \
    storage/visitor_event.cpp \
    storage/visitor_internal_event.cpp \
    storage/visitor_metadata.cpp \
    system/config_reader.cpp \
    system/args_parser.cpp \
    system/system_environment_facade_vs.cpp \
    system/objrepr_bus_vs.cpp \
    system/path_locator.cpp \
    video_server.cpp \
    datasource/source_manager_facade.cpp \
    analyze/analytic_manager_facade.cpp \
    storage/storage_engine_facade.cpp \
    communication/async_notifier.cpp \
    communication/visitor_notify_event.cpp \
    communication/commands/analyze/cmd_analyze_start.cpp \
    communication/commands/analyze/cmd_analyze_status.cpp \
    communication/commands/analyze/cmd_analyze_stop.cpp \
    communication/commands/analyze/cmd_outgoing_analyze_event.cpp \
    communication/commands/service/cmd_context_close.cpp \
    communication/commands/service/cmd_context_open.cpp \
    communication/commands/service/cmd_ping.cpp \
    communication/commands/service/cmd_shutdown_server.cpp \
    communication/commands/source/cmd_source_connect.cpp \
    communication/commands/source/cmd_source_disconnect.cpp \
    communication/commands/source/cmd_sources_get.cpp \
    communication/commands/storage/cmd_archive_info.cpp \
    communication/commands/storage/cmd_archive_start.cpp \
    communication/commands/storage/cmd_archive_status.cpp \
    communication/commands/storage/cmd_archive_stop.cpp \
    communication/commands/storage/cmd_download_video.cpp \
    communication/commands/storage/cmd_video_bookmark.cpp \
    analyze/event_retranslator.cpp \
    analyze/videocard_balancer.cpp \
    communication/protocols/video_receiver_protocol.cpp \
    analyze/analytics_statistics_client.cpp \
    analyze/visitor_event_stat.cpp \
    analyze/analyzer_proxy.cpp \
    analyze/analyzer_remote.cpp \
    analyze/analyzer.cpp

HEADERS += \
    common/common_types.h \
    common/common_types_with_plugins.h \
    common/common_vars.h \
    common/common_utils.h \
    communication/command_factory.h \
    communication/communication_gateway_facade_vs.h \
    communication/unified_command_convertor_vs.h \
    storage/archive_creator.h \
    storage/archive_creator_proxy.h \
    storage/archive_creator_remote.h \
    storage/database_manager.h \
    storage/objrepr_repository.h \
    storage/video_assembler.h \
    storage/video_recorder.h \
    storage/visitor_event.h \
    storage/visitor_internal_event.h \
    storage/visitor_metadata.h \
    system/config_reader.h \
    system/args_parser.h \
    system/system_environment_facade_vs.h \
    system/objrepr_bus_vs.h \
    system/path_locator.h \
    video_server.h \
    datasource/source_manager_facade.h \
    analyze/analytic_manager_facade.h \
    storage/storage_engine_facade.h \
    communication/async_notifier.h \
    communication/visitor_notify_event.h \
    communication/commands/analyze/cmd_analyze_start.h \
    communication/commands/analyze/cmd_analyze_status.h \
    communication/commands/analyze/cmd_analyze_stop.h \
    communication/commands/analyze/cmd_outgoing_analyze_event.h \
    communication/commands/service/cmd_context_close.h \
    communication/commands/service/cmd_context_open.h \
    communication/commands/service/cmd_ping.h \
    communication/commands/service/cmd_shutdown_server.h \
    communication/commands/source/cmd_source_connect.h \
    communication/commands/source/cmd_source_disconnect.h \
    communication/commands/source/cmd_sources_get.h \
    communication/commands/storage/cmd_archive_info.h \
    communication/commands/storage/cmd_archive_start.h \
    communication/commands/storage/cmd_archive_status.h \
    communication/commands/storage/cmd_archive_stop.h \
    communication/commands/storage/cmd_download_video.h \
    communication/commands/storage/cmd_video_bookmark.h \
    analyze/event_retranslator.h \
    communication/protocols/yas/enum.h \
    communication/protocols/yas/yas_compressor.h \
    communication/protocols/yas/yas_defines.h \
    analyze/videocard_balancer.h \
    communication/protocols/info_channel_data.h \
    communication/protocols/video_receiver_protocol.h \
    communication/protocols/video_receiver_typedefs.h \
    analyze/analytics_statistics_client.h \
    analyze/visitor_event_stat.h \
    analyze/analyzer_proxy.h \
    analyze/analyzer_remote.h \
    analyze/analyzer.h
