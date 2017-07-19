unset(MOC_SOURCES)
unset(HEADERS)
unset(PLAIN_HEADERS)
unset(FORMS)
unset(SOURCES)
unset(PLAIN_SOURCES)
unset(UI_FORMS)

list(APPEND HEADERS
	psimedia.h
)

list(APPEND PLAIN_HEADERS
	psimediaprovider.h
)

list(APPEND SOURCES
	psimedia.cpp
)

add_definitions( -DQT_GUI_LIB )

qt_wrap_cpp(MOC_SOURCES ${SOURCES} ${HEADERS})
add_library(psimedia STATIC ${SOURCES} ${HEADERS} ${MOC_SOURCES} ${UI_FORMS} ${PLAIN_SOURCES})
target_link_libraries(psimedia ${QT_LIBRARIES})
target_include_directories(psimedia PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
