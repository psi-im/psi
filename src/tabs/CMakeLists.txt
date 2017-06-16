unset(MOC_SOURCES)
unset(HEADERS)
unset(PLAIN_HEADERS)
unset(FORMS)
unset(SOURCES)
unset(PLAIN_SOURCES)
unset(UI_FORMS)

set(HEADERS
	tabdlg.h
	tabmanager.h
	tabbablewidget.h
)

set(PLAIN_SOURCES
	tabdlg.cpp
	tabmanager.cpp
	tabbablewidget.cpp
)

qt_wrap_cpp(MOC_SOURCES ${HEADERS})
add_library(tabs STATIC  ${HEADERS} ${MOC_SOURCES} ${PLAIN_SOURCES})
target_link_libraries(tabs ${QT_LIBRARIES} ${iris_LIB} tools widgets)
target_include_directories(tabs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
