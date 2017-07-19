cmake_minimum_required( VERSION 2.8.12 )
set_directory_properties(PROPERTIES
	INCLUDE_DIRECTORIES ""
	COMPILE_DEFINITIONS ""
)

if(USE_QT5)
	add_definitions(-DHAVE_QT5)
	set(QT5_BUILD ON)
endif()

option( BUILD_DEV_PLUGINS "Build plugins from dev directory" OFF )

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/generic")
	set(GENERIC_PLUGINS_FOUND ON)
	message(STATUS "Generic plugins found: ${CMAKE_CURRENT_LIST_DIR}")
else()
	message(FATAL_ERROR "ENABLE_PLUGINS flag is enabled but no generic plugins found at ${CMAKE_CURRENT_LIST_DIR}")
endif()
if(LINUX)
	if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/unix")
		set(UNIX_PLUGINS_FOUND ON)
		message(STATUS "Unix plugins found: ${CMAKE_CURRENT_LIST_DIR}")
	else()
		message(WARNING "ENABLE_PLUGINS flag is enabled but no unix plugins found at ${CMAKE_CURRENT_LIST_DIR}")
	endif()
endif()

if(BUILD_DEV_PLUGINS)
	if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/dev")
		set(DEV_PLUGINS_FOUND ON)
		message(STATUS "Dev plugins found: ${CMAKE_CURRENT_LIST_DIR}")
	else()
		message(WARNING "ENABLE_PLUGINS flag is enabled but no dev plugins found at ${CMAKE_CURRENT_LIST_DIR}")
	endif()
endif()

# Use the same path for shared and static plugins
if(WIN32)
	#Store old runtime output path
	set(OLD_RUNTIME_TARGETS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
	#Set runtime output path for plugins dll-files
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins")
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/")
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/")
else()
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins")
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins")
endif()

set(BUILD_PLUGINS "ALL" CACHE STRING "List of plugins to build")

if(NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	add_definitions(-DQT_NO_DEBUG )
else()
	add_definitions( -Wall )
endif()

if(NOT WIN32)
	set(LIB_SUFFIX "" CACHE STRING "Define suffix of directory name (32/64)")
	set(PLUGINS_PATH "lib${LIB_SUFFIX}/psi-plus/plugins" CACHE STRING "Install suffix for plugins")
else()
	set(PLUGINS_PATH "psi-plus/plugins" CACHE STRING "Install suffix for plugins")
endif()

if(GENERIC_PLUGINS_FOUND)
	add_subdirectory(generic)
endif()
if(LINUX AND UNIX_PLUGINS_FOUND)
	add_subdirectory(unix)
endif()
if(BUILD_DEV_PLUGINS AND DEV_PLUGINS_FOUND)
	add_subdirectory(dev)
endif()

unset(BUILD_PLUGINS CACHE)
unset(PLUGINS_PATH CACHE)
#Restore default psi-plsu runtime output path
if(WIN32)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OLD_RUNTIME_TARGETS}")
endif()
