cmake_minimum_required(VERSION 3.19)
project(psi_mam_prosody_live LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(QT_DEFAULT_MAJOR_VERSION 5)
if(NOT DEFINED PSI_SOURCE_DIR)
    message(FATAL_ERROR "PSI_SOURCE_DIR is required")
endif()
find_package(Qt5 REQUIRED COMPONENTS Core Network Xml)

# Use the same post-audit Iris revision as the BASIC integration build.
# The released Psi dependency lock is deliberately not modified.
set(IRIS_VERSION 1.3 CACHE STRING "" FORCE)
set(IRIS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(IRIS_ENABLE_OMEMO OFF CACHE BOOL "" FORCE)
set(IRIS_ENABLE_JINGLE_SCTP OFF CACHE BOOL "" FORCE)
set(IRIS_BUNDLED_QCA OFF CACHE BOOL "" FORCE)
set(IRIS_SYSTEM_QCA 2 CACHE STRING "" FORCE)
set(USE_QT6 OFF CACHE BOOL "" FORCE)
set(IRIS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${PSI_SOURCE_DIR}/iris" iris)

add_executable(psi_mam_prosody_client client.cpp)
target_link_libraries(psi_mam_prosody_client PRIVATE
    Iris::Iris Qt5::Core Qt5::Network Qt5::Xml)
