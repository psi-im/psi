unset(HEADERS)
unset(FORMS)
unset(SOURCES)
unset(UI_FORMS)
unset(EXTRA_LDFLAGS)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

if(ISDEBUG)
    option(SXE_NO_DEBUG "Add -DNDEBUG definition" ON)
    if(NOT SXE_NDEBUG)
        add_definitions(-DNDEBUG)
    endif()
endif()

set(SOURCES
    sxeedit.cpp
    sxemanager.cpp
    sxenewedit.cpp
    sxerecord.cpp
    sxerecordedit.cpp
    sxeremoveedit.cpp
    sxesession.cpp
)

set(HEADERS
    sxemanager.h
    sxerecord.h
    sxesession.h

    sxeedit.h
    sxenewedit.h
    sxerecordedit.h
    sxeremoveedit.h
)

add_library(sxe STATIC ${HEADERS} ${SOURCES})
target_link_libraries(sxe ${QT_LIBRARIES} ${iris_LIB} libpsi_tools widgets)
