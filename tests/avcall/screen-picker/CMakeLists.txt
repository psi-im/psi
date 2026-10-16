cmake_minimum_required(VERSION 3.16)
project(PsiScreenPicker LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(QT_DEFAULT_MAJOR_VERSION 6 CACHE STRING "Qt major")
find_package(Qt${QT_DEFAULT_MAJOR_VERSION} REQUIRED COMPONENTS Test Widgets)
get_filename_component(PSI_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(CAPTURE "${PSI_SOURCE}/src/avcall")
enable_testing()
# Exercise the host UI against a synthetic provider. Platform enumeration and
# native capture are built and tested in PsiMedia's own platform CI.
add_executable(screen_picker_test "${CAPTURE}/unittest/screenpicker.cpp"
    "${CAPTURE}/screensharecapture.cpp" "${CAPTURE}/screensharecapture.h"
    "${PSI_SOURCE}/src/psimedia/screencapture.cpp" "${PSI_SOURCE}/src/psimedia/screencapture.h"
    provider.cpp)
target_include_directories(screen_picker_test PRIVATE "${CAPTURE}" "${PSI_SOURCE}/src/psimedia"
    "${PSI_SOURCE}/plugins/include")
target_link_libraries(screen_picker_test PRIVATE Qt${QT_DEFAULT_MAJOR_VERSION}::Test Qt${QT_DEFAULT_MAJOR_VERSION}::Widgets)
add_test(NAME screen_picker COMMAND screen_picker_test)
set_tests_properties(screen_picker PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 20)
