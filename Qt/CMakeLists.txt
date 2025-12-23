cmake_minimum_required(VERSION 3.19)
project(RCcar_thread LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Mqtt)

qt_standard_project_setup()

qt_add_executable(RCcar_thread
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    drawcamera.h drawmap.h gaugewidget.h mainwindow.h mymqtt.h workers.h
    drawcamera.cpp drawmap.cpp gaugewidget.cpp mainwindow.cpp mymqtt.cpp workers.cpp
    app_types.h
)

target_link_libraries(RCcar_thread
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt6::Mqtt
)

include(GNUInstallDirs)

install(TARGETS RCcar_thread
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET RCcar_thread
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
