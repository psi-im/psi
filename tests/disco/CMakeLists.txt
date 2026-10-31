cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(psi_discovery_regressions LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(QT_DEFAULT_MAJOR_VERSION 6 CACHE STRING "Qt major version")
    enable_testing()
    find_package(Iris CONFIG REQUIRED)
    set(iris_LIB Iris::Iris)
endif()
find_package(Qt${QT_DEFAULT_MAJOR_VERSION} REQUIRED COMPONENTS Widgets Test)
add_executable(discovery_items_test discoveryitems.cpp
    ../../src/discoitems.cpp ../../src/discoitems_p.h ../../src/tasklist.h)
set_target_properties(discovery_items_test PROPERTIES AUTOMOC ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
target_include_directories(discovery_items_test PRIVATE ../../src)
target_link_libraries(discovery_items_test PRIVATE ${iris_LIB}
    Qt${QT_DEFAULT_MAJOR_VERSION}::Widgets Qt${QT_DEFAULT_MAJOR_VERSION}::Test)
add_test(NAME discovery_items COMMAND discovery_items_test)
set_tests_properties(discovery_items PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
