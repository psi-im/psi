unset(HEADERS)
unset(FORMS)
unset(SOURCES)
unset(UI_FORMS)
unset(EXTRA_LDFLAGS)

find_package(ZLIB REQUIRED)

include_directories(${ZLIB_INCLUDE_DIRS})

find_package(MINIZIP QUIET)

set(DEPLIBS
    ${ZLIB_LIBRARIES}
    )

if(NOT MINIZIP_FOUND)
    set(USE_PSI_MINIZIP ON)
else()
    include_directories(${MINIZIP_INCLUDE_DIR})
    set(DEPLIBS
        ${DEPLIBS}
        ${MINIZIP_LIBRARY}
        )
endif()

set(PLAIN_HEADERS
    zip.h
    )

set(PLAIN_SOURCES
    zip.cpp
    )

if(USE_PSI_MINIZIP)
    set(PLAIN_SOURCES
        ${PLAIN_SOURCES}
        minizip/unzip.c
        minizip/ioapi.c
        )
    add_definitions(-DPSIMINIZIP)
endif()

add_library(zip STATIC ${PLAIN_SOURCES} ${PLAIN_HEADERS})
target_link_libraries(zip ${QT_LIBRARIES} ${DEPLIBS})
target_include_directories(zip PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} minizip ..)
