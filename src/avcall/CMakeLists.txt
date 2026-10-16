unset(HEADERS)
unset(FORMS)
unset(SOURCES)
unset(UI_FORMS)

add_definitions(-DDEBUG_POSTFIX="")

set(HEADERS
    avcall.h
    avcallpolicy.h
    avcallaudiodirection.h
    calldlg.h
    mediadevicewatcher.h
    mediacryptocapabilities.h
    psimediajingle.h
    screensharecapture.h
    callview.h
    callstage.h
    callringer.h
)

set(SOURCES
    avcall.cpp
    avcallaudiodirection.cpp
    calldlg.cpp
    mediadevicewatcher.cpp
    mediacryptocapabilities.cpp
    psimediajingle.cpp
    screensharecapture.cpp
    callview.cpp
    callstage.cpp
    callringer.cpp
)

find_package(Qt${QT_DEFAULT_MAJOR_VERSION} REQUIRED COMPONENTS Concurrent)

add_library(avcall STATIC ${SOURCES} ${HEADERS})
add_dependencies(avcall build_ui_files)
target_link_libraries(avcall Qt${QT_DEFAULT_MAJOR_VERSION}::Concurrent ${QT_LIBRARIES} ${iris_LIB} tools widgets whiteboarding psimedia)
target_include_directories(avcall PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

if(BUILD_TESTING)
    add_subdirectory(unittest)
endif()
