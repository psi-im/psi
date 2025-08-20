cmake_minimum_required(VERSION 3.10.0)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "")
endif()

if("${QT_DEFAULT_MAJOR_VERSION}" STREQUAL "")
    set(QT_DEFAULT_MAJOR_VERSION 5)
endif()

# Psi or Psi+ detection
option(PSI_PLUS "Build Psi+ client instead of Psi" OFF)
if(PSI_PLUS)
    add_definitions(-DPSI_PLUS)
    project(psi-plus)
    message(STATUS "===Building Psi+===")
else()
    project(psi)
    message(STATUS "===Building Psi===")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules" )

include(psi-cmake-functions)
include(policyRules)

# Define LINUX on Linux like and BSD as WIN32 on Windows and APPLE on macOS
if(UNIX AND (NOT (APPLE OR HAIKU) AND NOT CYGWIN))
    set(LINUX ON)
endif()

# Check for submodules
set(SBM_LIST
    ${PROJECT_SOURCE_DIR}/3rdparty/http-parser/http_parser.h
    ${PROJECT_SOURCE_DIR}/3rdparty/qhttp/qhttp.pro
    ${PROJECT_SOURCE_DIR}/3rdparty/qite/qite.pro
    ${PROJECT_SOURCE_DIR}/iris/CMakeLists.txt
)
foreach(submodule ${SBM_LIST})
    if(NOT EXISTS "${submodule}")
        message(FATAL_ERROR "Psi ${submodule} submodule not found.\nPlease run:\n====\ncd ${PROJECT_SOURCE_DIR}\ngit submodule update --init --recursive\n====\nbefore run cmake again")
    endif()
endforeach()

# Detect MXE cross-compilation
check_MXE(DEFAULT_USE_MXE)

set( DEFAULT_BUNDLED_QCA OFF )
if (APPLE OR ((MSVC OR DEFAULT_USE_MXE) OR BUNDLED_IRIS_ALL))
    set( DEFAULT_BUNDLED_QCA ON )
endif()

set( DEFAULT_BUNDLED_USRSCTP OFF )
if (APPLE OR ((MSVC OR DEFAULT_USE_MXE) OR BUNDLED_IRIS_ALL))
    set( DEFAULT_BUNDLED_USRSCTP ON )
endif()

# Common options
set( CHAT_TYPE "BASIC" CACHE STRING "Type of chatlog engine. WEBKIT | WEBENGINE | BASIC" )
set_property(CACHE CHAT_TYPE PROPERTY STRINGS WEBKIT WEBENGINE BASIC)
option( PRODUCTION "Build production version" OFF )
option( VERBOSE_PROGRAM_NAME "Verbose output binary name" OFF )
option( BUILD_PSIMEDIA "Build psimedia plugin if its code is found in sources tree. Works only with ENABLE_PLUGINS=ON" OFF )
option( ENABLE_PLUGINS "Enable plugins" OFF )
option( USE_HUNSPELL "Build psi with hunspell spellcheck" ON )
option( USE_ENCHANT "Build psi with enchant spellcheck" OFF )
option( USE_ASPELL "Build psi with aspell spellcheck" OFF )
option( USE_CCACHE "Use ccache utility if found" ON )
option( USE_CRASH "Enable builtin sigsegv handling" OFF )
option( USE_KEYCHAIN "Enable QtKeychain support" ON )
option( USE_QT6 "Enable Qt6 support" OFF )
option( ONLY_BINARY "Build and install only binary file" OFF )
option( ONLY_PLUGINS "Build psi plugins only" OFF )
option( INSTALL_EXTRA_FILES "Install sounds, iconsets, certs, client_icons.txt, themes" ON )
option( INSTALL_PLUGINS_SDK "Install sdk files to build plugins outside of project" OFF )
option( PLUGINS_NO_DEBUG "Add -DPLUGINS_NO_DEBUG definition" OFF )
# Developers options
option( DEV_MODE "Enable prepare-bin-libs target for MS Windows only. Set PSI_DATADIR and PSI_LIBDIR to CMAKE_RUNTIME_OUTPUT_DIRECTORY to debug plugins for Linux only" OFF )
# Iris options
option( BUNDLED_IRIS "Build iris library bundled" ON )
option( BUNDLED_IRIS_ALL "Build bundled iris library with bundled QCA and bundled USRSCTP" OFF)
# note Blake2b is needed only with Qt5. Qt6 has its own implementation
option( IRIS_BUNDLED_QCA "Adds: DTLS, Blake2b (needed with Qt5) and other useful for XMPP crypto-stuff" ${DEFAULT_BUNDLED_QCA} )
option( IRIS_BUNDLED_USRSCTP "Compile compatible usrsctp lib when system one is not available or uncompatible (required for p2p file transfer)" ${DEFAULT_BUNDLED_USRSCTP} )
option( BUNDLED_KEYCHAIN "Build QtKeychain library bundled" OFF )
option( USE_TASKBARNOTIFIER "Use taskbar notifications for incoming events" ON )

if (UNIX AND "${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    option( ENABLE_ASAN "Enable compilation with address sanitizer" OFF )
endif()

# Cross-compilation and native builds for MS Windows
option( ENABLE_PORTABLE "Create portable version of Psi+ for MS Windows" OFF )
option( USE_MXE "Use MXE (cross-compilation build environment for MS Windows)" ${DEFAULT_USE_MXE} )

# Other systems
if(LINUX)
    option( USE_DBUS "Enable DBUS support" ON )
    option( USE_X11 "Enable X11 features support" ON )
    option( USE_XSS "Enable Xscreensaver support" ON )
    option( LIMIT_X11_USAGE "Disable usage of X11 features which may crash program" OFF )
elseif(APPLE)
    option( USE_SPARKLE "Use Sparkle for APPLE builds" OFF )
    option( USE_GROWL "Use growl for macOS builds" OFF )
    option( USE_MAC_DOC "Use macOS dock" OFF )
endif()

if( USE_HUNSPELL AND (USE_ENCHANT AND USE_ASPELL) )
    message(FATAL_ERROR "Flags USE_HUNSPELL, USE_ASPELL and USE_ENCHANT cannot be enabled at the same time.\nPlease enable only one of them")
elseif( USE_HUNSPELL AND USE_ASPELL )
    message(FATAL_ERROR "Both flags USE_HUNSPELL and USE_ASPELL cannot be enabled at the same time.\nPlease enable only one of them")
elseif( USE_ASPELL AND USE_ENCHANT )
    message(FATAL_ERROR "Both flags USE_ASPELL and USE_ENCHANT cannot be enabled at the same time.\nPlease enable only one of them")
elseif( USE_HUNSPELL AND USE_ENCHANT )
    message(FATAL_ERROR "Both flags USE_HUNSPELL and USE_ENCHANT cannot be enabled at the same time.\nPlease enable only one of them")
endif()

if( ONLY_BINARY AND INSTALL_EXTRA_FILES )
    message(FATAL_ERROR "Both flags ONLY_BINARY and INSTALL_EXTRA_FILES cannot be enabled at the same time.\nPlease enable only one of them")
endif()

set( GLOBAL_DEPENDS_DEBUG_MODE ON )

set(IS_WEBKIT OFF CACHE INTERNAL "Use webkit. Internal variable")
set(IS_WEBENGINE OFF CACHE INTERNAL "Use webengine. Internal variable")
string(TOLOWER "${CHAT_TYPE}" LCHAT_TYPE)

if("${LCHAT_TYPE}" STREQUAL "webkit")
    set(IS_WEBKIT ON)
    message(STATUS "Chatlog type - QtWebKit")
elseif("${LCHAT_TYPE}" STREQUAL "webengine")
    set(IS_WEBENGINE ON)
    message(STATUS "Chatlog type - QtWebEngine")
else()
    set(IS_WEBKIT OFF)
    set(IS_WEBENGINE OFF)
    message(STATUS "Chatlog type - Basic")
endif()

if(USE_QT6)
    set(QT_DEFAULT_MAJOR_VERSION 6)
endif()

message(STATUS "System name - ${CMAKE_SYSTEM_NAME}")

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug" OR ("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo"))
    set(ISDEBUG ON CACHE INTERNAL "Debug on RelWithDebInfo build type enabled")
    option(PLUGIN_INSTALL_PATH_DEBUG "Add -DPLUGIN_INSTALL_PATH_DEBUG definition" OFF)
    option(CHATVIEW_CORRECTION_DEBUG "Add -DCORRECTION_DEBUG definition" OFF)
    if(PLUGIN_INSTALL_PATH_DEBUG)
        add_definitions(-DPLUGIN_INSTALL_PATH_DEBUG)
    endif()
    if(WIN32)
        option( NO_DEBUG_OPTIMIZATION "Disable optimization for debug builds" OFF )
    endif()
endif()

if(PLUGINS_NO_DEBUG)
    add_definitions(-DPLUGINS_NO_DEBUG)
endif()

if(ONLY_PLUGINS)
    set(ENABLE_PLUGINS ON)
endif()

if(ENABLE_PLUGINS AND BUILD_PSIMEDIA)
    if(PSI_PLUS)
        option(USE_PSI "use psi" OFF)
    else()
        option(USE_PSI "use psi" ON)
    endif()
    option(BUILD_DEMO "build demo" OFF)
elseif(NOT ENABLE_PLUGINS AND BUILD_PSIMEDIA)
    message(FATAL_ERROR "BUILD_PSIMEDIA flag not works without ENABLE_PLUGINS flag.\nPlease enable ENABLE_PLUGINS flag or disable BUILD_PSIMEDIA flag")
endif()

if(ENABLE_PLUGINS AND (NOT EXISTS "${PROJECT_SOURCE_DIR}/plugins/CMakeLists.txt"))
    message(FATAL_ERROR "ENABLE_PLUGINS option is enabled but no plugins sources found at ${PROJECT_SOURCE_DIR}/plugins.\nPlease download and copy plguins sources from official repository or disable this option")
endif()


if(WIN32 OR USE_MXE)
    set(BUNDLED_IRIS_ALL ON)
endif()

# For GNU/Linux and *BSD systems:
if(UNIX AND (NOT (APPLE OR HAIKU) AND NOT CYGWIN))
    if(USE_X11)
        message(STATUS "X11 features support - ENABLED")
        if(LIMIT_X11_USAGE)
            message(STATUS "Unsafe X11 features support - DISABLED")
            set(USE_XSS OFF)
            message(STATUS "Xscreensaver support - DISABLED")
        endif()
        if(USE_XSS)
            message(STATUS "Xscreensaver support - ENABLED")
        endif()
    endif()
    if(USE_DBUS)
        message(STATUS "DBus support - ENABLED")
    endif()
endif()

# Detect *BSD systems
STRING (REGEX MATCH "BSD" PROJECT_OS_BSD ${CMAKE_SYSTEM_NAME})
if(PROJECT_OS_BSD)
    message(STATUS "BSD-like system detected")
    add_definitions(-DIOAPI_NO_64)
endif()

# Detect NetBSD and handle library path accordingly
STRING (REGEX MATCH "NetBSD" PROJECT_OS_NETBSD ${CMAKE_SYSTEM_NAME})
if(PROJECT_OS_NETBSD)
    message(STATUS "NetBSD detected")
    set(CMAKE_C_FLAGS "-Wl,-rpath,/usr/X11R7/lib -Wl,-rpath,/usr/pkg/lib -Wl,-rpath,/usr/pkg/qt${QT_DEFAULT_MAJOR_VERSION}/lib")
    set(CMAKE_CXX_FLAGS "-Wl,-rpath,/usr/X11R7/lib -Wl,-rpath,/usr/pkg/lib -Wl,-rpath,/usr/pkg/qt${QT_DEFAULT_MAJOR_VERSION}/lib")
endif()

# Qt dependencies make building very slow
# Track only .h and .hpp files
include_regular_expression("^.*\\.h$|^.*\\.hpp$")

# Put executable in build root dir
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/psi" )

# Set CXX and C Flags
if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-overloaded-virtual")
elseif(WIN32)
    include(win32_definitions)
endif()

if(ENABLE_ASAN)
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
endif()

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    message(STATUS "CXX debug flags: ${CMAKE_CXX_FLAGS_DEBUG}")
    message(STATUS "C debug flags: ${CMAKE_C_FLAGS_DEBUG}")
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
    message(STATUS "CXX debug flags: ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    message(STATUS "C debug flags: ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
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

if (BUNDLED_IRIS_ALL)
    message(STATUS "BUNDLED_IRIS_ALL - ENABLED")
    set(BUNDLED_IRIS ON)
    set(IRIS_BUNDLED_QCA ON)
    set(IRIS_BUNDLED_USRSCTP ON)
endif()

if(IRIS_BUNDLED_QCA)
    add_definitions(-DQCA_STATIC)
endif()

if(USE_CCACHE)
    # Configure CCache if available
    find_program(CCACHE_PATH ccache DOC "Path to ccache")
    if(CCACHE_PATH)
        message(STATUS "Found ccache at ${CCACHE_PATH}")
        if(MSVC AND (CMAKE_VERSION VERSION_GREATER "3.13.0"))
            set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")
            set(CMAKE_VS_GLOBALS
                "TrackFileAccess=false"
                "UseMultiToolTask=true"
                "DebugInformationFormat=OldStyle"
            )
        endif()
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_PATH})
        set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE_PATH})
    endif()
endif()

if(NOT ONLY_PLUGINS)
    add_subdirectory( 3rdparty )
    if(BUNDLED_IRIS)
        set(IRIS_ENABLE_INSTALL OFF CACHE BOOL "Install iris headers" FORCE)
        add_subdirectory(iris)
    else()
        find_package( Iris REQUIRED )
        include_directories(${Iris_INCLUDE_DIR})
    endif()
    set( iris_LIB iris )
    add_subdirectory(src)
    if(ENABLE_PLUGINS)
        add_subdirectory(plugins)
    endif()
else()
    add_subdirectory(plugins)
endif()
