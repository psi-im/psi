cmake_minimum_required(VERSION 3.10)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(psi_crypto_capabilities_regression LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(QT_DEFAULT_MAJOR_VERSION 6 CACHE STRING "Qt major version")
    enable_testing()
endif()
find_package(Qt${QT_DEFAULT_MAJOR_VERSION} REQUIRED COMPONENTS Core Concurrent Test)
add_executable(media_crypto_capabilities_test probe.cpp
    ../../../src/avcall/mediacryptocapabilities.cpp
    ../../../src/avcall/mediacryptocapabilities.h)
set_target_properties(media_crypto_capabilities_test PROPERTIES AUTOMOC ON)
target_include_directories(media_crypto_capabilities_test PRIVATE ../../../src/avcall)
target_link_libraries(media_crypto_capabilities_test PRIVATE
    Qt${QT_DEFAULT_MAJOR_VERSION}::Core Qt${QT_DEFAULT_MAJOR_VERSION}::Concurrent Qt${QT_DEFAULT_MAJOR_VERSION}::Test)
add_test(NAME media_crypto_capabilities COMMAND media_crypto_capabilities_test)
set_tests_properties(media_crypto_capabilities PROPERTIES TIMEOUT 15)
