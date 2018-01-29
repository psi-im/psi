unset(HEADERS)
unset(PLAIN_HEADERS)
unset(PLAIN_SOURCES)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

list(APPEND PLAIN_HEADERS
    cocoacommon.h
    CocoaInitializer.h
    )

list(APPEND HEADERS
    CocoaTrayClick.h
    )

list(APPEND PLAIN_SOURCES
    cocoacommon.mm
    CocoaInitializer.mm
    CocoaTrayClick.cpp
    )

qt_wrap_cpp(MOC_SOURCES ${HEADERS})
add_library(CocoaUtilities STATIC ${HEADERS} ${MOC_SOURCES} ${PLAIN_HEADERS} ${PLAIN_SOURCES})
target_link_libraries(CocoaUtilities ${QT_LIBRARIES} widgets)
target_include_directories(CocoaUtilities PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
