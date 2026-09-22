cmake_minimum_required(VERSION 3.19)
project(psi_jingle_live_smoke LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(QT_DEFAULT_MAJOR_VERSION 6)

option(JINGLE_LIVE_ENABLE_FT "Build SCTP-enabled Jingle file-transfer live client" OFF)

if(NOT DEFINED PSI_SOURCE_DIR)
    message(FATAL_ERROR "PSI_SOURCE_DIR is required")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Network Xml)
if(DEFINED PSIMEDIA_SOURCE_DIR)
    find_package(Qt6 REQUIRED COMPONENTS Gui Widgets)
endif()

set(IRIS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(IRIS_ENABLE_JINGLE_SCTP ${JINGLE_LIVE_ENABLE_FT} CACHE BOOL "" FORCE)
set(IRIS_ENABLE_OMEMO OFF CACHE BOOL "" FORCE)
set(IRIS_BUNDLED_USRSCTP OFF CACHE BOOL "" FORCE)
set(IRIS_BUNDLED_QCA OFF CACHE BOOL "" FORCE)
set(IRIS_SYSTEM_QCA 3 CACHE STRING "" FORCE)
set(USE_QT6 ON CACHE BOOL "" FORCE)
add_subdirectory("${PSI_SOURCE_DIR}/iris" iris)

add_executable(psi_jmi_live_client jmi_client.cpp)
target_link_libraries(psi_jmi_live_client PRIVATE
    Iris::Iris
    Qt6::Core
    Qt6::Network
    Qt6::Xml
)

if(JINGLE_LIVE_ENABLE_FT)
    add_executable(psi_jingle_ft_live_client ft_client.cpp)
    target_link_libraries(psi_jingle_ft_live_client PRIVATE
        Iris::Iris
        Qt6::Core
        Qt6::Network
        Qt6::Xml
    )
endif()

if(DEFINED PSIMEDIA_SOURCE_DIR)
    set(BUILD_DEMO OFF CACHE BOOL "" FORCE)
    set(BUILD_PSIPLUGIN OFF CACHE BOOL "" FORCE)
    set(PSIMEDIA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(USE_PSI ON CACHE BOOL "" FORCE)
    set(MAIN_PROGRAM_NAME psi CACHE STRING "" FORCE)
    set(PLUGINS_ROOT_DIR "${PSI_SOURCE_DIR}/plugins" CACHE STRING "" FORCE)

    add_subdirectory("${PSIMEDIA_SOURCE_DIR}" psimedia)

    add_library(psi_jingle_live_media_adapter STATIC
        "${PSI_SOURCE_DIR}/src/psimedia/psimedia.cpp"
        "${PSI_SOURCE_DIR}/src/psimedia/psimedia.h"
        "${PSI_SOURCE_DIR}/src/psimedia/psimedia_p.h"
        "${PSI_SOURCE_DIR}/src/avcall/psimediajingle.cpp"
        "${PSI_SOURCE_DIR}/src/avcall/psimediajingle.h"
    )
    target_compile_definitions(psi_jingle_live_media_adapter PRIVATE QT_GUI_LIB)
    target_include_directories(psi_jingle_live_media_adapter PUBLIC
        "${PSI_SOURCE_DIR}/src"
        "${PSI_SOURCE_DIR}/src/psimedia"
        "${PSI_SOURCE_DIR}/src/avcall"
        "${PSI_SOURCE_DIR}/plugins/include"
    )
    target_link_libraries(psi_jingle_live_media_adapter PUBLIC
        Iris::Iris
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
    )

    add_executable(psi_jingle_call_live_client call_client.cpp)
    target_include_directories(psi_jingle_call_live_client PRIVATE
        "${PSI_SOURCE_DIR}/src"
        "${PSI_SOURCE_DIR}/src/psimedia"
        "${PSI_SOURCE_DIR}/src/avcall"
        "${PSI_SOURCE_DIR}/plugins/include"
        "${PSIMEDIA_SOURCE_DIR}/gstprovider"
    )
    target_link_libraries(psi_jingle_call_live_client PRIVATE
        psi_jingle_live_media_adapter
        gstprovidersrc
        Iris::Iris
        Qt6::Core
        Qt6::Network
        Qt6::Xml
    )
endif()
