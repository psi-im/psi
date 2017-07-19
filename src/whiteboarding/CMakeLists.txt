unset(MOC_SOURCES)
unset(HEADERS)
unset(PLAIN_HEADERS)
unset(FORMS)
unset(SOURCES)
unset(PLAIN_SOURCES)
unset(UI_FORMS)
unset(EXTRA_LDFLAGS)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(PLAIN_SOURCES
	wbdlg.cpp
	wbitem.cpp
	wbmanager.cpp
	wbnewimage.cpp
	wbnewitem.cpp
	wbnewpath.cpp
	wbscene.cpp
	wbwidget.cpp
)

set(HEADERS
	wbdlg.h
	wbitem.h
	wbmanager.h
	wbscene.h
	wbwidget.h
)

set(PLAIN_HEADERS
	wbnewimage.h
	wbnewitem.h
	wbnewpath.h
)

qt_wrap_cpp(MOC_SOURCES ${HEADERS})
add_library(whiteboarding STATIC ${HEADERS} ${MOC_SOURCES} ${PLAIN_SOURCES} ${PLAIN_HEADERS})
add_dependencies(whiteboarding build_ui_files)
target_link_libraries(whiteboarding ${QT_LIBRARIES} tools widgets)
