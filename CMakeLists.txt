cmake_minimum_required(VERSION 3.1.0)
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "")
endif()

#Common options
option( BUNDLED_IRIS "Build iris library bundled" ON )
option( ONLY_PLUGINS "Build psi-plus plugins only" OFF )
option( USE_HUNSPELL "Build psi-plus with hunspell spellcheck" ON )
option( USE_ENCHANT "Build psi-plus with enchant spellcheck" OFF )
option( USE_QT5 "Use Qt5 to build psi-plus" ON )
option( ENABLE_PLUGINS "Enable plugins" OFF )
option( ENABLE_WEBKIT "Enable webkit support" ON )
option( USE_WEBENGINE "Use WebEngine support instead of Webkit" OFF )
option( USE_CCACHE "Use ccache utility if found" ON )
option( VERBOSE_PROGRAM_NAME "Verbose output binary name" OFF ) #Experimental
#Windows or MXE only
option( USE_MXE "Use MXE cross-compilation" OFF )
option( ENABLE_PORTABLE "Create portable version of Psi+ in win32" OFF)
option( PRODUCTION "Build production version" OFF )
#Iris options
option( USE_QJDNS "Use qjdns/jdns library. Disabled by default for Qt5" OFF )
option( SEPARATE_QJDNS "Build qjdns as separate lib" OFF )


set( IS_PSIPLUS ON CACHE BOOL "Build Psi+ client")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/psiwindowheader.cpp")
	message(STATUS "Psi+ features - FOUND")
	set(IS_PSIPLUS ON)
	project(psi-plus)
else()
	message(STATUS "Psi+ features - NOT FOUND")
	set(IS_PSIPLUS OFF)
	project(psi)
endif()

set(GLOBAL_DEPENDS_DEBUG_MODE ON)

set( CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules" )

set( ENABLE_SQL OFF CACHE BOOL "Enable SQL history support")

if(ENABLE_WEBKIT)
	add_definitions(
		-DWEBKIT
	)
	if(USE_QT5)
		find_package( Qt5Core REQUIRED )
		find_package( Qt5 COMPONENTS WebEngine QUIET )
		if(${Qt5Core_VERSION} VERSION_GREATER 5.6.0)
			if( Qt5WebEngine_FOUND )
				set( USE_WEBENGINE ON )
				add_definitions(
					-DWEBENGINE=1
				)
				message(STATUS "QtWebEngine - enabled")
			else()
				set( USE_WEBENGINE OFF )
			endif()
		endif()
	endif()
	message(STATUS "Webkit - enabled")
endif()


if(EXISTS "${PROJECT_SOURCE_DIR}/src/historyimp.cpp")
	message(STATUS "SQL history - enabled")
	set(ENABLE_SQL ON)
else()
	set(ENABLE_SQL OFF)
endif()

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug" OR ("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo"))
	set(ISDEBUG ON)
endif()

if(WIN32)
	if(MSVC)
		set(BUILD_ARCH "win64" CACHE STRING "CPU architecture win32/win64")
		set(SDK_PATH "e:/build/msvc2015-root" CACHE STRING "Path to SDK")
	else()
		set(BUILD_ARCH "i386" CACHE STRING "CPU architecture i386/x86_64")
		set(SDK_PATH "e:/build/psisdk" CACHE STRING "Path to SDK")
		set(SDK_SUFFIX ${BUILD_ARCH})
		#temporary Qt5
		if(USE_QT5)
			set(SDK_PATH "${SDK_PATH}/Qt5")
			set(SDK_SUFFIX "")
		endif()
		set(LIBGCRYPT_SDK "libgcrypt")
		set(LIBGPGERROR_SDK "libgpg-error")
	endif()
	#autodetect 64bit architecture
	if(CMAKE_SIZEOF_VOID_P MATCHES "8")
		if(MSVC)
			set(BUILD_ARCH "win64")
		else()
			set(BUILD_ARCH "x86_64")
		endif()
	endif()
	message(STATUS "Detecting build architecture: ${BUILD_ARCH} detected")
	if(EXISTS "${SDK_PATH}")
		if(MSVC)
			set(QCA_DIR "${SDK_PATH}/" CACHE STRING "Path to QCA")
			set(IDN_ROOT "${SDK_PATH}/" CACHE STRING "Path to IDN library")
			set(HUNSPELL_ROOT "${SDK_PATH}/" CACHE STRING "Path to hunspell library")
			set(LIBGCRYPT_ROOT "${SDK_PATH}/" CACHE STRING "Path to libgcrypt library")
			set(LIBGPGERROR_ROOT "${SDK_PATH}/" CACHE STRING "Path to libgpg-error library")
			set(LIBOTR_ROOT "${SDK_PATH}/" CACHE STRING "Path to libotr library")
			set(LIBTIDY_ROOT "${SDK_PATH}/" CACHE STRING "Path to libtidy library")
			set(ZLIB_ROOT "${SDK_PATH}/" CACHE STRING "Path to zlib")
		else()
			set(QCA_DIR "${SDK_PATH}/qca/${SDK_SUFFIX}/" CACHE STRING "Path to QCA")
			set(IDN_ROOT "${SDK_PATH}/libidn/${SDK_SUFFIX}/" CACHE STRING "Path to IDN library")
			set(HUNSPELL_ROOT "${SDK_PATH}/hunspell/${SDK_SUFFIX}/" CACHE STRING "Path to hunspell library")
			set(LIBGCRYPT_ROOT "${SDK_PATH}/${LIBGCRYPT_SDK}/${SDK_SUFFIX}/" CACHE STRING "Path to libgcrypt library")
			set(LIBGPGERROR_ROOT "${SDK_PATH}/${LIBGPGERROR_SDK}/${SDK_SUFFIX}/" CACHE STRING "Path to libgpg-error library")
			set(LIBOTR_ROOT "${SDK_PATH}/libotr/${SDK_SUFFIX}/" CACHE STRING "Path to libotr library")
			set(LIBTIDY_ROOT "${SDK_PATH}/libtidy/${SDK_SUFFIX}/" CACHE STRING "Path to libtidy library")
			set(QJSON_ROOT "${SDK_PATH}/qjson/${SDK_SUFFIX}" CACHE STRING "Path to qjson library")
			set(ZLIB_ROOT "${SDK_PATH}/zlib/${SDK_SUFFIX}/" CACHE STRING "Path to zlib")
			set(QJDNS_DIR "${SDK_PATH}/qjdns/${SDK_SUFFIX}/" CACHE STRING "Path to qjdns")
			set(GST_SDK "${SDK_PATH}/gstbundle/${SDK_SUFFIX}/" CACHE STRING "Path to gstreamer SDK")
			set(PSIMEDIA_DIR "${SDK_PATH}/psimedia/${SDK_SUFFIX}/" CACHE STRING "Path to psimedia plugin")
		endif()
	else()
		message(WARNING "Psi SDK not found at ${SDK_PATH}. Please set SDK_PATH variable or add Psi dependencies to PATH system environmet variable")
	endif()
	set(PLUGINS_PATH "/plugins" CACHE STRING "Install suffix for plugins")
endif()

set(VER_FILE ${PROJECT_SOURCE_DIR}/version)
if(EXISTS "${VER_FILE}")
	message(STATUS "Found Psi version file: ${VER_FILE}")
	file(STRINGS "${VER_FILE}" VER_LINES)
	if(IS_PSIPLUS)
		string(REGEX MATCHALL "^([0-9]+\\.[0-9]+\\.[0-9]+)+.+Psi:([a-fA-F0-9]+)+.+Psi\\+:([a-fA-F0-9]+)+" VER_LINE_A ${VER_LINES})
		if(${CMAKE_MATCH_COUNT} EQUAL 3)
			if(CMAKE_MATCH_1)
				set(APP_VERSION ${CMAKE_MATCH_1})
			endif()
			if(CMAKE_MATCH_2)
				set(PSI_REVISION ${CMAKE_MATCH_2})
			endif()
			if(CMAKE_MATCH_3)
				set(PSI_PLUS_REVISION ${CMAKE_MATCH_3})
			endif()
		endif()
		if(APP_VERSION)
			message(STATUS "Psi version found: ${APP_VERSION}")
		elseif(NOT APP_VERSION AND (NOT PSI_VERSION) )
			message(WARNING "Psi+ version not found! 1.0-dev version will be set")
			set(APP_VERSION "1.0")
		endif()
		if(PSI_REVISION)
			message(STATUS "Psi revision found: ${PSI_REVISION}")
		endif()
		if(PSI_PLUS_REVISION)
			message(STATUS "Psi+ revision found: ${PSI_PLUS_REVISION}")
		endif()
	else()
		string(REGEX MATCH "^[0-9]+\\.[0-9]+" VER_LINE ${VER_LINES})
		if (VER_LINE)
			set(APP_VERSION ${VER_LINE})
			message(STATUS "Psi version found: ${VER_LINE}")
		elseif( NOT PSI_VERSION )
			message(WARNING "Psi version not found! 1.0-dev version will be set")
			set(APP_VERSION "1.0")
		endif()
	endif()
endif()

if( IS_PSIPLUS AND ( APP_VERSION AND ( PSI_REVISION AND PSI_PLUS_REVISION ) ) )
	set(PSIPLUS_VERSION_OBTAINED ON)
elseif( NOT IS_PSIPLUS AND APP_VERSION )
	set(PSI_VERSION_OBTAINED ON)
endif()

# Define LINUX on Linux like as WIN32 on Windows and APPLE on Mac OS X
if(UNIX AND NOT APPLE)
	set(LINUX ON)
	add_definitions(
		-DHAVE_X11
		-DHAVE_FREEDESKTOP
		-DAPP_PREFIX=${CMAKE_INSTALL_PREFIX}
		-DAPP_BIN_NAME=${PROJECT_NAME}
	)
endif()

#Detect MXE cross-compilation
if( (CMAKE_CROSSCOMPILING) AND (DEFINED MSYS) )
	message(STATUS "MXE environment detected")
	set(USE_MXE ON)
endif()

# Qt dependencies make building very slow
# Track only .h files
include_regular_expression("^.*\\.h$")

# Put executable in build root dir
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/psi" )

if(APPLE)
	# Force Qt5 build on Mac OS X. Qt4 is not supported
	set(USE_QT5 ON)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-overloaded-virtual")
else()
# Enable all warnings
	if(NOT MSVC)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
		if(NOT ISDEBUG)
			set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /LTCG")
			set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LTCG")
		endif()
		add_definitions(-DNOMINMAX)
	endif()
endif()

message(STATUS "CXX flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "C flags: ${CMAKE_C_FLAGS}")

if(ISDEBUG)
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0")
	if(MSVC)
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /INCREMENT:NO /MANIFEST:NO")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /INCREMENT:NO /MANIFEST:NO")
		set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
	endif()
	message(STATUS "CXX debug flags: ${CMAKE_CXX_FLAGS_DEBUG}")
	message(STATUS "C debug flags: ${CMAKE_C_FLAGS_DEBUG}")
endif()

if(USE_QT5)
	option( QT4_BUILD "Build with Qt4 support" OFF )
else()
	set( USE_QT5 OFF)
	option( QT4_BUILD "Build with Qt4 support" ON )
endif()

if(NOT WIN32 AND PRODUCTION)
	message(WARNING "PRODUCTION flag only for Windows OS")
	set(PRODUCTION OFF)
endif()

if(NOT WIN32 AND ENABLE_PORTABLE)
	message(WARNING "Portable version creation possible only for Windows OS")
	set(ENABLE_PORTABLE OFF)
elseif(WIN32 AND ENABLE_PORTABLE)
	set(PRODUCTION ON)
endif()

# Copy a list of files from one directory to another. Only full paths.
function(copy SOURCE DEST TARGET)
	if(EXISTS ${SOURCE})
		set(OUT_TARGET_FILE "${CMAKE_BINARY_DIR}/${TARGET}.cmake")

		string(REGEX REPLACE "\\\\+" "/" DEST "${DEST}")
		string(REGEX REPLACE "\\\\+" "/" SOURCE "${SOURCE}")

		if(NOT TARGET ${TARGET})
			file(REMOVE "${OUT_TARGET_FILE}")
			add_custom_target(${TARGET} COMMAND ${CMAKE_COMMAND} -P "${OUT_TARGET_FILE}")
		endif()

		if(IS_DIRECTORY ${SOURCE})
			# copy directory
			file(GLOB_RECURSE FILES "${SOURCE}/*")
			get_filename_component(SOURCE_DIR_NAME ${SOURCE} NAME)

			foreach(FILE ${FILES})
				file(RELATIVE_PATH REL_PATH ${SOURCE} ${FILE})
				set(REL_PATH "${SOURCE_DIR_NAME}/${REL_PATH}")
				get_filename_component(REL_PATH ${REL_PATH} DIRECTORY)
				set(DESTIN "${DEST}/${REL_PATH}")

				string(REGEX REPLACE "/+" "/" DESTIN ${DESTIN})
				string(REGEX REPLACE "/+" "/" FILE ${FILE})

				file(APPEND
					"${OUT_TARGET_FILE}"
					"file(INSTALL \"${FILE}\" DESTINATION \"${DESTIN}\" USE_SOURCE_PERMISSIONS)\n")
			endforeach()
		else()
			string(REPLACE "//" "/" DEST ${DEST})
			if(DEST MATCHES "/$")
				set(DIR "${DEST}")
				string(REGEX REPLACE "^(.+)/$" "\\1" DIR ${DIR})
			else()
				# need to copy and rename
				get_filename_component(DIR ${DEST} DIRECTORY)
				get_filename_component(FILENAME ${DEST} NAME)
				get_filename_component(SOURCE_FILENAME ${SOURCE} NAME)
			endif()
			file(APPEND
				"${OUT_TARGET_FILE}"
				"file(INSTALL \"${SOURCE}\" DESTINATION \"${DIR}\" USE_SOURCE_PERMISSIONS)\n")
			if(DEFINED FILENAME)
				file(APPEND
					"${OUT_TARGET_FILE}"
					"file(RENAME \"${DIR}/${SOURCE_FILENAME}\" \"${DIR}/${FILENAME}\")\n")
			endif()
		endif()
	endif()
endfunction()

if(USE_MXE)
	set(USE_CCACHE OFF)
endif()

if(USE_CCACHE)
	# Configure CCache if available
	find_program(CCACHE_PATH ccache DOC "Path to ccache")
	if(CCACHE_PATH)
		message(STATUS "Found ccache at ${CCACHE_PATH}")
		set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_PATH})
		set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE_PATH})
	endif()
endif()


if(NOT ONLY_PLUGINS)
	if( USE_WEBENGINE )
		add_subdirectory( 3rdparty )
	endif()
	if( ${BUNDLED_IRIS} )
		add_subdirectory( iris )
	else( ${BUNDLED_IRIS} )
		find_package( Iris REQUIRED )
		include_directories(${Iris_INCLUDE_DIR})
	endif( ${BUNDLED_IRIS} )
	set( iris_LIB iris )

	add_subdirectory(src)
else()
	set(ENABLE_PLUGINS ON)
	add_subdirectory(src/plugins)
endif()
