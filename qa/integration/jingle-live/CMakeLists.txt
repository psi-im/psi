cmake_minimum_required(VERSION 3.19)
project(psi_jingle_live_smoke LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(QT_DEFAULT_MAJOR_VERSION 6)

option(JINGLE_LIVE_ENABLE_FT "Build SCTP-enabled Jingle file-transfer live client" OFF)
option(JINGLE_LIVE_FT_SANITIZE "Instrument the live Jingle FT client and Iris with ASan/UBSan" ON)

if(NOT DEFINED PSI_SOURCE_DIR)
    message(FATAL_ERROR "PSI_SOURCE_DIR is required")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Network Xml)
if(DEFINED PSIMEDIA_SOURCE_DIR)
    find_package(Qt6 REQUIRED COMPONENTS Gui Widgets)
endif()

# Keep the live FT reproducer fully instrumented, including Iris itself. The
# crash we are chasing occurs while unwinding synchronous SCTP/readyRead paths,
# so instrumenting only the small driver would miss accesses made in Iris.
set(JINGLE_LIVE_FT_ASAN_OPTIONS_SOURCE "")
if(JINGLE_LIVE_ENABLE_FT AND JINGLE_LIVE_FT_SANITIZE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -O1 -g)
    add_link_options(-fsanitize=address,undefined -fno-omit-frame-pointer)

    # The live client intentionally exits immediately after a completed session,
    # while QCA/OpenSSL/SASL and ICE keep process-lifetime allocations around.
    # LeakSanitizer therefore turns otherwise successful FT cases into exit 1.
    # Keep ASan/UBSan enabled for UAF, invalid-access and reentrancy bugs, but do
    # not let unrelated shutdown leaks hide the data-plane result we are after.
    set(JINGLE_LIVE_FT_ASAN_OPTIONS_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/ft_asan_options.cpp")
    file(WRITE "${JINGLE_LIVE_FT_ASAN_OPTIONS_SOURCE}"
        "extern \"C\" const char *__asan_default_options() { return \"detect_leaks=0\"; }\n")
endif()

# Live integration jobs deliberately build arbitrary Iris gitlink commits from
# shallow clones. Such commits need not have a release tag reachable from their
# one-commit history, so provide the build version explicitly rather than
# weakening Iris' strict standalone version resolution.
set(IRIS_VERSION 1.1 CACHE STRING "" FORCE)
set(IRIS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(IRIS_ENABLE_JINGLE_SCTP ${JINGLE_LIVE_ENABLE_FT} CACHE BOOL "" FORCE)
set(IRIS_ENABLE_OMEMO OFF CACHE BOOL "" FORCE)
set(IRIS_BUNDLED_USRSCTP OFF CACHE BOOL "" FORCE)
set(IRIS_BUNDLED_QCA OFF CACHE BOOL "" FORCE)
set(IRIS_SYSTEM_QCA 3 CACHE STRING "" FORCE)
set(USE_QT6 ON CACHE BOOL "" FORCE)
add_subdirectory("${PSI_SOURCE_DIR}/iris" iris)

add_executable(psi_jmi_live_client jmi_client.cpp)
target_link_libraries(psi_jmi_live_client PRIVATE
    Iris::Iris
    Qt6::Core
    Qt6::Network
    Qt6::Xml
)

if(JINGLE_LIVE_ENABLE_FT)
    add_executable(psi_jingle_ft_live_client
        ft_client.cpp
        ${JINGLE_LIVE_FT_ASAN_OPTIONS_SOURCE}
    )
    target_link_libraries(psi_jingle_ft_live_client PRIVATE
        Iris::Iris
        Qt6::Core
        Qt6::Network
        Qt6::Xml
    )

    add_executable(psi_jingle_ft_memory_probe ft_memory_probe.cpp)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(psi_jingle_ft_memory_probe PRIVATE
            -include iris/xmpp-im/jingle-s5b.h
            -include iris/xmpp-im/jingle-ibb.h
        )
    endif()
    target_link_libraries(psi_jingle_ft_memory_probe PRIVATE
        Iris::Iris
        Qt6::Core
        Qt6::Network
        Qt6::Xml
    )
endif()

if(DEFINED PSIMEDIA_SOURCE_DIR)
    set(BUILD_DEMO OFF CACHE BOOL "" FORCE)
    set(BUILD_PSIPLUGIN OFF CACHE BOOL "" FORCE)
    set(PSIMEDIA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(USE_PSI ON CACHE BOOL "" FORCE)
    set(MAIN_PROGRAM_NAME psi CACHE STRING "" FORCE)
    set(PLUGINS_ROOT_DIR "${PSI_SOURCE_DIR}/plugins" CACHE STRING "" FORCE)

    add_subdirectory("${PSIMEDIA_SOURCE_DIR}" psimedia)

    add_library(psi_jingle_live_media_adapter STATIC
        "${PSI_SOURCE_DIR}/src/psimedia/psimedia.cpp"
        "${PSI_SOURCE_DIR}/src/psimedia/psimedia.h"
        "${PSI_SOURCE_DIR}/src/psimedia/psimedia_p.h"
        "${PSI_SOURCE_DIR}/src/avcall/psimediajingle.cpp"
        "${PSI_SOURCE_DIR}/src/avcall/psimediajingle.h"
    )
    target_compile_definitions(psi_jingle_live_media_adapter PRIVATE QT_GUI_LIB)
    target_include_directories(psi_jingle_live_media_adapter PUBLIC
        "${PSI_SOURCE_DIR}/src"
        "${PSI_SOURCE_DIR}/src/psimedia"
        "${PSI_SOURCE_DIR}/src/avcall"
        "${PSI_SOURCE_DIR}/plugins/include"
    )
    target_link_libraries(psi_jingle_live_media_adapter PUBLIC
        Iris::Iris
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
    )

    add_executable(psi_jingle_call_live_client call_client.cpp)
    target_include_directories(psi_jingle_call_live_client PRIVATE
        "${PSI_SOURCE_DIR}/src"
        "${PSI_SOURCE_DIR}/src/psimedia"
        "${PSI_SOURCE_DIR}/src/avcall"
        "${PSI_SOURCE_DIR}/plugins/include"
        "${PSIMEDIA_SOURCE_DIR}/gstprovider"
    )
    target_link_libraries(psi_jingle_call_live_client PRIVATE
        psi_jingle_live_media_adapter
        gstprovidersrc
        Iris::Iris
        Qt6::Core
        Qt6::Network
        Qt6::Xml
    )
endif()
