unset(HEADERS)
unset(SOURCES)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

list(APPEND HEADERS
    cocoacommon.h
    CocoaInitializer.h
    CocoaTrayClick.h
    )

list(APPEND SOURCES
    cocoacommon.mm
    CocoaInitializer.mm
    CocoaTrayClick.cpp
    )

add_library(CocoaUtilities STATIC ${HEADERS} ${SOURCES})
target_link_libraries(CocoaUtilities ${QT_LIBRARIES} widgets)
target_include_directories(CocoaUtilities PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
