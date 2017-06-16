unset(MOC_SOURCES)
unset(HEADERS)
unset(PLAIN_HEADERS)
unset(FORMS)
unset(SOURCES)
unset(PLAIN_SOURCES)
unset(UI_FORMS)
unset(EXTRA_LDFLAGS)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

list(APPEND HEADERS
	contactmanagerdlg.h
	contactmanagermodel.h
	contactmanagerview.h
)

list(APPEND PLAIN_SOURCES
	contactmanagerdlg.cpp
	contactmanagermodel.cpp
	contactmanagerview.cpp
)

list(APPEND FORMS
	contactmanagerdlg.ui
)

qt4_wrap_ui(UI_FORMS ${FORMS})
qt_wrap_cpp(MOC_SOURCES ${HEADERS})
add_library(contactmanager STATIC ${HEADERS} ${MOC_SOURCES} ${UI_FORMS} ${PLAIN_SOURCES})
target_link_libraries(contactmanager ${QT_LIBRARIES} widgets)
target_include_directories(contactmanager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
