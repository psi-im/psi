unset(HEADERS)
unset(FORMS)
unset(SOURCES)
unset(UI_FORMS)

if(WIN32 AND IS_PSIPLUS)
    if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/QDChecker.cpp")
        set(USE_QDCHECKER ON)
    endif()
endif()

include_directories(
    ../
)

list(APPEND HEADERS
    AutoUpdater.h
)

list(APPEND SOURCES
    AutoUpdater.cpp
)

if(APPLE AND USE_SPARKLE)
    list(APPEND HEADERS
        SparkleAutoUpdater.h
    )
    list(APPEND SOURCES
        SparkleAutoUpdater.mm
    )
    set(EXTRA_LIB
        ${SPARKLE_LIBRARY}
    )
endif()

if(USE_QDCHECKER)
    add_definitions(-DUSE_QDCHECKER)
    list(APPEND HEADERS
        QDChecker.h
    )
    list(APPEND SOURCES
        QDChecker.cpp
    )
    list(APPEND FORMS
        QDChangeLog.ui
    )
    include_directories(
        ../tools/optionstree
        ../widgets
    )
    set(EXTRA_STATIC_LIBS
        tools
        widgets
    )
endif()

qt5_wrap_ui(UI_FORMS ${FORMS})
add_library(AutoUpdater STATIC ${HEADERS} ${SOURCES} ${UI_FORMS})
if(USE_QDCHECKER)
    add_dependencies(AutoUpdater build_ui_files tools widgets)
endif()
target_link_libraries(AutoUpdater ${EXTRA_STATIC_LIBS} ${QT_LIBRARIES} ${EXTRA_LIB})
target_include_directories(AutoUpdater PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
