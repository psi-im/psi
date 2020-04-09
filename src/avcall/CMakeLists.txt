unset(HEADERS)
unset(FORMS)
unset(SOURCES)
unset(UI_FORMS)

include_directories(
    ${PROJECT_SOURCE_DIR}/iris/include/iris
    ${PROJECT_SOURCE_DIR}/iris/src
)

add_definitions(-DDEBUG_POSTFIX="")

set(HEADERS
    jinglertptasks.h
    jinglertp.h
    avcall.h
    calldlg.h
    mediadevicewatcher.h
)

set(SOURCES
    jinglertp.cpp
    avcall.cpp
    calldlg.cpp

    jinglertptasks.cpp
    mediadevicewatcher.cpp
)

set(FORMS
    call.ui
)

qt5_wrap_ui(UI_FORMS ${FORMS})
add_library(avcall STATIC ${SOURCES} ${HEADERS} ${UI_FORMS})
add_dependencies(avcall build_ui_files)
target_link_libraries(avcall ${QT_LIBRARIES} ${iris_LIB} libpsi_tools widgets)
target_include_directories(avcall PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
