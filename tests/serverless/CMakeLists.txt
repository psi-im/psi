find_package(Qt${QT_DEFAULT_MAJOR_VERSION} REQUIRED COMPONENTS Widgets Test)
add_library(serverless_ui_probe MODULE ui_probe.cpp)
set_target_properties(serverless_ui_probe PROPERTIES AUTOMOC ON
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/styles"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/styles")
target_link_libraries(serverless_ui_probe PRIVATE Qt${QT_DEFAULT_MAJOR_VERSION}::Widgets Qt${QT_DEFAULT_MAJOR_VERSION}::Test)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_executable(serverless_ui_peer peer.cpp)
target_link_libraries(serverless_ui_peer PRIVATE ${iris_LIB})
add_test(NAME serverless_ui COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/ui_smoke.py
    $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_CURRENT_BINARY_DIR} $<TARGET_FILE:serverless_ui_peer>)
set_tests_properties(serverless_ui PROPERTIES TIMEOUT 100)
