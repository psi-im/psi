set(CMAKE_CXX_STANDARD 14)

set( CMAKE_MODULE_PATH
    "${CMAKE_MODULE_PATH}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/cmake/modules" )
set(CMAKE_AUTOMOC ON)

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
if(IS_WEBKIT)
    find_package(Qt5 COMPONENTS WebKit WebKitWidgets REQUIRED)
    set(WEBKIT_LIBS Qt5::WebKit Qt5::WebKitWidgets)
elseif(IS_WEBENGINE)
    find_package(Qt5 COMPONENTS WebEngine WebEngineWidgets WebChannel REQUIRED)
    set(WEBKIT_LIBS Qt5::WebEngine Qt5::WebEngineWidgets Qt5::WebChannel)
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

if(NOT ONLY_BINARY AND (EXISTS "${TRANSLATIONS_DIR}"))
    set(LANGS_EXISTS ON)
    message(STATUS "${PROJECT_NAME} translations found at ${TRANSLATIONS_DIR}")
    find_package(Qt5 COMPONENTS LinguistTools REQUIRED)
    set(QT_LUPDATE_EXECUTABLE ${Qt5_LUPDATE_EXECUTABLE})
endif()

set(QT_LIBRARIES Qt5::Widgets Qt5::Core Qt5::Network Qt5::Xml ${WEBKIT_LIBS} Qt5::Concurrent Qt5::Multimedia Qt5::Svg Qt5::Sql ${KEYCHAIN_LIBS})

if(LINUX)
    list(APPEND QT_LIBRARIES Qt5::DBus Qt5::X11Extras ${LIBXCB_LIBRARIES})
endif()
if(UNIX OR IS_WEBENGINE)
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

if(HAIKU)
    list(APPEND EXTRA_LIBS root be network)
endif()

if(USE_ENCHANT)
    find_package(Enchant REQUIRED)
    list(APPEND EXTRA_LIBS ${Enchant_LIBRARY})
    include_directories(${Enchant_INCLUDE_DIR})
elseif(USE_HUNSPELL)
    find_package(Hunspell REQUIRED)
    list(APPEND EXTRA_LIBS ${HUNSPELL_LIBRARY})
    include_directories(${HUNSPELL_INCLUDE_DIR})
    if( WIN32 )
        include_directories( ${HUNSPELL_INCLUDE_DIR}/../ )
    endif()
elseif(USE_ASPELL)
    find_package(ASPELL REQUIRED)
    list(APPEND EXTRA_LIBS ${ASPELL_LIBRARIES})
    include_directories(${ASPELL_INCLUDE_DIR})
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Qca_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/3rdparty/qite/libqite
)

if(BUNDLED_IRIS)
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

if(IS_WEBKIT)
    set(PSI_VER_SUFFIX ", webkit")
elseif(IS_WEBENGINE)
    set(PSI_VER_SUFFIX ", webengine")
endif()

if( NOT IS_PSIPLUS )
    set(CLIENT_NAME "Psi")
    set(CLIENT_SNAME "psi")
    set(CLIENT_CAPS_NODE "https://psi-im.org")
    set(PSILOGO_PREFIX "")
    set(MAIN_ICON "psimain.png")
else()
    set(CLIENT_NAME "Psi+")
    set(CLIENT_SNAME "psi+")
    set(CLIENT_CAPS_NODE "https://psi-plus.com")
    set(PSILOGO_PREFIX "psiplus/")
    set(MAIN_ICON "psiplus_icon.png")
endif()

include(${PROJECT_SOURCE_DIR}/cmake/modules/get-version.cmake)

if(LINUX)
    message(STATUS "${CLIENT_NAME} libraries directory: ${PSI_LIBDIR}")
    message(STATUS "${CLIENT_NAME} data directory: ${PSI_DATADIR}")
elseif(APPLE)
    message(STATUS "${CLIENT_NAME} libraries directory: ${PSI_LIBDIR}")
endif()

set(CONFIG_OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/config.h")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CONFIG_OUTPUT_FILE} @ONLY)

#Copy default iconsets to build directory and add jisp files to prepare-bin target
function(prepare_iconsets ACTION)
    file(GLOB_RECURSE all_iconsets "${PROJECT_SOURCE_DIR}/iconsets/*")
    message(STATUS "Processing iconsets for ${ACTION}")
    foreach(_ITEM ${all_iconsets})
        get_filename_component(FNAME ${_ITEM} NAME)
        file(RELATIVE_PATH FREL ${PROJECT_SOURCE_DIR} ${_ITEM})
        get_filename_component(FDIR ${FREL} DIRECTORY)
        if(NOT ${ACTION} STREQUAL "prepare-bin")
            if("${_ITEM}" MATCHES ".*/default/.*" AND (NOT "${_ITEM}" MATCHES ".*/system/default/icondef.xml"))
                configure_file(${_ITEM} "${CMAKE_CURRENT_BINARY_DIR}/${FDIR}/${FNAME}" COPYONLY)
            endif()
        else()
            if(NOT "${_ITEM}" MATCHES ".*/default/.*|.*README")
                copy(${_ITEM} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${FDIR}/${FNAME}" ${ACTION})
            endif()
        endif()
        unset(_ITEM)
        unset(FNAME)
        unset(FREL)
        unset(FDIR)
    endforeach()
endfunction()

prepare_iconsets(prepare-src)
#Generate iconsets.qrc and icondef.xml
set(ICONDEFXML_OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/iconsets/system/default/icondef.xml")
set(ICONSETSQRC_OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/iconsets.qrc")
configure_file(${PROJECT_SOURCE_DIR}/icondef.xml.in ${ICONDEFXML_OUTPUT_FILE} @ONLY)
configure_file(${PROJECT_SOURCE_DIR}/iconsets.qrc.in ${ICONSETSQRC_OUTPUT_FILE} @ONLY)

include(src.cmake)

if(EXISTS "${CONFIG_OUTPUT_FILE}")
    message(STATUS "${CONFIG_OUTPUT_FILE} file generated")
    list(APPEND HEADERS ${CONFIG_OUTPUT_FILE})
endif()

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

if(UNIX OR IS_WEBENGINE)
    add_definitions(-DHAVE_WEBSERVER)
    find_package(HttpParser QUIET "2.2")
    if(NOT HttpParser_FOUND)
        include_directories(
            ${PROJECT_SOURCE_DIR}/3rdparty/http-parser
        )
    else()
        include_directories(
            ${HttpParser_INCLUDE_DIR}
        )
    endif()
    include_directories(
        ${PROJECT_SOURCE_DIR}/3rdparty
        ${PROJECT_SOURCE_DIR}/3rdparty/qhttp/src
        ${PROJECT_SOURCE_DIR}/3rdparty/qhttp/src/private
    )
endif()

if(LANGS_EXISTS)
    file(GLOB PSI_TRANSLATIONS "${TRANSLATIONS_DIR}/psi_*.ts")

    set_property(SOURCE ${PSI_TRANSLATIONS}
        PROPERTY OUTPUT_LOCATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/translations
        )

    #update translations with: make lupdate
    add_custom_target(lupdate)
    foreach(_LANG ${PSI_TRANSLATIONS})
        get_filename_component(_SHORT_LANG ${_LANG} NAME_WE)
        string(REPLACE "@" "_" _SHORT_LANG ${_SHORT_LANG})
        add_custom_command(TARGET lupdate
            PRE_BUILD
            COMMAND ${QT_LUPDATE_EXECUTABLE}
            ARGS -no-obsolete -recursive ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/iris -ts ${_LANG}
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            VERBATIM
        )
    endforeach()

    # Psi translations
    qt5_add_translation(QM ${PSI_TRANSLATIONS})

    if(WIN32 OR APPLE)
        # Qt translations
        get_target_property(QtCore_location Qt5::Core LOCATION)
        get_filename_component(QtCore_DIR "${QtCore_location}" DIRECTORY)
        if(APPLE)
            get_filename_component(QT_TRANSLATIONS_DIR "${QtCore_DIR}/../../translations" ABSOLUTE)
        else()
            get_filename_component(QT_TRANSLATIONS_DIR "${QtCore_DIR}/../translations" ABSOLUTE) #checked with MXE
        endif()
        if(EXISTS "${QT_TRANSLATIONS_DIR}")
            message(STATUS "Qt translations found at ${QT_TRANSLATIONS_DIR}")
            file(GLOB QT_TRANSLATIONS "${QT_TRANSLATIONS_DIR}/q*.qm")
            list(APPEND QM ${QT_TRANSLATIONS})
        endif()
    endif()
    add_custom_target(build_translations DEPENDS ${QM})
endif()

set(RESOURCES
    ${PROJECT_SOURCE_DIR}/psi.qrc
    ${ICONSETSQRC_OUTPUT_FILE}
)

qt5_add_resources(QRC_SOURCES ${RESOURCES})

qt5_wrap_ui(UI_FORMS ${FORMS})
add_custom_target(build_ui_files DEPENDS "${UI_FORMS}")

if(WIN32)
    if(IS_PSIPLUS)
        set(APP_NAME "Psi+")
    else()
        set(APP_NAME "Psi")
    endif()
    set(APP_COPYRIGHT "GNU GPL v2")
    set(APP_DESCRIPTION "${APP_NAME} : A cross-platform XMPP client aimed at experienced users.")
    if(IS_PSIPLUS)
        set(COMPANY_NAME "Psi+ project")
        set(APP_ICON "app-plus.ico")
    else()
        set(COMPANY_NAME "psi-im.org")
        set(APP_ICON "app.ico")
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
    ${UI_FORMS}
    ${QRC_SOURCES}
    ${RC_FILE}
)

include(${PROJECT_SOURCE_DIR}/cmake/modules/fix-codestyle.cmake)

#Experimental feature
if(VERBOSE_PROGRAM_NAME)
    if(IS_WEBKIT)
        set(VERBOSE_SUFFIX "-webkit")
    elseif(IS_WEBENGINE)
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

if(LANGS_EXISTS)
    add_dependencies(${PROJECT_NAME} build_translations)
endif()
add_dependencies(${PROJECT_NAME} iris)
if(UNIX OR IS_WEBENGINE)
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
    if(IS_WEBENGINE)
        list(APPEND EXTRA_LIBS
            imm32
        )
    endif()
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
copy("${PROJECT_SOURCE_DIR}/sound/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sound/" prepare-bin)
if(IS_WEBKIT OR IS_WEBENGINE)
    copy("${PROJECT_SOURCE_DIR}/themes/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/themes/" prepare-bin)
endif()
if(ENABLE_PLUGINS AND EXISTS ${PROJECT_SOURCE_DIR}/skins)
    copy("${PROJECT_SOURCE_DIR}/skins/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/skins/" prepare-bin)
endif()
copy("${PROJECT_SOURCE_DIR}/client_icons.txt" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin)
if((WIN32 OR APPLE) AND LANGS_EXISTS)
    set(QM_OUTPUT "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/translations")
    foreach(_LANG ${QM})
        get_filename_component(QM_NAME ${_LANG} NAME)
        if(NOT EXISTS "${QM_OUTPUT}/${QM_NAME}")
            copy("${_LANG}" "${QM_OUTPUT}/" prepare-bin)
        endif()
    endforeach()
endif()
prepare_iconsets(prepare-bin)
#PREPARE SECTION END

if(INSTALL_EXTRA_FILES)
    set(EXCLUDE_REGEXP "activities|affiliations|clients|emoticons|moods|system|default")
endif()

#INSTALL SECTION
if(LINUX)
    set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
    set(DATA_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/share")
    set(APPS_INSTALL_DIR "${DATA_INSTALL_DIR}/applications")
    set(PIXMAPS_INSTALL_DIR "${DATA_INSTALL_DIR}/pixmaps")
    set(ICONS_INSTALL_PREFIX "${DATA_INSTALL_DIR}/icons/hicolor")
    set(RESOLUTIONS 16 32 48 64 128)
    install(TARGETS ${PROJECT_NAME} DESTINATION ${BIN_INSTALL_DIR})
    if(NOT ONLY_BINARY)
        if(NOT VERBOSED_NAME)
            set(VERBOSED_NAME ${PROJECT_NAME})
        endif()
        #Generate .desktop file
        include(${PROJECT_SOURCE_DIR}/cmake/modules/generate_desktopfile.cmake)
        install(FILES ${OUT_DESK_FILE} DESTINATION ${APPS_INSTALL_DIR})
        if(IS_PSIPLUS)
            set(PSI_LOGO_PREFIX ${PROJECT_SOURCE_DIR}/iconsets/system/default/psiplus)
            set(SHARE_SUFF "psi-plus")
        else()
            set(PSI_LOGO_PREFIX ${PROJECT_SOURCE_DIR}/iconsets/system/default)
            set(SHARE_SUFF "psi")
        endif()
        #Install logo and pixmaps
        install(FILES ${PSI_LOGO_PREFIX}/logo_128.png DESTINATION ${PIXMAPS_INSTALL_DIR} RENAME ${VERBOSED_NAME}.png)
        foreach(resolution ${RESOLUTIONS})
            unset(LOGO_NAME)
            set(LOGO_NAME "${PSI_LOGO_PREFIX}/logo_${resolution}.png")
            install(FILES ${LOGO_NAME} DESTINATION "${ICONS_INSTALL_PREFIX}/${resolution}x${resolution}/apps" RENAME ${VERBOSED_NAME}.png)
        endforeach()
        if(INSTALL_EXTRA_FILES)
            set(PROJECT_OUTPUT_DATA_DIR "${DATA_INSTALL_DIR}/${SHARE_SUFF}")
            install(FILES ${PROJECT_SOURCE_DIR}/client_icons.txt DESTINATION ${PROJECT_OUTPUT_DATA_DIR})
            #iconsets
            install(DIRECTORY ${PROJECT_SOURCE_DIR}/iconsets DESTINATION ${PROJECT_OUTPUT_DATA_DIR} REGEX ${EXCLUDE_REGEXP} EXCLUDE)
            install(DIRECTORY ${OTHER_FILES} DESTINATION ${PROJECT_OUTPUT_DATA_DIR})
            if(IS_WEBKIT OR IS_WEBENGINE)
                install(DIRECTORY ${PROJECT_SOURCE_DIR}/themes DESTINATION ${PROJECT_OUTPUT_DATA_DIR})
            endif()
            if(LANGS_EXISTS)
                install(FILES ${QM} DESTINATION ${PROJECT_OUTPUT_DATA_DIR}/translations)
            endif()
            install(FILES ${DOC_FILES} DESTINATION "${PROJECT_OUTPUT_DATA_DIR}")
        endif()
        if(INSTALL_PLUGINS_SDK)
            set(API_OUTPUT_DATA_DIR "${DATA_INSTALL_DIR}/${SHARE_SUFF}")
            set(API_INCLUDES_DIR "${CMAKE_INSTALL_PREFIX}/include/${SHARE_SUFF}/plugins")
            set(plugins_dir "${PSI_LIBDIR}/plugins")
            set(plugins_dir_sfx "lib${LIB_SUFFIX}/${SHARE_SUFF}/plugins")
            set(data_dir "${API_OUTPUT_DATA_DIR}")
            set(data_dir_sfx "share/${SHARE_SUFF}/plugins")
            set(plugins_includes_dir "${API_INCLUDES_DIR}")
            configure_file(${PROJECT_SOURCE_DIR}/src/plugins/pluginsconf.pri.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/pluginsconf.pri @ONLY)
            configure_file(${PROJECT_SOURCE_DIR}/src/plugins/variables.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/variables.cmake @ONLY)
            install(FILES ${PLUGINS_INCLUDES} DESTINATION ${API_INCLUDES_DIR})
            install(FILES ${CMAKE_CURRENT_BINARY_DIR}/variables.cmake DESTINATION ${API_OUTPUT_DATA_DIR}/plugins)
            install(FILES ${PROJECT_SOURCE_DIR}/src/plugins/plugins.pri DESTINATION ${API_OUTPUT_DATA_DIR}/plugins)
            install(FILES ${PROJECT_SOURCE_DIR}/src/plugins/psiplugin.pri DESTINATION ${API_OUTPUT_DATA_DIR}/plugins)
            install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pluginsconf.pri DESTINATION ${API_OUTPUT_DATA_DIR}/plugins)
            install(FILES ${PROJECT_SOURCE_DIR}/cmake/modules/FindPsiPluginsApi.cmake DESTINATION ${DATA_INSTALL_DIR}/cmake/Modules)
        endif()
    endif()
elseif(HAIKU)
    if(IS_PSIPLUS)
        set(SHARE_SUFF "data/psi-plus")
        install(FILES ${PROJECT_SOURCE_DIR}/iconsets/system/default/psiplus/logo_128.png DESTINATION ${CMAKE_INSTALL_PREFIX}/data/pixmaps RENAME psi-plus.png)
    else()
        set(SHARE_SUFF "data/psi")
        install(FILES ${PROJECT_SOURCE_DIR}/iconsets/system/default/logo_128.png DESTINATION ${CMAKE_INSTALL_PREFIX}/data/pixmaps RENAME psi.png)
    endif()
    install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX})
    install(FILES ${PROJECT_SOURCE_DIR}/client_icons.txt DESTINATION ${CMAKE_INSTALL_PREFIX}/${SHARE_SUFF})

    install(DIRECTORY ${OTHER_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX}/${SHARE_SUFF})
    install(DIRECTORY ${PROJECT_SOURCE_DIR}/iconsets DESTINATION ${CMAKE_INSTALL_PREFIX}/${SHARE_SUFF} REGEX ${EXCLUDE_REGEXP} EXCLUDE)
    if(IS_WEBKIT OR IS_WEBENGINE)
        install(DIRECTORY ${PROJECT_SOURCE_DIR}/themes DESTINATION ${CMAKE_INSTALL_PREFIX}/${SHARE_SUFF})
    endif()
    if(LANGS_EXISTS)
        install(FILES ${QM} DESTINATION ${CMAKE_INSTALL_PREFIX}/${SHARE_SUFF}/translations)
    endif()
elseif(WIN32)
    install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX})
    if(NOT ONLY_BINARY AND INSTALL_EXTRA_FILES)
        install(FILES ${PROJECT_SOURCE_DIR}/client_icons.txt DESTINATION ${CMAKE_INSTALL_PREFIX})
        install(DIRECTORY ${OTHER_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX})
        install(DIRECTORY ${PROJECT_SOURCE_DIR}/iconsets DESTINATION ${CMAKE_INSTALL_PREFIX} REGEX ${EXCLUDE_REGEXP} EXCLUDE)
        if(IS_WEBKIT OR IS_WEBENGINE)
            install(DIRECTORY ${PROJECT_SOURCE_DIR}/themes DESTINATION ${CMAKE_INSTALL_PREFIX})
        endif()
        if(LANGS_EXISTS)
            install(FILES ${QM} DESTINATION ${CMAKE_INSTALL_PREFIX}/translations)
        endif()
    endif()
    if(USE_HUNSPELL AND EXISTS "${PROJECT_SOURCE_DIR}/myspell")
        copy("${PROJECT_SOURCE_DIR}/myspell/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/myspell/" prepare-bin)
    endif()
    if(ENABLE_PORTABLE OR DEV_MODE)
        include(${PROJECT_SOURCE_DIR}/cmake/modules/win32-prepare-deps.cmake)
    endif()
elseif(APPLE)
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

    # Special hack for QtWebEngineProcess executable from Homebrew
    if(IS_WEBENGINE AND HOMEBREW)
        set(QtWebEngineProcess_path "Versions/5/Helpers/QtWebEngineProcess.app")
        set(QtWebEngineProcess_file "${QtWebEngineProcess_path}/Contents/MacOS/QtWebEngineProcess")
        set(QtWebEngineCore_path "${CLIENT_NAME}.app/Contents/Frameworks/QtWebEngineCore.framework")
        set(MACDEPLOYQT_EXTRA_OPTS "-executable=${QtWebEngineCore_path}/${QtWebEngineProcess_file}")
    else()
        set(MACDEPLOYQT_EXTRA_OPTS "")
    endif()
    # end of QtWebEngineProcess hack

    # Simple and smart hack for using of macdeployqt to fix Psi plugins
    if(ENABLE_PLUGINS AND (NOT ONLY_BINARY))
        install(DIRECTORY "${CMAKE_BINARY_DIR}/psi/plugins"
                DESTINATION "${CLIENT_NAME}.app/Contents/PlugIns")
    endif()
    # end of plugins hack

    # Create a deployable application bundle that contains the all necessary libraries
    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST
                          ${PROJECT_SOURCE_DIR}/mac/Info.plist.in)
    install(CODE "execute_process(COMMAND ${MACDEPLOYQT} ${CLIENT_NAME}.app -verbose=2 \
                                                         ${MACDEPLOYQT_EXTRA_OPTS}
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
            install(FILES ${MACOSX_ICON} DESTINATION "${PROJECT_OUTPUT_DATA_DIR}")
            install(FILES ${PROJECT_SOURCE_DIR}/client_icons.txt DESTINATION "${PROJECT_OUTPUT_DATA_DIR}")
            install(DIRECTORY ${OTHER_FILES} DESTINATION "${PROJECT_OUTPUT_DATA_DIR}")
            install(DIRECTORY ${PROJECT_SOURCE_DIR}/iconsets DESTINATION ${PROJECT_OUTPUT_DATA_DIR} REGEX ${EXCLUDE_REGEXP} EXCLUDE)
            if(IS_WEBKIT OR IS_WEBENGINE)
                install(DIRECTORY ${PROJECT_SOURCE_DIR}/themes DESTINATION "${PROJECT_OUTPUT_DATA_DIR}")
            endif()
            if(LANGS_EXISTS)
                install(FILES ${QM} DESTINATION "${PROJECT_OUTPUT_DATA_DIR}/translations")
            endif()
            if(USE_HUNSPELL AND EXISTS "${PROJECT_SOURCE_DIR}/myspell")
                install(DIRECTORY "${PROJECT_SOURCE_DIR}/myspell" DESTINATION "${PROJECT_OUTPUT_DATA_DIR}")
            endif()
        endif()
    endif()
endif()
#INSTALL SECTION END

if(ENABLE_PLUGINS AND (NOT ONLY_BINARY))
    add_subdirectory(plugins)
endif()

