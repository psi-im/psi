unset(MOC_SOURCES)
unset(HEADERS)
unset(PLAIN_HEADERS)
unset(FORMS)
unset(SOURCES)
unset(PLAIN_SOURCES)
unset(UI_FORMS)

if(WIN32 AND IS_PSIPLUS)
	set(USE_QDCHECKER ON)
endif()

include_directories(
	../
)

list(APPEND PLAIN_HEADERS
	AutoUpdater.h
)

list(APPEND PLAIN_SOURCES
	AutoUpdater.cpp
)

if(APPLE)
	list(APPEND HEADERS
		SparkleAutoUpdater.h
	)
	list(APPEND SOURCES
		SparkleAutoUpdater.mm
	)
	set(EXTRA_LIB
		${SPARKLE_LIBRARY}
	)
endif()

if(USE_QDCHECKER)
	add_definitions(-DUSE_QDCHECKER)
	list(APPEND HEADERS
		QDChecker.h
	)
	list(APPEND PLAIN_SOURCES
		QDChecker.cpp
	)
	list(APPEND FORMS
		QDChangeLog.ui
	)
	include_directories(
		../tools/optionstree
		../widgets
	)
	set(EXTRA_STATIC_LIBS
		tools
		widgets
	)
endif()

qt4_wrap_ui(UI_FORMS ${FORMS})
qt_wrap_cpp(MOC_SOURCES ${HEADERS})
add_library(AutoUpdater STATIC ${HEADERS} ${MOC_SOURCES} ${UI_FORMS} ${PLAIN_SOURCES})
if(USE_QDCHECKER)
	add_dependencies(AutoUpdater build_ui_files tools widgets)
endif()
target_link_libraries(AutoUpdater ${EXTRA_STATIC_LIBS} ${QT_LIBRARIES} ${EXTRA_LIB})
target_include_directories(AutoUpdater PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
