set(CMAKE_CXX_STANDARD 14)

set( CMAKE_MODULE_PATH
    "${CMAKE_MODULE_PATH}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/cmake/modules" )
set(CMAKE_AUTOMOC OFF)

if(LINUX)
    set(LIB_SUFFIX "" CACHE STRING "Define suffix of directory name (32/64)")
    if(NOT DEV_MODE)
        set(PSI_LIBDIR "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}/${PROJECT_NAME}" CACHE STRING "Path to Psi/Psi+ libraries directory")
        set(PSI_DATADIR "${CMAKE_INSTALL_PREFIX}/share/${PROJECT_NAME}" CACHE STRING "Path to Psi/Psi+ data directory")
    else()
        set(PSI_LIBDIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
        set(PSI_DATADIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    endif()
endif()

if(APPLE AND NOT PSI_LIBDIR)
    if(NOT DEV_MODE)
        set(PSI_LIBDIR "/Applications/${CLIENT_NAME}.app/Contents/Resources/plugins")
    else()
        set(PSI_LIBDIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins")
    endif()
endif()

set(TRANSLATIONS_DIR "${PROJECT_SOURCE_DIR}/translations")

find_package(Qt5 COMPONENTS Widgets Core Gui Xml Network Concurrent Multimedia Svg Sql REQUIRED)
if(ENABLE_WEBKIT)
    if(NOT USE_WEBENGINE)
        find_package(Qt5 COMPONENTS WebKit WebKitWidgets REQUIRED)
        set(WEBKIT_LIBS Qt5::WebKit Qt5::WebKitWidgets)
    else()
        find_package(Qt5 COMPONENTS WebEngine WebEngineWidgets WebChannel REQUIRED)
        set(WEBKIT_LIBS Qt5::WebEngine Qt5::WebEngineWidgets Qt5::WebChannel)
    endif()
endif()
if(USE_KEYCHAIN)
    find_package( Qt5 COMPONENTS Keychain QUIET )
    if( Qt5Keychain_FOUND )
        message(STATUS "Found Qt5Keychain: ${QTKEYCHAIN_LIBRARIES}")
        set(KEYCHAIN_LIBS ${QTKEYCHAIN_LIBRARIES})
        include_directories(${QTKEYCHAIN_INCLUDE_DIRS})
        add_definitions(-DHAVE_KEYCHAIN)
    else()
        message(STATUS "Qt5Keychain - not found")
    endif()
endif()

if(LINUX)
    find_package(Qt5 COMPONENTS DBus X11Extras REQUIRED)
    find_package(XCB REQUIRED)
    set(EXTRA_LIBS
        ${XCB_LIBRARY}
    )
    include_directories(${LIBXCB_INCLUDE_DIR})
    add_definitions(${LIBXCB_DEFINITIONS})
endif()

if(APPLE)
    find_package(Qt5MacExtras REQUIRED)
    if(USE_SPARKLE)
        find_package(Sparkle REQUIRED)
        include_directories(${SPARKLE_INCLUDE_DIR})
        add_definitions(-DHAVE_SPARKLE)
    endif()
endif()

add_definitions(${LIBXCB_DEFINITIONS})

if(NOT ONLY_BINARY AND (EXISTS "${TRANSLATIONS_DIR}"))
    set(LANGS_EXISTS ON)
    find_package(Qt5 COMPONENTS LinguistTools REQUIRED)
    set(QT_LUPDATE_EXECUTABLE ${Qt5_LUPDATE_EXECUTABLE})
endif()

set(QT_LIBRARIES Qt5::Widgets Qt5::Core Qt5::Network Qt5::Xml ${WEBKIT_LIBS} Qt5::Concurrent Qt5::Multimedia Qt5::Svg Qt5::Sql ${KEYCHAIN_LIBS})

if(LINUX)
    list(APPEND QT_LIBRARIES Qt5::DBus Qt5::X11Extras ${LIBXCB_LIBRARIES})
endif()
if(USE_WEBENGINE)
    list(APPEND EXTRA_LIBS qhttp)
endif()
if(APPLE)
    list(APPEND QT_LIBRARIES Qt5::MacExtras)
    if(USE_SPARKLE)
        list(APPEND EXTRA_LDFLAGS ${SPARKLE_LIBRARY})
    endif()
    list(APPEND EXTRA_LDFLAGS "-framework Carbon" "-framework AppKit" "-framework IOKit" "-framework CoreAudio" "-framework AudioToolbox")
endif()

find_package( Qca REQUIRED )

if(LINUX)
    find_package(X11 REQUIRED)
    include_directories(${X11_INCLUDE_DIR})
endif()

if(USE_ENCHANT)
    find_package(Enchant REQUIRED)
    list(APPEND EXTRA_LIBS ${Enchant_LIBRARY})
    include_directories(${Enchant_INCLUDE_DIR})
endif()
if(USE_HUNSPELL)
    find_package(Hunspell REQUIRED)
    list(APPEND EXTRA_LIBS ${HUNSPELL_LIBRARY})
    include_directories(${HUNSPELL_INCLUDE_DIR})
    if( WIN32 )
        include_directories( ${HUNSPELL_INCLUDE_DIR}/../ )
    endif()
endif()

macro(qt_wrap_cpp)
    set(_SOURCES ${ARGN})
    list(REMOVE_AT _SOURCES 0)
    foreach(SOURCE ${_SOURCES})
        if(SOURCE MATCHES "^(.*/|)([^/.]+)\\.h$")
            set(DEST "moc_${CMAKE_MATCH_2}.cpp")
            qt5_generate_moc(${SOURCE} ${DEST} TARGET ${PROJECT_NAME})
            list(APPEND ${ARGV0} ${DEST})
        elseif(SOURCE MATCHES "^(.*/|)([^/.]+)\\.cpp$")
            set(DEST "${CMAKE_MATCH_2}.moc")
            qt5_generate_moc(${SOURCE} ${DEST} TARGET ${PROJECT_NAME})
            list(APPEND ${ARGV0} ${DEST})
        else()
            message(WARNING "Wrong source ${SOURCE}")
        endif()
    endforeach()
    unset(_SOURCES)
endmacro()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}
    ${Qca_INCLUDE_DIR}
)

if( BUNDLED_IRIS )
    include_directories(
        ${PROJECT_SOURCE_DIR}/iris/src
        ${PROJECT_SOURCE_DIR}/iris/include
        ${PROJECT_SOURCE_DIR}/iris/include/iris
    )
else()
    include_directories(${Iris_INCLUDE_DIR})
endif()

string(TIMESTAMP PSI_COMPILATION_DATE "%Y-%m-%d")
string(TIMESTAMP PSI_COMPILATION_TIME "%H:%M:%S")

if(ENABLE_WEBKIT)
    if(NOT USE_WEBENGINE)
        set(PSI_VER_SUFFIX ", webkit")
    else()
        set(PSI_VER_SUFFIX ", webengine")
    endif()
endif()

if( NOT IS_PSIPLUS )
    set(CLIENT_NAME "Psi")
else()
    set(CLIENT_NAME "Psi+")
endif()

include(${PROJECT_SOURCE_DIR}/cmake/modules/get-version.cmake)

if(LINUX)
    message(STATUS "${CLIENT_NAME} libraries directory: ${PSI_LIBDIR}")
    message(STATUS "${CLIENT_NAME} data directory: ${PSI_DATADIR}")
elseif(APPLE)
    message(STATUS "${CLIENT_NAME} libraries directory: ${PSI_LIBDIR}")
endif()

configure_file(config.h.in config.h @ONLY)

add_definitions(
    -DQT_STATICPLUGIN
    -DHAVE_CONFIG
)

include(src.cmake)

# Only headers or very small sources
include(irisprotocol/irisprotocol.cmake)
include(protocol/protocol.cmake)
include(plugins/plugins.cmake)

add_subdirectory(AutoUpdater)
add_subdirectory(options)
add_subdirectory(tabs)
add_subdirectory(privacy)
add_subdirectory(Certificates)
add_subdirectory(avcall)
add_subdirectory(psimedia)
add_subdirectory(contactmanager)
add_subdirectory(tools)
add_subdirectory(libpsi/dialogs)
add_subdirectory(libpsi/tools)
add_subdirectory(widgets)
add_subdirectory(sxe)
add_subdirectory(whiteboarding)
if(APPLE)
    add_subdirectory(CocoaUtilities)
endif()

if(USE_WEBENGINE)
    include_directories(
        ${PROJECT_SOURCE_DIR}/3rdparty
        ${PROJECT_SOURCE_DIR}/3rdparty/http-parser
        ${PROJECT_SOURCE_DIR}/3rdparty/qhttp/src
        ${PROJECT_SOURCE_DIR}/3rdparty/qhttp/src/private
    )
endif()

if(LANGS_EXISTS)
    set(TRANSLATIONS
        ${TRANSLATIONS_DIR}/psi_be.ts
        ${TRANSLATIONS_DIR}/psi_bg.ts
        ${TRANSLATIONS_DIR}/psi_ca.ts
        ${TRANSLATIONS_DIR}/psi_cs.ts
        ${TRANSLATIONS_DIR}/psi_de.ts
        ${TRANSLATIONS_DIR}/psi_en.ts
        ${TRANSLATIONS_DIR}/psi_eo.ts
        ${TRANSLATIONS_DIR}/psi_es.ts
        ${TRANSLATIONS_DIR}/psi_et.ts
        ${TRANSLATIONS_DIR}/psi_fa.ts
        ${TRANSLATIONS_DIR}/psi_fi.ts
        ${TRANSLATIONS_DIR}/psi_fr.ts
        ${TRANSLATIONS_DIR}/psi_he.ts
        ${TRANSLATIONS_DIR}/psi_hu.ts
        ${TRANSLATIONS_DIR}/psi_it.ts
        ${TRANSLATIONS_DIR}/psi_ja.ts
        ${TRANSLATIONS_DIR}/psi_kk.ts
        ${TRANSLATIONS_DIR}/psi_mk.ts
        ${TRANSLATIONS_DIR}/psi_nl.ts
        ${TRANSLATIONS_DIR}/psi_pl.ts
        ${TRANSLATIONS_DIR}/psi_pt_BR.ts
        ${TRANSLATIONS_DIR}/psi_pt.ts
        ${TRANSLATIONS_DIR}/psi_ru.ts
        ${TRANSLATIONS_DIR}/psi_sk.ts
        ${TRANSLATIONS_DIR}/psi_sl.ts
        ${TRANSLATIONS_DIR}/psi_sr@latin.ts
        ${TRANSLATIONS_DIR}/psi_sv.ts
        ${TRANSLATIONS_DIR}/psi_sw.ts
        ${TRANSLATIONS_DIR}/psi_uk.ts
        ${TRANSLATIONS_DIR}/psi_ur_PK.ts
        ${TRANSLATIONS_DIR}/psi_vi.ts
        ${TRANSLATIONS_DIR}/psi_zh_CN.ts
        ${TRANSLATIONS_DIR}/psi_zh_TW.ts
    )

    set_property(SOURCE ${TRANSLATIONS}
        PROPERTY OUTPUT_LOCATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/translations)

    add_custom_target(lupdate)
    foreach(_LANG ${TRANSLATIONS})
        if(EXISTS "${_LANG}")
            get_filename_component(_SHORT_LANG ${_LANG} NAME_WE)
            string(REPLACE "@" "_" _SHORT_LANG ${_SHORT_LANG})
            add_custom_target(${_SHORT_LANG}
                COMMAND ${QT_LUPDATE_EXECUTABLE}
                ARGS -no-obsolete -recursive . ../iris -ts ${_LANG}
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                VERBATIM
            )
            add_dependencies(lupdate ${_SHORT_LANG})
        endif()
    endforeach()

    qt5_add_translation(QM ${TRANSLATIONS})
endif()

set(RESOURCES
    ${PROJECT_SOURCE_DIR}/psi.qrc
    ${PROJECT_SOURCE_DIR}/iconsets.qrc
)
qt5_add_resources(QRC_SOURCES ${RESOURCES})

qt5_wrap_ui(UI_FORMS ${FORMS})
add_custom_target(build_ui_files DEPENDS "${UI_FORMS}" SOURCES "${FORMS}")

qt_wrap_cpp(MOC_SOURCES ${SOURCES} ${HEADERS})

if(WIN32)
    if(IS_PSIPLUS)
        set(APP_NAME "Psi+")
    else()
        set(APP_NAME "Psi")
    endif()
    set(APP_COPYRIGHT "GNU GPL v2")
    set(APP_DESCRIPTION "${APP_NAME} : A cross-platform XMPP client designed for the power user")
    if(IS_PSIPLUS)
        set(COMPANY_NAME "Psi+ project")
    else()
        set(COMPANY_NAME "psi-im.org")
    endif()
    string(REPLACE "\." "," FILE_VERSION ${APP_VERSION})
    if(BUILD_ARCH STREQUAL "i386" OR (BUILD_ARCH STREQUAL "win32"))
        set(RC_SUFFIX "win32")
    else()
        set(RC_SUFFIX "win64")
    endif()
    # win resource creation
    configure_file(${PROJECT_SOURCE_DIR}/win32/psi_win.rc.in ${CMAKE_CURRENT_BINARY_DIR}/psi_win.rc @ONLY)
    # win resource compilation
    function(compile_rc_file RC_FILE_NAME RC_OUTPUT_NAME)
        set(_RC_FILE_NAME ${RC_FILE_NAME})
        set(_RC_OUTPUT_NAME ${RC_OUTPUT_NAME})
        if(NOT MSVC)
            set(CMD_ARG
                --include=${CMAKE_CURRENT_SOURCE_DIR}
                --input=${_RC_FILE_NAME}
                --output=${_RC_OUTPUT_NAME}
        )
        else()
            set(CMD_ARG
                /fo
                ${_RC_OUTPUT_NAME}
                ${_RC_FILE_NAME}
            )
        endif()
        add_custom_command(OUTPUT ${_RC_OUTPUT_NAME}
            COMMAND ${CMAKE_RC_COMPILER}
            ARGS ${CMD_ARG}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/win32
            VERBATIM
        )
        set(_RC_FILE_NAME "")
        set(_RC_OUTPUT_NAME "")
    endfunction()

    set(DECS_RC_NAME ${CMAKE_CURRENT_BINARY_DIR}/psi_win.rc)
    set(RC_FILE ${CMAKE_CURRENT_BINARY_DIR}/psi_win.o)
    compile_rc_file(${DECS_RC_NAME} ${RC_FILE})
endif()

add_executable(${PROJECT_NAME}
    WIN32
    MACOSX_BUNDLE
    ${SOURCES}
    ${HEADERS}
    ${MOC_SOURCES}
    ${UI_FORMS}
    ${PLAIN_SOURCES}
    ${PLAIN_HEADERS}
    ${QRC_SOURCES}
    ${QM}
    ${RC_FILE}
)

include(${PROJECT_SOURCE_DIR}/cmake/modules/fix-codestyle.cmake)

#Experimental feature
if(VERBOSE_PROGRAM_NAME)
    if(ENABLE_WEBKIT AND (NOT USE_WEBENGINE))
        set(VERBOSE_SUFFIX "-webkit")
    elseif(USE_WEBENGINE)
        set(VERBOSE_SUFFIX "-webengine")
    endif()
endif()
#

if(ENABLE_PORTABLE OR VERBOSE_PROGRAM_NAME)
    if(ENABLE_PORTABLE)
        set(PORTABLE_SUFFIX "-portable")
    endif()
    set(VERBOSED_NAME "${PROJECT_NAME}${VERBOSE_SUFFIX}${PORTABLE_SUFFIX}")
    set_target_properties(${PROJECT_NAME}
                PROPERTIES
                OUTPUT_NAME
                ${VERBOSED_NAME}
    )
endif()

set(STANDARD_NUM 14)

add_dependencies(${PROJECT_NAME} iris)
if(USE_WEBENGINE)
    add_dependencies(${PROJECT_NAME} qhttp)
endif()
add_dependencies(${PROJECT_NAME} build_ui_files)

if(WIN32 AND MSVC)
    list(APPEND EXTRA_LIBS
        wsock32
        user32
        shell32
        gdi32
        advapi32
    )
    if(USE_WEBENGINE)
        list(APPEND EXTRA_LIBS
            imm32
        )
    endif()
    add_definitions(
        -DQT_STATICPLUGIN
    )
endif()

target_link_libraries(${PROJECT_NAME}
    ${QT_LIBRARIES}
    ${EXTRA_LDFLAGS}
    ${X11_LIBRARIES}
    ${EXTRA_LIBS}
    iris
    ${Qca_LIBRARY}
)
target_link_libraries(${PROJECT_NAME}
    options
    Certificates
    psimedia
    contactmanager
    avcall
    whiteboarding
    sxe
    tools
    libpsi_dialogs
    libpsi_tools
    widgets
    privacy
    tabs
    AutoUpdater
)
if(APPLE)
    target_link_libraries(${PROJECT_NAME} CocoaUtilities)
endif()

set(OTHER_FILES
    ${PROJECT_SOURCE_DIR}/certs
    ${PROJECT_SOURCE_DIR}/iconsets
    ${PROJECT_SOURCE_DIR}/sound
)

if(ENABLE_PLUGINS AND EXISTS ${PROJECT_SOURCE_DIR}/skins)
    list(APPEND OTHER_FILES ${PROJECT_SOURCE_DIR}/skins)
endif()

set(DOC_FILES
    "${PROJECT_SOURCE_DIR}/COPYING"
    "${PROJECT_SOURCE_DIR}/README.html"
)

#PREPARE SECTION
copy("${PROJECT_SOURCE_DIR}/certs/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/certs/" prepare-bin)
copy("${PROJECT_SOURCE_DIR}/iconsets/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/iconsets/" prepare-bin)
if(ENABLE_WEBKIT)
    copy("${PROJECT_SOURCE_DIR}/themes/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/themes/" prepare-bin)
endif()
copy("${PROJECT_SOURCE_DIR}/sound/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sound/" prepare-bin)
if(ENABLE_PLUGINS)
    copy("${PROJECT_SOURCE_DIR}/skins/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/skins/" prepare-bin)
endif()
copy("${PROJECT_SOURCE_DIR}/client_icons.txt" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin)

#INSTALL SECTION
if(LINUX)
    set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
    set(DATA_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/share")
    set(APPS_INSTALL_DIR "${DATA_INSTALL_DIR}/applications")
    set(PIXMAPS_INSTALL_DIR "${DATA_INSTALL_DIR}/pixmaps")
    install(TARGETS ${PROJECT_NAME} DESTINATION ${BIN_INSTALL_DIR})
    if(NOT ONLY_BINARY)
        set(DESKTOP_FILE "${PROJECT_SOURCE_DIR}/psi.desktop")
        if(NOT VERBOSED_NAME)
            set(VERBOSED_NAME ${PROJECT_NAME})
        endif()
        if(VERBOSE_PROGRAM_NAME)
            include(${PROJECT_SOURCE_DIR}/cmake/modules/generate_desktopfile.cmake)
        else()
            set(OUT_DESK_FILE "${DESKTOP_FILE}")
        endif()
        install(FILES ${OUT_DESK_FILE} DESTINATION ${APPS_INSTALL_DIR} RENAME ${VERBOSED_NAME}.desktop)
        if(IS_PSIPLUS)
            set(PSI_LOGO_FILE ${PROJECT_SOURCE_DIR}/iconsets/system/default/psiplus/logo_128.png)
            set(SHARE_SUFF "psi-plus")
        else()
            set(PSI_LOGO_FILE ${PROJECT_SOURCE_DIR}/iconsets/system/default/logo_128.png)
            set(SHARE_SUFF "psi")
        endif()
        install(FILES ${PSI_LOGO_FILE} DESTINATION ${PIXMAPS_INSTALL_DIR} RENAME ${VERBOSED_NAME}.png)
        if(INSTALL_EXTRA_FILES)
            set(PROJECT_OUTPUT_DATA_DIR "${DATA_INSTALL_DIR}/${SHARE_SUFF}")
            install(FILES ${PROJECT_SOURCE_DIR}/client_icons.txt DESTINATION ${PROJECT_OUTPUT_DATA_DIR})
            install(DIRECTORY ${OTHER_FILES} DESTINATION ${PROJECT_OUTPUT_DATA_DIR})
            if(ENABLE_WEBKIT)
                install(DIRECTORY ${PROJECT_SOURCE_DIR}/themes DESTINATION ${PROJECT_OUTPUT_DATA_DIR})
            endif()
            if(LANGS_EXISTS)
                install(FILES ${QM} DESTINATION ${PROJECT_OUTPUT_DATA_DIR}/translations)
            endif()
            install(FILES ${DOC_FILES} DESTINATION "${PROJECT_OUTPUT_DATA_DIR}")
        endif()
        if(INSTALL_PLUGINS_SDK)
            set(plugins_dir "${PSI_LIBDIR}/plugins")
            set(data_dir ${PROJECT_OUTPUT_DATA_DIR})
            configure_file(${PROJECT_SOURCE_DIR}/src/plugins/pluginsconf.pri.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/pluginsconf.pri @ONLY)
            configure_file(${PROJECT_SOURCE_DIR}/src/plugins/variables.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/variables.cmake @ONLY)
            install(FILES ${PLUGINS_INCLUDES} DESTINATION ${PROJECT_OUTPUT_DATA_DIR}/plugins/include)
            install(FILES ${CMAKE_CURRENT_BINARY_DIR}/variables.cmake DESTINATION ${PROJECT_OUTPUT_DATA_DIR}/plugins)
            install(FILES ${PROJECT_SOURCE_DIR}/src/plugins/plugins.pri DESTINATION ${PROJECT_OUTPUT_DATA_DIR}/plugins)
            install(FILES ${PROJECT_SOURCE_DIR}/src/plugins/psiplugin.pri DESTINATION ${PROJECT_OUTPUT_DATA_DIR}/plugins)
            install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pluginsconf.pri DESTINATION ${PROJECT_OUTPUT_DATA_DIR}/plugins)
            install(FILES ${PROJECT_SOURCE_DIR}/cmake/modules/FindPsiPluginsApi.cmake DESTINATION ${DATA_INSTALL_DIR}/cmake/Modules)
        endif()
    endif()
endif(LINUX)

if(WIN32)
    install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX})
    if(NOT ONLY_BINARY AND INSTALL_EXTRA_FILES)
        install(FILES ${PROJECT_SOURCE_DIR}/client_icons.txt DESTINATION ${CMAKE_INSTALL_PREFIX})
        install(DIRECTORY ${OTHER_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX})
        if(ENABLE_WEBKIT)
            install(DIRECTORY ${PROJECT_SOURCE_DIR}/themes DESTINATION ${CMAKE_INSTALL_PREFIX})
        endif()
        if(LANGS_EXISTS)
            install(FILES ${QM} DESTINATION ${CMAKE_INSTALL_PREFIX}/translations)
        endif()
    endif()
    if(EXISTS "${PROJECT_SOURCE_DIR}/myspell")
        copy("${PROJECT_SOURCE_DIR}/myspell/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/myspell/" prepare-bin)
    endif()
    if(ENABLE_PORTABLE OR DEV_MODE)
        include(${PROJECT_SOURCE_DIR}/cmake/modules/win32-prepare-deps.cmake)
    endif()
endif(WIN32)

if(APPLE)
    include("${PROJECT_SOURCE_DIR}/mac/macos_definitions.cmake")

    set(PROJECT_OUTPUT_DATA_DIR "${CLIENT_NAME}.app/Contents/Resources")

    list(APPEND SOURCES ${MACOSX_ICON})
    set_source_files_properties(${MACOSX_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_property(TARGET ${PROJECT_NAME} PROPERTY OUTPUT_NAME "${CLIENT_NAME}")
    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "bin" BUNDLE DESTINATION ".")

    # Special hack for qca package from Homebrew
    if(HOMEBREW)
        install(DIRECTORY "${HOMEBREW}/opt/qca/lib/qt5/plugins/crypto"
                DESTINATION "${CLIENT_NAME}.app/Contents/PlugIns")
    endif()
    # end of qca hack

    # Simple and smart hack for using of macdeployqt to fix Psi plugins
    if(ENABLE_PLUGINS AND (NOT ONLY_BINARY))
        install(DIRECTORY "${CMAKE_BINARY_DIR}/psi/plugins"
                DESTINATION "${CLIENT_NAME}.app/Contents/PlugIns")
    endif()
    # end of plugins hack

    # Create a deployable application bundle that contains the all necessary libraries
    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST
                          ${PROJECT_SOURCE_DIR}/mac/Info.plist.in)
    install(CODE "execute_process(COMMAND ${MACDEPLOYQT} ${CLIENT_NAME}.app -verbose=2
                                  WORKING_DIRECTORY \${CMAKE_INSTALL_PREFIX})")

    # Move Psi plugins fixed by macdeployqt to proper place
    if(ENABLE_PLUGINS AND (NOT ONLY_BINARY))
        install(CODE "execute_process(COMMAND mv ${CLIENT_NAME}.app/Contents/PlugIns/plugins \
                                                 ${CLIENT_NAME}.app/Contents/Resources/
                                      WORKING_DIRECTORY \${CMAKE_INSTALL_PREFIX})")
    endif()
    # end of plugins hack

    if(NOT ONLY_BINARY)
        if(INSTALL_EXTRA_FILES)
            install(FILES ${PROJECT_SOURCE_DIR}/client_icons.txt DESTINATION "${PROJECT_OUTPUT_DATA_DIR}")
            install(DIRECTORY ${OTHER_FILES} DESTINATION "${PROJECT_OUTPUT_DATA_DIR}")
            if(ENABLE_WEBKIT)
                install(DIRECTORY ${PROJECT_SOURCE_DIR}/themes DESTINATION "${PROJECT_OUTPUT_DATA_DIR}")
            endif()
            if(LANGS_EXISTS)
                install(FILES ${QM} DESTINATION "${PROJECT_OUTPUT_DATA_DIR}/translations")
            endif()
            install(FILES ${MACOSX_ICON} DESTINATION "${PROJECT_OUTPUT_DATA_DIR}")
        endif()
    endif()
endif(APPLE)
#INSTALL SECTION END

if(ENABLE_PLUGINS AND (NOT ONLY_BINARY))
    add_subdirectory(plugins)
endif()

