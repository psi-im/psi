cmake_minimum_required(VERSION 3.19)
project(psi_jmi_live_smoke LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(QT_DEFAULT_MAJOR_VERSION 6)

if(NOT DEFINED PSI_SOURCE_DIR)
    message(FATAL_ERROR "PSI_SOURCE_DIR is required")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Network Xml)

set(IRIS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(IRIS_ENABLE_SRTP OFF CACHE BOOL "" FORCE)
set(IRIS_ENABLE_JINGLE_SCTP OFF CACHE BOOL "" FORCE)
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
