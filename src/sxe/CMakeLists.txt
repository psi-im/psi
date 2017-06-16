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
)

set(PLAIN_HEADERS
	sxeedit.h
	sxenewedit.h
	sxerecordedit.h
	sxeremoveedit.h
)

qt_wrap_cpp(MOC_SOURCES ${HEADERS})
add_library(sxe STATIC ${HEADERS} ${MOC_SOURCES} ${PLAIN_SOURCES} ${PLAIN_HEADERS})
target_link_libraries(sxe ${QT_LIBRARIES} ${iris_LIB} libpsi_tools widgets)
