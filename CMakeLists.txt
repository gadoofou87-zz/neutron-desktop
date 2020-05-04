cmake_minimum_required(VERSION 3.5)

project(neutron-desktop LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(ECM REQUIRED NO_MODULE)

set(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    ${CMAKE_CURRENT_LIST_DIR}/.CMake
    ${ECM_MODULE_PATH})

find_package(CryptoPP REQUIRED)
find_package(liboqs REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Core Multimedia Network Sql Widgets)
find_package(KF5 REQUIRED COMPONENTS ConfigWidgets)

include_directories(
    ${CRYPTOPP_INCLUDE_DIRS}
    ${LIBOQS_INCLUDE_DIRS})

link_libraries(
    ${CRYPTOPP_LIBRARIES}
    ${LIBOQS_LIBRARIES}
    Qt5::Core
    Qt5::Multimedia
    Qt5::Network
    Qt5::Sql
    Qt5::Widgets
    KF5::ConfigWidgets)

add_executable(neutron-desktop
    src/main.cpp
    src/core/client.cpp
    src/core/file.cpp
    src/core/packet.cpp
    src/core/server.cpp
    src/chatbrowser.cpp
    src/connectdialog.cpp
    src/historyform.cpp
    src/mainwindow.cpp
    src/transferdialog.cpp
    resources.qrc)
