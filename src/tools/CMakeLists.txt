unset(HEADERS)
unset(FORMS)
unset(SOURCES)
unset(UI_FORMS)
unset(EXTRA_LDFLAGS)

list(APPEND SOURCES
    # iconset
    iconset/iconset.cpp
    iconset/anim.cpp

    # advwidget
    advwidget/advwidget.cpp

    # optionstree
    optionstree/optionstree.cpp
    optionstree/optionstreemodel.cpp
    optionstree/optionstreereader.cpp
    optionstree/optionstreewriter.cpp
    optionstree/varianttree.cpp

    # psifile
    tunecontroller/filetunecontroller.cpp
    tunecontroller/pollingtunecontroller.cpp
    tunecontroller/tunecontrollermanager.cpp
    tunecontroller/plugins/psifile/psifileplugin.cpp

    # voice recorder
    voicerecorder/voicerecorder.cpp
    voicerecorder/voicerecorder.h
)

list(APPEND HEADERS
    # iconset
    iconset/iconset.h

    # optionstree
    optionstree/optionstree.h
    optionstree/varianttree.h
    optionstree/optionstreemodel.h

    # advwidget
    advwidget/advwidget.h

    # tunecontroller
    tunecontroller/filetunecontroller.h
    tunecontroller/pollingtunecontroller.h
    tunecontroller/tunecontroller.h

    # psifile
    tunecontroller/tunecontrollermanager.h
    tunecontroller/plugins/psifile/psifileplugin.h

    # iconset
    iconset/anim.h

    # optionstree
    optionstree/optionstreereader.h
    optionstree/optionstreewriter.h

    # tunecontroller
    tunecontroller/tune.h
    tunecontroller/tunecontrollerinterface.h
    tunecontroller/tunecontrollerplugin.h
)

if(USE_CRASH)
    list(APPEND HEADERS
        crash/crash.h
    )

    if(LINUX)
        list(APPEND SOURCES
            crash/crash_sigsegv.cpp
            crash/crash.cpp
        )
        list(APPEND HEADERS
            crash/crash_sigsegv.h
        )
    endif()
    if(WIN32)
        list(APPEND SOURCES
            crash/crash_dummy.cpp
        )
    endif()
endif()

include_directories(tunecontroller)

add_definitions(-DTC_PSIFILE)
include_directories(
    tunecontroller/plugins/psifile
)

if(APPLE)
    list(APPEND EXTRA_LDFLAGS "-framework CoreFoundation")
    include_directories(tunecontroller/plugins/itunes)
    add_definitions(-DTC_ITUNES)
    list(APPEND SOURCES
        tunecontroller/itunestunecontroller.cpp
        tunecontroller/plugins/itunes/itunesplugin.cpp
    )

    list(APPEND HEADERS
        tunecontroller/itunestunecontroller.h
    )
elseif(WIN32)
    list(APPEND EXTRA_LDFLAGS user32)
    include_directories(
        tunecontroller/plugins/winamp
        tunecontroller/plugins/aimp
    )
    add_definitions(-DTC_WINAMP -DTC_AIMP)
    list(APPEND SOURCES
        tunecontroller/plugins/aimp/aimpplugin.cpp

        tunecontroller/aimptunecontroller.cpp
        tunecontroller/winamptunecontroller.cpp
        tunecontroller/plugins/winamp/winampplugin.cpp
    )

    list(APPEND HEADERS
        tunecontroller/aimptunecontroller.h
        tunecontroller/winamptunecontroller.h
        tunecontroller/plugins/winamp/winampplugin.h
    )
elseif(LINUX)  
    include_directories(tunecontroller/plugins/mpris)
    add_definitions(-DTC_MPRIS)
    list(APPEND SOURCES
        tunecontroller/plugins/mpris/mprisplugin.cpp

        tunecontroller/mpristunecontroller.cpp
        tunecontroller/plugins/mpris/mprisplugin.cpp
    )

    list(APPEND HEADERS
        tunecontroller/mpristunecontroller.h
    )
endif()

add_library(tools STATIC ${SOURCES} ${HEADERS})
target_link_libraries(tools ${QT_LIBRARIES} ${iris_LIB} zip ${EXTRA_LDFLAGS})
target_include_directories(tools PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/crash
    ${CMAKE_CURRENT_SOURCE_DIR}/optionstree
    ${CMAKE_CURRENT_SOURCE_DIR}/advwidget
    ${CMAKE_CURRENT_SOURCE_DIR}/iconset
    ${CMAKE_CURRENT_SOURCE_DIR}/tunecontroller
    ${CMAKE_CURRENT_SOURCE_DIR}/voicerecorder
)
