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
    psimediajingle.h
    screensharecapture.h
    callview.h
)

set(SOURCES
    avcall.cpp
    avcallaudiodirection.cpp
    calldlg.cpp
    mediadevicewatcher.cpp
    psimediajingle.cpp
    screensharecapture.cpp
    callview.cpp
)

add_library(avcall STATIC ${SOURCES} ${HEADERS})
add_dependencies(avcall build_ui_files)
target_link_libraries(avcall ${QT_LIBRARIES} ${iris_LIB} tools widgets)
target_include_directories(avcall PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

if(LINUX AND USE_DBUS)
    target_compile_definitions(avcall PUBLIC PSI_SCREENSHARE_PORTAL)
    target_link_libraries(avcall Qt${QT_DEFAULT_MAJOR_VERSION}::DBus)
endif()
if(LINUX AND USE_X11)
    target_compile_definitions(avcall PRIVATE PSI_SCREENSHARE_X11)
    target_link_libraries(avcall ${X11_LIBRARIES})
    if(X11_Xrandr_LIB AND X11_Xrandr_INCLUDE_PATH)
        target_compile_definitions(avcall PRIVATE PSI_SCREENSHARE_XRANDR)
        target_link_libraries(avcall ${X11_Xrandr_LIB})
    endif()
endif()

if(BUILD_TESTING)
    add_subdirectory(unittest)
endif()
