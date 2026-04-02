cmake_minimum_required(VERSION 3.16)

project(BMS VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick SerialPort Sql Charts)

qt_standard_project_setup(REQUIRES 6.10)

qt_add_executable(appBMS
    main.cpp
    ads1256controller.cpp
    ads1256controller.h
    logdatabase.cpp
    logdatabase.h
    loglistmodel.cpp
    loglistmodel.h
    scalarekf.h
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
        Qt6::SerialPort
        Qt6::Sql
        Qt6::Charts
)

install(TARGETS appBMS
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
