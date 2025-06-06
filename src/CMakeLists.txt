set(CMAKE_CXX_STANDARD 20)

list(APPEND CMAKE_MODULE_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/cmake/modules" )
set(CMAKE_AUTOMOC ON)

if(LINUX)
    set(LIB_SUFFIX "" CACHE STRING "Define suffix of directory name (32/64)")
    if(DEV_MODE)
        set(PSI_LIBDIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
        set(PSI_DATADIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    else()
        set(PSI_LIBDIR "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}/${PROJECT_NAME}" CACHE STRING "Path to Psi/Psi+ libraries directory")
        set(PSI_DATADIR "${CMAKE_INSTALL_PREFIX}/share/${PROJECT_NAME}" CACHE STRING "Path to Psi/Psi+ data directory")
    endif()
endif()

if(APPLE AND NOT PSI_LIBDIR)
    if(DEV_MODE)
        set(PSI_LIBDIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins")
    else()
        set(PSI_LIBDIR "/Applications/${CLIENT_NAME}.app/Contents/Resources/plugins")
    endif()
endif()

set(TRANSLATIONS_DIR "${PROJECT_SOURCE_DIR}/translations" CACHE STRING "Path to Psi/Psi+ translations directory")

#Work with external packages
set(REQUIRED_QT_COMPONENTS Widgets Core Gui Xml Network Concurrent Multimedia Svg Sql)
if(QT_DEFAULT_MAJOR_VERSION GREATER_EQUAL 6)
    list(APPEND REQUIRED_QT_COMPONENTS SvgWidgets)
endif()
if(IS_WEBKIT)
    list(APPEND REQUIRED_QT_COMPONENTS WebKit WebKitWidgets)
    set(WEBKIT_LIBS Qt${QT_DEFAULT_MAJOR_VERSION}::WebKit Qt${QT_DEFAULT_MAJOR_VERSION}::WebKitWidgets)
elseif(IS_WEBENGINE)
    if(QT_DEFAULT_MAJOR_VERSION GREATER_EQUAL 6)
        set(CORE_SUFFIX Core)
    endif()
    list(APPEND REQUIRED_QT_COMPONENTS WebEngine${CORE_SUFFIX} WebEngineWidgets WebChannel)
    list(APPEND WEBKIT_LIBS Qt${QT_DEFAULT_MAJOR_VERSION}::WebEngine${CORE_SUFFIX} Qt${QT_DEFAULT_MAJOR_VERSION}::WebEngineWidgets Qt${QT_DEFAULT_MAJOR_VERSION}::WebChannel)
endif()

if(LINUX)
    if(USE_DBUS)
        list(APPEND REQUIRED_QT_COMPONENTS DBus)
    endif()
    if(USE_X11 OR LIMIT_X11_USAGE)
        if(QT_DEFAULT_MAJOR_VERSION LESS 6)
            list(APPEND REQUIRED_QT_COMPONENTS X11Extras)
        endif()
        find_package(XCB REQUIRED)
        set(EXTRA_LIBS
            ${XCB_LIBRARY}
        )
        include_directories(${LIBXCB_INCLUDE_DIR})
        add_definitions(${LIBXCB_DEFINITIONS})
    endif()
endif()

if(APPLE)
    find_package(Qt${QT_DEFAULT_MAJOR_VERSION}MacExtras REQUIRED)
    if(USE_SPARKLE)
        find_package(Sparkle REQUIRED)
        include_directories(${SPARKLE_INCLUDE_DIR})
        add_definitions(-DHAVE_SPARKLE)
    endif()
endif()

if(NOT ONLY_BINARY AND (EXISTS "${TRANSLATIONS_DIR}"))
    set(LANGS_EXISTS ON)
    message(STATUS "${PROJECT_NAME} translations found at ${TRANSLATIONS_DIR}")
    list(APPEND REQUIRED_QT_COMPONENTS LinguistTools)
    set(QT_LUPDATE_EXECUTABLE ${Qt${QT_DEFAULT_MAJOR_VERSION}_LUPDATE_EXECUTABLE})
endif()

if(QT_DEFAULT_MAJOR_VERSION LESS "6")
    find_package(Qt${QT_DEFAULT_MAJOR_VERSION} 5.10 REQUIRED COMPONENTS ${REQUIRED_QT_COMPONENTS})
else()
    find_package(Qt${QT_DEFAULT_MAJOR_VERSION} 6.6 REQUIRED COMPONENTS ${REQUIRED_QT_COMPONENTS})
endif()
if(USE_KEYCHAIN)
    if(NOT BUNDLED_KEYCHAIN)
        find_package(Qt${QT_DEFAULT_MAJOR_VERSION}Keychain REQUIRED)
    else()
        include(qtkeychain-bundled)
    endif()
    set(KEYCHAIN_LIBS ${QTKEYCHAIN_LIBRARIES})
    include_directories(${QTKEYCHAIN_INCLUDE_DIRS})
    add_definitions(-DHAVE_KEYCHAIN)
endif()
set(QT_LIBRARIES
    Qt${QT_DEFAULT_MAJOR_VERSION}::Widgets
    Qt${QT_DEFAULT_MAJOR_VERSION}::Core
    Qt${QT_DEFAULT_MAJOR_VERSION}::Network
    Qt${QT_DEFAULT_MAJOR_VERSION}::Xml
    ${WEBKIT_LIBS}
    Qt${QT_DEFAULT_MAJOR_VERSION}::Concurrent
    Qt${QT_DEFAULT_MAJOR_VERSION}::Multimedia
    Qt${QT_DEFAULT_MAJOR_VERSION}::Svg
    Qt${QT_DEFAULT_MAJOR_VERSION}::Sql
    ${KEYCHAIN_LIBS}
)
if(QT_DEFAULT_MAJOR_VERSION GREATER_EQUAL 6)
    list(APPEND QT_LIBRARIES Qt${QT_DEFAULT_MAJOR_VERSION}::SvgWidgets)
endif()


if(LINUX)
    if(USE_DBUS)
        list(APPEND QT_LIBRARIES Qt${QT_DEFAULT_MAJOR_VERSION}::DBus)
    endif()
    if(USE_X11 OR LIMIT_X11_USAGE)
        if(QT_DEFAULT_MAJOR_VERSION LESS 6)
            list(APPEND QT_LIBRARIES Qt${QT_DEFAULT_MAJOR_VERSION}::X11Extras)
        endif()
        list(APPEND QT_LIBRARIES ${LIBXCB_LIBRARIES})
    endif()
endif()
if(UNIX OR IS_WEBENGINE)
    list(APPEND EXTRA_LIBS qhttp)
endif()
if(APPLE)
    list(APPEND QT_LIBRARIES Qt${QT_DEFAULT_MAJOR_VERSION}::MacExtras)
    if(USE_SPARKLE)
        list(APPEND EXTRA_LDFLAGS ${SPARKLE_LIBRARY})
    endif()
    list(APPEND EXTRA_LDFLAGS "-framework Carbon" "-framework AppKit" "-framework IOKit" "-framework CoreAudio" "-framework AudioToolbox")
endif()

if (NOT IRIS_BUNDLED_QCA)
    find_package( Qca REQUIRED )
    include_directories(${Qca_INCLUDE_DIR})
else()
    find_package(OpenSSL REQUIRED)
    include_directories(${Qca_INCLUDE_DIR_EXPORT})
    set(Qca_LIBRARY ${Qca_LIBRARY_EXPORT} OpenSSL::SSL OpenSSL::Crypto)
    #Additional win32 link dependencies
    if(WIN32 OR USE_MXE)
        list(APPEND Qca_LIBRARY crypt32 ws2_32)
    endif()
endif()

if(LINUX AND (USE_X11 OR LIMIT_X11_USAGE))
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
    ${PROJECT_SOURCE_DIR}/3rdparty/qite/libqite
)

if(NOT BUNDLED_IRIS)
    include_directories(${Iris_INCLUDE_DIR})
endif()

#Prepare and generate config.h file
string(TIMESTAMP PSI_COMPILATION_DATE "%Y-%m-%d" UTC)
string(TIMESTAMP PSI_COMPILATION_TIME "%H:%M:%S" UTC)

if(IS_WEBKIT)
    set(PSI_VER_SUFFIX ", webkit")
elseif(IS_WEBENGINE)
    set(PSI_VER_SUFFIX ", webengine")
endif()

if( NOT PSI_PLUS )
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

include(get-version)

if(LINUX)
    if(PSI_PLUS)
        set(SHARE_SUFF "psi-plus")
    else()
        set(SHARE_SUFF "psi")
    endif()
elseif(HAIKU)
    if(PSI_PLUS)
        set(SHARE_SUFF "data/psi-plus")
    else()
        set(SHARE_SUFF "data/psi")
    endif()
endif()

if(LINUX)
    message(STATUS "${CLIENT_NAME} libraries directory: ${PSI_LIBDIR}")
    message(STATUS "${CLIENT_NAME} data directory: ${PSI_DATADIR}")
elseif(APPLE)
    message(STATUS "${CLIENT_NAME} libraries directory: ${PSI_LIBDIR}")
endif()

set(CONFIG_OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/psi_config.h")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/psi_config.h.in ${CONFIG_OUTPUT_FILE} @ONLY)

prepare_iconsets(prepare-src)
#Generate iconsets.qrc and icondef.xml
set(ICONDEFXML_OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/iconsets/system/default/icondef.xml")
set(ICONSETSQRC_OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/iconsets.qrc")
configure_file(${PROJECT_SOURCE_DIR}/icondef.xml.in ${ICONDEFXML_OUTPUT_FILE} @ONLY)
configure_file(${PROJECT_SOURCE_DIR}/iconsets.qrc.in ${ICONSETSQRC_OUTPUT_FILE} @ONLY)

#Include SOURCES HEADERS FORMS
include(src.cmake)

#Append psi_config.h to HEADERS list
if(EXISTS "${CONFIG_OUTPUT_FILE}")
    message(STATUS "${CONFIG_OUTPUT_FILE} file generated")
    list(APPEND HEADERS ${CONFIG_OUTPUT_FILE})
endif()

# Only headers or very small sources
include(irisprotocol/irisprotocol.cmake)
include(protocol/protocol.cmake)
include(${PROJECT_SOURCE_DIR}/plugins/plugins.cmake)

#Static libs
add_subdirectory(AutoUpdater)
add_subdirectory(options)
add_subdirectory(tabs)
add_subdirectory(privacy)
add_subdirectory(Certificates)
add_subdirectory(avcall)
add_subdirectory(psimedia)
add_subdirectory(contactmanager)
add_subdirectory(tools)
add_subdirectory(widgets)
add_subdirectory(sxe)
add_subdirectory(whiteboarding)
if(APPLE)
    add_subdirectory(CocoaUtilities)
endif()

#Adds qhttp if needed
if(UNIX OR IS_WEBENGINE)
    add_definitions(-DHAVE_WEBSERVER)
    find_package(HttpParser 2.2 QUIET)
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

#Work with project translation files
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
    Qt_add_translation(QM ${PSI_TRANSLATIONS})

    if(WIN32 OR APPLE)
        # Qt translations
        get_target_property(QtCore_location Qt${QT_DEFAULT_MAJOR_VERSION}::Core LOCATION)
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
if(IS_WEBKIT OR IS_WEBENGINE)
    list(APPEND RESOURCES ${PROJECT_SOURCE_DIR}/themes/chatview.qrc)
endif()

Qt_add_resources(QRC_SOURCES ${RESOURCES})
qt_wrap_ui(UI_FORMS ${FORMS})
#Small hack to compile ui files before some *.cpp files
add_custom_target(build_ui_files DEPENDS "${UI_FORMS}")

#Prepare and comiple win rc file
if(WIN32)
    if(PSI_PLUS)
        set(APP_NAME "Psi+")
    else()
        set(APP_NAME "Psi")
    endif()
    set(APP_COPYRIGHT "GNU GPL v2")
    set(APP_DESCRIPTION "${APP_NAME} XMPP client")
    set(APP_COMMENTS "${APP_NAME} : A cross-platform XMPP client designed for experienced users.")
    if(PSI_PLUS)
        set(COMPANY_NAME "Psi+ project")
        set(APP_ICON "app-plus.ico")
    else()
        set(COMPANY_NAME "psi-im.org")
        set(APP_ICON "app.ico")
    endif()
    string(REPLACE "\." "," FILE_VERSION ${APP_VERSION})
    if(NOT IS_WIN64)
        set(MANIFEST_ARCH "X86")
    else()
        set(MANIFEST_ARCH "amd64")
    endif()
    # win resource creation
    if(${QT_DEFAULT_MAJOR_VERSION} LESS 6)
        configure_file(${PROJECT_SOURCE_DIR}/win32/psi_win.rc.in ${CMAKE_CURRENT_BINARY_DIR}/psi_win.rc @ONLY)
        #configure_file(${PROJECT_SOURCE_DIR}/win32/psi.manifest.in ${CMAKE_CURRENT_BINARY_DIR}/psi.manifest @ONLY)
        # win resource compilation
        set(DECS_RC_NAME ${CMAKE_CURRENT_BINARY_DIR}/psi_win.rc)
        set(RC_FILE ${CMAKE_CURRENT_BINARY_DIR}/psi_win.o)
        compile_rc_file(${DECS_RC_NAME} ${RC_FILE})
    endif()
endif()

#Main executable creation
add_executable(${PROJECT_NAME}
    WIN32
    MACOSX_BUNDLE
    ${SOURCES}
    ${HEADERS}
    ${UI_FORMS}
    ${QRC_SOURCES}
    ${RC_FILE}
)

#Generate .rc file for windows with Qt6
if(WIN32 AND (${QT_DEFAULT_MAJOR_VERSION} GREATER_EQUAL 6))
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
        QT_TARGET_VERSION "${APP_VERSION}"
        QT_TARGET_COMPANY_NAME "${COMPANY_NAME}"
        QT_TARGET_DESCRIPTION "${APP_DESCRIPTION}"
        QT_TARGET_COPYRIGHT "${APP_COPYRIGHT}"
        QT_TARGET_PRODUCT_NAME "${APP_NAME}"
        QT_TARGET_COMMENTS "${APP_COMMENTS}"
        QT_TARGET_INTERNALNAME "${APP_NAME}"
        QT_TARGET_RC_ICONS "${PROJECT_SOURCE_DIR}/win32/${APP_ICON}"
    )
    _qt_internal_generate_win32_rc_file(${PROJECT_NAME})
endif()

include(fix-codestyle)

#Add webkit/webengine suffix to output binary name
if(VERBOSE_PROGRAM_NAME)
    if(IS_WEBKIT)
        set(VERBOSE_SUFFIX "-webkit")
    elseif(IS_WEBENGINE)
        set(VERBOSE_SUFFIX "-webengine")
    endif()
endif()

#Add portable suffix to output binary name
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

#Build dependencies before project
if(LANGS_EXISTS)
    add_dependencies(${PROJECT_NAME} build_translations)
endif()
add_dependencies(${PROJECT_NAME} iris)
if(UNIX OR IS_WEBENGINE)
    add_dependencies(${PROJECT_NAME} qhttp)
endif()
add_dependencies(${PROJECT_NAME} build_ui_files)

if(BUNDLED_KEYCHAIN)
    add_dependencies(${PROJECT_NAME} QtkeychainProject)
endif()

#Add win32 additional dependencies
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
    Certificates
    psimedia
    contactmanager
    avcall
    whiteboarding
    sxe
    tools
    widgets
    privacy
    tabs
    options
    AutoUpdater
)

if(APPLE)
    target_link_libraries(${PROJECT_NAME} CocoaUtilities)
endif()

#Privare Psi project defenitions
if(LINUX)
    if(NOT VERBOSED_NAME)
        set(VERBOSED_NAME ${PROJECT_NAME})
    endif()
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        HAVE_FREEDESKTOP
        APP_PREFIX=${CMAKE_INSTALL_PREFIX}
        APP_BIN_NAME=${VERBOSED_NAME}
    )
    #Set compile definions for options static library
    target_compile_definitions(options PRIVATE
        HAVE_FREEDESKTOP
        APP_BIN_NAME=${VERBOSED_NAME}
    )
endif()

#TaskbarNotifier definition
if(USE_TASKBARNOTIFIER)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_TASKBARNOTIFIER)
endif()
if(USE_CRASH)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_CRASH)
endif()
if(LINUX AND USE_X11)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_X11)
    if(LIMIT_X11_USAGE)
        target_compile_definitions(${PROJECT_NAME} PRIVATE LIMIT_X11_USAGE)
    endif()
endif()
if(IS_WEBKIT OR IS_WEBENGINE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WEBKIT)
    if(IS_WEBENGINE)
        target_compile_definitions(${PROJECT_NAME} PRIVATE WEBENGINE)
    endif()
endif()
if(ISDEBUG AND CHATVIEW_CORRECTION_DEBUG)
    target_compile_definitions(${PROJECT_NAME} PRIVATE CORRECTION_DEBUG)
endif()

#Pre-install section
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

set(APP_DATA_FILE
    "${PROJECT_SOURCE_DIR}/linux/${PROJECT_NAME}.appdata.xml"
)

#PREPARE SECTION
copy("${PROJECT_SOURCE_DIR}/certs/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/certs/" prepare-bin)
copy("${PROJECT_SOURCE_DIR}/sound/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sound/" prepare-bin)
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
        include(generate_desktopfile)
        install(FILES ${OUT_DESK_FILE} DESTINATION ${APPS_INSTALL_DIR})
        if(PSI_PLUS)
            set(PSI_LOGO_PREFIX ${PROJECT_SOURCE_DIR}/iconsets/system/default/psiplus)
        else()
            set(PSI_LOGO_PREFIX ${PROJECT_SOURCE_DIR}/iconsets/system/default)
        endif()
        #Install logo and pixmaps
        install(FILES ${PSI_LOGO_PREFIX}/logo_128.png DESTINATION ${PIXMAPS_INSTALL_DIR} RENAME ${VERBOSED_NAME}.png)
        install(FILES ${APP_DATA_FILE} DESTINATION "${DATA_INSTALL_DIR}/metainfo")
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
            configure_file(${PROJECT_SOURCE_DIR}/plugins/pluginsconf.pri.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/pluginsconf.pri @ONLY)
            configure_file(${PROJECT_SOURCE_DIR}/plugins/variables.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/variables.cmake @ONLY)
            install(FILES ${PLUGINS_INCLUDES} DESTINATION ${API_INCLUDES_DIR})
            install(FILES ${CMAKE_CURRENT_BINARY_DIR}/variables.cmake DESTINATION ${API_OUTPUT_DATA_DIR}/plugins)
            install(FILES ${PROJECT_SOURCE_DIR}/plugins/plugins.pri DESTINATION ${API_OUTPUT_DATA_DIR}/plugins)
            install(FILES ${PROJECT_SOURCE_DIR}/plugins/psiplugin.pri DESTINATION ${API_OUTPUT_DATA_DIR}/plugins)
            install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pluginsconf.pri DESTINATION ${API_OUTPUT_DATA_DIR}/plugins)
            install(FILES ${PROJECT_SOURCE_DIR}/cmake/modules/FindPsiPluginsApi.cmake DESTINATION ${DATA_INSTALL_DIR}/cmake/Modules)
        endif()
    endif()
elseif(HAIKU)
    if(PSI_PLUS)
        install(FILES ${PROJECT_SOURCE_DIR}/iconsets/system/default/psiplus/logo_128.png DESTINATION ${CMAKE_INSTALL_PREFIX}/data/pixmaps RENAME psi-plus.png)
    else()
        install(FILES ${PROJECT_SOURCE_DIR}/iconsets/system/default/logo_128.png DESTINATION ${CMAKE_INSTALL_PREFIX}/data/pixmaps RENAME psi.png)
    endif()
    install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX})
    install(FILES ${PROJECT_SOURCE_DIR}/client_icons.txt DESTINATION ${CMAKE_INSTALL_PREFIX}/${SHARE_SUFF})

    install(DIRECTORY ${OTHER_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX}/${SHARE_SUFF})
    install(DIRECTORY ${PROJECT_SOURCE_DIR}/iconsets DESTINATION ${CMAKE_INSTALL_PREFIX}/${SHARE_SUFF} REGEX ${EXCLUDE_REGEXP} EXCLUDE)
    if(LANGS_EXISTS)
        install(FILES ${QM} DESTINATION ${CMAKE_INSTALL_PREFIX}/${SHARE_SUFF}/translations)
    endif()
elseif(WIN32)
    install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX})
    if(NOT ONLY_BINARY AND INSTALL_EXTRA_FILES)
        install(FILES ${PROJECT_SOURCE_DIR}/client_icons.txt DESTINATION ${CMAKE_INSTALL_PREFIX})
        install(DIRECTORY ${OTHER_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX})
        install(DIRECTORY ${PROJECT_SOURCE_DIR}/iconsets DESTINATION ${CMAKE_INSTALL_PREFIX} REGEX ${EXCLUDE_REGEXP} EXCLUDE)
        if(LANGS_EXISTS)
            install(FILES ${QM} DESTINATION ${CMAKE_INSTALL_PREFIX}/translations)
        endif()
    endif()
    if(USE_HUNSPELL AND EXISTS "${PROJECT_SOURCE_DIR}/myspell")
        copy("${PROJECT_SOURCE_DIR}/myspell/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/myspell/" prepare-bin)
    endif()
    if(ENABLE_PORTABLE OR DEV_MODE)
        include(win32-prepare-deps)
    endif()
elseif(APPLE)
    include("${PROJECT_SOURCE_DIR}/mac/macos_definitions.cmake")

    set(PROJECT_OUTPUT_DATA_DIR "${CLIENT_NAME}.app/Contents/Resources")

    list(APPEND SOURCES ${MACOSX_ICON})
    set_source_files_properties(${MACOSX_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_property(TARGET ${PROJECT_NAME} PROPERTY OUTPUT_NAME "${CLIENT_NAME}")
    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "bin" BUNDLE DESTINATION ".")

    # Special hack for qca package from Homebrew
    if(HOMEBREW AND NOT IRIS_BUNDLED_QCA)
        install(DIRECTORY "${HOMEBREW}/opt/qca/lib/Qt${QT_DEFAULT_MAJOR_VERSION}/plugins/crypto"
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
