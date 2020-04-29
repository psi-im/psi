unset(HEADERS)
unset(FORMS)
unset(SOURCES)
unset(UI_FORMS)

list(APPEND HEADERS
    psimedia.h
    psimedia_p.h
)

list(APPEND SOURCES
    psimedia.cpp
)

add_definitions( -DQT_GUI_LIB )

add_library(psimedia STATIC ${SOURCES} ${HEADERS} ${UI_FORMS})
target_link_libraries(psimedia ${QT_LIBRARIES})
target_include_directories(psimedia PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
