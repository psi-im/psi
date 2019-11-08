cmake_minimum_required(VERSION 3.1.0)
#Set automoc and autouic policy
if(POLICY CMP0071)
    cmake_policy(SET CMP0071 OLD)
    message(STATUS "CMP0071 policy set to OLD")
endif()
if(POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)
    message(STATUS "CMP0074 policy set to NEW")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "")
endif()

#Psi or Psi+ detection
set(APP_INFO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/applicationinfo.cpp")
if(EXISTS "${APP_INFO_FILE}")
    message(STATUS "${APP_INFO_FILE} - Found")
    file(STRINGS "${APP_INFO_FILE}" AINFO_LINES)
    string(REGEX MATCH "#define PROG_SNAME \"psi\\+\"" AINFO_LINE ${AINFO_LINES})
    if(AINFO_LINE)
        message(STATUS "Psi+ features - FOUND")
        set(IS_PSIPLUS ON)
        project(psi-plus)
    else()
        message(STATUS "Psi+ features - NOT FOUND")
        set(IS_PSIPLUS OFF)
        project(psi)
    endif()
endif()

#Common options
option( BUNDLED_IRIS "Build iris library bundled" ON )
option( ONLY_PLUGINS "Build psi-plus plugins only" OFF )
option( USE_HUNSPELL "Build psi-plus with hunspell spellcheck" ON )
option( USE_ENCHANT "Build psi-plus with enchant spellcheck" OFF )
option( ENABLE_PLUGINS "Enable plugins" OFF )
set( CHAT_TYPE "WEBENGINE" CACHE STRING "Type of chatlog engine. WEBKIT | WEBENGINE | BASIC")
option( USE_CCACHE "Use ccache utility if found" ON )
option( VERBOSE_PROGRAM_NAME "Verbose output binary name" OFF ) #Experimental
option( USE_CRASH "Enable builtin sigsegv handling" OFF )
option( USE_KEYCHAIN "Enable Qt5Keychain support" ON )
option( BUILD_PSIMEDIA "Build psimedia plugin if sources found" OFF )
option( ONLY_BINARY "Build and install only binary file" OFF )
option( INSTALL_EXTRA_FILES "Install sounds, iconsets, certs, client_icons.txt, themes" ON )
option( INSTALL_PLUGINS_SDK "Install sdk files to build plugins outside of project" OFF )
option( DEV_MODE "Enable prepare-bin-libs target for Windows OS only. Set PSI_DATADIR and PSI_LIBDIR to CMAKE_RUNTIME_OUTPUT_DIRECTORY to debug plugins for Linux only" OFF )
#Iris options
option( USE_QJDNS "Use qjdns/jdns library. Disabled by default for Qt5" OFF )
option( SEPARATE_QJDNS "Build qjdns as separate lib" OFF )

if( IS_PSIPLUS )
    option( PRODUCTION "Build production version" OFF )
else()
    option( PRODUCTION "Build production version" ON )
endif()
#Windows or MXE only
option( USE_MXE "Use MXE cross-compilation" OFF )
option( ENABLE_PORTABLE "Create portable version of Psi+ in win32" OFF )
#Apple only
if(APPLE)
    option( USE_SPARKLE "Use Sparkle for APPLE builds" ON )
    option( USE_GROWL "Use growl for macOS builds" OFF )
    option( USE_MAC_DOC "Use macOS dock" OFF )
endif()

if( USE_HUNSPELL AND USE_ENCHANT )
    message(FATAL_ERROR "Both flags USE_HUNSPELL and USE_ENCHANT cannot be enabled at the same time. Please disable one of them")
endif()

if( ONLY_BINARY AND INSTALL_EXTRA_FILES )
    message(FATAL_ERROR "Both flags ONLY_BINARY and INSTALL_EXTRA_FILES cannot be enabled at the same time. Please disable one of them")
endif()

set( GLOBAL_DEPENDS_DEBUG_MODE ON )

set( CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules" )

set(IS_WEBKIT OFF CACHE BOOL "Use webkit. Internal variable")
set(IS_WEBENGINE OFF CACHE BOOL "Use webengine. Internal variable")
string(TOLOWER "${CHAT_TYPE}" LCHAT_TYPE)

if("${LCHAT_TYPE}" STREQUAL "webkit")
    add_definitions(
        -DWEBKIT
    )
    set(IS_WEBKIT ON)
    message(STATUS "Chatlog type - QtWebKit")
elseif("${LCHAT_TYPE}" STREQUAL "webengine")
    set(IS_WEBENGINE ON)
    find_package( Qt5Core 5.6.0 REQUIRED )
    add_definitions(
        -DWEBKIT
        -DWEBENGINE=1
    )
    message(STATUS "Chatlog type - QtWebEngine")
else()
    set(IS_WEBKIT OFF)
    set(IS_WEBENGINE OFF)
    message(STATUS "Chatlog type - Basic")
endif()

message(STATUS "System name - ${CMAKE_SYSTEM_NAME}")

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug" OR ("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo"))
    set(ISDEBUG ON)
endif()

if(ONLY_PLUGINS)
    set(ENABLE_PLUGINS ON)
endif()

if(USE_CRASH)
    add_definitions(-DUSE_CRASH)
endif()

#Detect MXE cross-compilation
if( (CMAKE_CROSSCOMPILING) AND (DEFINED MSYS) )
    message(STATUS "MXE environment detected")
    set(USE_MXE ON)
    message(STATUS "MXE root path: ${CMAKE_PREFIX_PATH}")
    if(IS_WEBENGINE)
        message(FATAL_ERROR "Webengine is not available in MXE. Please set the CHAT_TYPE variable to Webkit or Basic")
    endif()
    if(USE_KEYCHAIN AND (EXISTS "${CMAKE_PREFIX_PATH}/lib/cmake/Qt5Keychain"))
        set(Qt5Keychain_DIR "${CMAKE_PREFIX_PATH}/lib/cmake/Qt5Keychain" CACHE STRING "Path to Qt5Keychain cmake files")
    endif()
endif()

# Define LINUX on Linux like as WIN32 on Windows and APPLE on macOS
if(UNIX AND NOT (APPLE OR HAIKU))
    set(LINUX ON)
    add_definitions(
        -DHAVE_X11
        -DHAVE_FREEDESKTOP
        -DAPP_PREFIX=${CMAKE_INSTALL_PREFIX}
        -DAPP_BIN_NAME=${PROJECT_NAME}
    )
endif()

#Detect *BSD systems
STRING (REGEX MATCH "BSD" PROJECT_OS_BSD ${CMAKE_SYSTEM_NAME})
if(PROJECT_OS_BSD)
    add_definitions(-DIOAPI_NO_64)
endif()

# Qt dependencies make building very slow
# Track only .h files
include_regular_expression("^.*\\.h$")

# Put executable in build root dir
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/psi" )

#Set CXX and C Flags
if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-overloaded-virtual")
elseif(WIN32)
    include("${CMAKE_CURRENT_SOURCE_DIR}/win32/win32_definitions.cmake")
endif()

if(ISDEBUG)
    message(STATUS "CXX debug flags: ${CMAKE_CXX_FLAGS_DEBUG}")
    message(STATUS "C debug flags: ${CMAKE_C_FLAGS_DEBUG}")
else()
    message(STATUS "CXX flags: ${CMAKE_CXX_FLAGS}")
    message(STATUS "C flags: ${CMAKE_C_FLAGS}")
endif()

if(NOT WIN32 AND ENABLE_PORTABLE)
    message(WARNING "Portable version creation possible only for Windows OS")
    set(ENABLE_PORTABLE OFF)
elseif(WIN32 AND ENABLE_PORTABLE)
    message(STATUS "Portable version - ENABLED")
endif()

if( PRODUCTION )
    message(STATUS "Production version - ENABLED")
endif()

if( DEV_MODE OR ENABLE_PORTABLE )
    message(STATUS "prepare-bin-libs target - ENABLED")
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
    add_subdirectory( 3rdparty )
    if(BUNDLED_IRIS)
        add_subdirectory( iris )
    else()
        find_package( Iris REQUIRED )
        include_directories(${Iris_INCLUDE_DIR})
    endif()
    set( iris_LIB iris )
    add_subdirectory(src)
    if(EXISTS "${PROJECT_SOURCE_DIR}/psimedia" AND BUILD_PSIMEDIA)
        if(IS_PSIPLUS)
            option(USE_PSI "use psi" OFF)
        else()
            option(USE_PSI "use psi" ON)
        endif()
        option(BUILD_DEMO "build demo" OFF)
        add_subdirectory(psimedia)
    elseif(BUILD_PSIMEDIA)
        message(FATAL_ERROR "BUILD_PSIMEDIA flag is enabled but Psimedia sources not found at ${PROJECT_SOURCE_DIR}/psimedia")
    endif()
else()
    add_subdirectory(src/plugins)
endif()
