unset(MOC_SOURCES)
unset(HEADERS)
unset(PLAIN_HEADERS)
unset(FORMS)
unset(SOURCES)
unset(PLAIN_SOURCES)
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
)

set(SOURCES
	jinglertp.cpp
	avcall.cpp
	calldlg.cpp
)

set(PLAIN_SOURCES
	jinglertptasks.cpp
)

set(FORMS
	call.ui
)

qt4_wrap_ui(UI_FORMS ${FORMS})
qt_wrap_cpp(MOC_SOURCES ${SOURCES} ${HEADERS})
add_library(avcall STATIC ${SOURCES} ${HEADERS} ${MOC_SOURCES} ${UI_FORMS} ${PLAIN_SOURCES})
add_dependencies(avcall build_ui_files)
target_link_libraries(avcall ${QT_LIBRARIES} ${iris_LIB} libpsi_tools widgets)
target_include_directories(avcall PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
