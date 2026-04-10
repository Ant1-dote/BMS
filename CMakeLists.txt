cmake_minimum_required(VERSION 3.16)

project(BMS VERSION 2.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(APP_ICON_ICO "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/bms_app.ico")
set(APP_ICON_RESOURCE_WINDOWS "")
set(APP_ICON_QRC "")

if(WIN32 AND EXISTS "${APP_ICON_ICO}")
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/app_icon.rc")
    set(APP_ICON_QRC "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/icons.qrc")
endif()

find_package(Qt6 REQUIRED COMPONENTS Quick QuickDialogs2 SerialPort Sql Charts Widgets)

qt_standard_project_setup(REQUIRES 6.10)

qt_add_executable(appBMS
    main.cpp
    ads1256controller.cpp
    ads1256controller.h
    logdatabase.cpp
    logdatabase.h
    loglistmodel.cpp
    loglistmodel.h
    samplesqlwriter.cpp
    samplesqlwriter.h
    crashlogger.cpp
    crashlogger.h
    scalarekf.h
    ${APP_ICON_RESOURCE_WINDOWS}
    ${APP_ICON_QRC}
)

qt_add_qml_module(appBMS
    URI BMS
    QML_FILES
        Main.qml
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appBMS PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appBMS
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appBMS
    PRIVATE
        Qt6::Quick
        Qt6::QuickDialogs2
        Qt6::SerialPort
        Qt6::Sql
        Qt6::Charts
        Qt6::Widgets
)

install(TARGETS appBMS
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
