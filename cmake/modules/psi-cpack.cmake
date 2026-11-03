# CPACK packaging support for Linux-like systems.
# Included from CMakeLists.txt

find_program(PSI_CPACK_DPKG_DEB_EXECUTABLE dpkg-deb)
find_program(PSI_CPACK_RPMBUILD_EXECUTABLE rpmbuild)
set(HOMEDIR "$ENV{HOME}")
find_program(CPACK_APPIMAGE_TOOL_EXECUTABLE "${HOMEDIR}/AppImages/appimagetool.appimage")
find_program(CPACK_APPIMAGE_PATCHELF_EXECUTABLE patchelf)

include(GNUInstallDirs)

set(_CPACK_PACKAGE_NAME "${PROJECT_NAME}")
if(CPACK_VERBOSED_NAME)
    set(_CPACK_PACKAGE_NAME "${CPACK_VERBOSED_NAME}")
endif()

set(CPACK_PACKAGE_NAME ${_CPACK_PACKAGE_NAME})

if(PSI_PLUS)
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Psi+ XMPP client")
else()
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Psi XMPP client")
endif()

if(NOT DEFAULT_VER)
    set(DEFAULT_VER "1.5")
endif()

# Generate qt.conf function
function(cpack_generate_qconf _INSTALL_PATH)
    set(QTCONF_TEXT
    "
[Paths]
Prefix = .
Binaries = .
Libraries = ../${CMAKE_INSTALL_LIBDIR}
Plugins = ../${CMAKE_INSTALL_LIBDIR}/qt${QT_DEFAULT_MAJOR_VERSION}/plugins
Data = ../${CMAKE_INSTALL_DATAROOTDIR}
Translations = ../${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/translations
    "
    )
    file(WRITE "${CMAKE_BINARY_DIR}/qt.conf" "${QTCONF_TEXT}")
    install(
        FILES "${CMAKE_BINARY_DIR}/qt.conf"
        DESTINATION "${_INSTALL_PATH}"
    )
    unset(QTCONF_TEXT)
endfunction()
# Install additional files function
function(install_add_files _RES_FILE_NAME _RES_FILE_SEARCH_PATH _RES_INSTALL_PATH)
    file(GLOB _RESOURCES
        "${_RES_FILE_SEARCH_PATH}/${_RES_FILE_NAME}"
    )
    foreach(_resource IN LISTS _RESOURCES)
        if(EXISTS "${_resource}")
            install(
                FILES "${_resource}"
                DESTINATION
                "${_RES_INSTALL_PATH}"
            )
        endif()
    endforeach()
    unset(_RESOURCES)
endfunction()

set(CPACK_PACKAGE_VENDOR "psi-im.org")
set(CPACK_PACKAGE_CONTACT "psi-im.org")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://psi-im.org/")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/COPYING")
set(CPACK_DESCRIPTION_SUFFIX "designed for experienced users.")

# PSI_VERSION can contain a date or revision suffix. CPack package versions
# must be numeric, so retain only its numeric prefix.
set(_PSI_CPACK_VERSION "${CPACK_PSI_VERSION}")
string(REGEX MATCH "^[0-9]+(\\.[0-9]+)*" _PSI_CPACK_VERSION "${_PSI_CPACK_VERSION}")
if(NOT _PSI_CPACK_VERSION)
    set(_PSI_CPACK_VERSION "${DEFAULT_VER}.0")
endif()
set(CPACK_PACKAGE_VERSION "${_PSI_CPACK_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")

# Create custom config file to separate DEB and RPM build from AppImage and DragNDrop
set(CPACK_CUSTOM_CONFIG_FILE "${CMAKE_CURRENT_BINARY_DIR}/CPackCustomConfig.cmake")
file(WRITE "${CPACK_CUSTOM_CONFIG_FILE}" "
if(CPACK_GENERATOR STREQUAL \"AppImage\")
    set(CPACK_COMPONENTS_GROUPING \"ALL_COMPONENTS_IN_ONE\")
    set(CPACK_MONOLITHIC_INSTALL 1)
    set(CPACK_SET_DESTDIR ON)
    set(CPACK_PACKAGING_INSTALL_PREFIX \"/\")
elseif(CPACK_GENERATOR STREQUAL \"DragNDrop\")
    set(CPACK_COMPONENTS_GROUPING \"ALL_COMPONENTS_IN_ONE\")
    set(CPACK_MONOLITHIC_INSTALL 1)
else()
    set(CPACK_COMPONENTS_GROUPING \"ONE_PER_GROUP\")
    set(CPACK_MONOLITHIC_INSTALL 0)
endif()
")
set(CPACK_PROJECT_CONFIG_FILE "${CPACK_CUSTOM_CONFIG_FILE}")

set(_PSI_CPACK_GENERATORS)
# Debian CPack generator
if(PSI_CPACK_DPKG_DEB_EXECUTABLE)
    list(APPEND _PSI_CPACK_GENERATORS DEB)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEB_COMPONENT_INSTALL ON)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_ENABLE_COMPONENT_DEPENDS ON)
    set(CPACK_DEBIAN_PACKAGE_SECTION "net")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CPACK_PACKAGE_HOMEPAGE_URL}")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}, ${CPACK_DESCRIPTION_SUFFIX}")
    message(STATUS "CPack: DEB generator enabled (${PSI_CPACK_DPKG_DEB_EXECUTABLE})")
else()
    message(STATUS "CPack: dpkg-deb not found; DEB generator disabled")
endif()
# RPM CPack generator
if(PSI_CPACK_RPMBUILD_EXECUTABLE)
    list(APPEND _PSI_CPACK_GENERATORS RPM)
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
    set(CPACK_RPM_COMPONENT_INSTALL ON)
    set(CPACK_RPM_PACKAGE_LICENSE "GPL-2.0")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Internet")
    set(CPACK_RPM_PACKAGE_URL "${CPACK_PACKAGE_HOMEPAGE_URL}")
    set(CPACK_RPM_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}, ${CPACK_DESCRIPTION_SUFFIX}")
    # Let rpmbuild detect ELF and shared-library requirements automatically.
    set(CPACK_RPM_PACKAGE_AUTOREQ ON)
    message(STATUS "CPack: RPM generator enabled (${PSI_CPACK_RPMBUILD_EXECUTABLE})")
else()
    message(STATUS "CPack: rpmbuild not found; RPM generator disabled")
endif()
# AppImage CPack generator. Needs CMake >= 4.2
if(CMAKE_VERSION GREATER_EQUAL 4.2.0)
    if(CPACK_APPIMAGE_TOOL_EXECUTABLE AND CPACK_APPIMAGE_PATCHELF_EXECUTABLE)
        list(APPEND _PSI_CPACK_GENERATORS "AppImage")
        set(CPACK_PACKAGE_ICON "${CPACK_PSI_ICON}")
        # Create qt.conf file in bin catalog
        cpack_generate_qconf(${CMAKE_INSTALL_BINDIR})
        # Create custom AppRun
        file(WRITE "${CMAKE_BINARY_DIR}/AppRun.sh" "#!/bin/sh
HERE=\"$(dirname \"$(readlink -f \"\${0}\")\")\"

export GST_PLUGIN_PATH=\"\${HERE}/usr/${CMAKE_INSTALL_LIBDIR}/gstreamer-1.0\"
# Path to the libraries inside of AppImage
export LD_LIBRARY_PATH=\"\${HERE}/usr/${CMAKE_INSTALL_LIBDIR}:\${LD_LIBRARY_PATH}\"

# Run binary file
exec \"\${HERE}/usr/bin/${CPACK_PACKAGE_NAME}\" \"$@\"
"
        )
        install(CODE "
file(INSTALL
    FILES \"${CMAKE_BINARY_DIR}/AppRun.sh\"
    DESTINATION \"\${CMAKE_INSTALL_PREFIX}/..\"
    RENAME \"AppRun\"
    PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
)
"
        )
        include(psi-linux-deploy)
        install_appimage_runtime_deps()
        message(STATUS "CPack: AppImage generator added")
    endif()
endif()

# DragNDrop CPack generator for macOS
if(APPLE)
    list(APPEND _PSI_CPACK_GENERATORS "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")
    set(CPACK_DMG_DS_STORE_DIR "${CMAKE_SOURCE_DIR}")
    set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_SOURCE_DIR}/src/iconsets/system/default/psi_icon.png")
    set(CPACK_DMG_FORMAT "UDZO")
    set(CPACK_DMG_FILESYSTEM "HFS+")
    # Set icon for the .dmg file if available
    if(CPACK_PSI_ICON)
        set(CPACK_DMG_ICON "${CPACK_PSI_ICON}")
    endif()
    # Configure DMG window appearance
    set(CPACK_DMG_WINDOW_STYLE "styled")
    # Directories where fixup_bundle will search for libraries.
    # CMAKE_BINARY_DIR — the project build directory.
    # Additional directories can be added here, for example:
    # /opt/homebrew/lib
    # /usr/local/lib
    set(APP_LIBRARY_DIRS
        "${CMAKE_BINARY_DIR}"
        "/opt/homebrew/lib"
        "/usr/local/lib"
    )
    # It is important to escape variables that need to be evaluated
    # during install/CPack, rather than during configure.
    # Search for additional libraries
    install(CODE "
        include(BundleUtilities)
        set(BU_CHMOD_BUNDLE_ITEMS TRUE)
        fixup_bundle(
            \"\\\$ENV{DESTDIR}\\\${CMAKE_INSTALL_PREFIX}/${CPACK_PACKAGE_NAME}\"
            \"\"
            \"${APP_LIBRARY_DIRS}\"
        )
    ")
    # Deploy Qt libs and plugins
    find_program(CPACK_MACDEPLOYQT_BIN macdeployqt)
    install(CODE "
        execute_process(
            COMMAND ${CPACK_MACDEPLOYQT_BIN}
            \"\\\$ENV{DESTDIR}\\\${CMAKE_INSTALL_PREFIX}/${CPACK_PACKAGE_NAME}\"
            RESULT_VARIABLE MACDEPLOYQT_RESULT
        )
        if(NOT MACDEPLOYQT_RESULT EQUAL 0)
            message(FATAL_ERROR \"macdeployqt failed\")
        endif()
    ")
    message(STATUS "CPack: DragNDrop generator enabled for macOS")
endif()

if(_PSI_CPACK_GENERATORS)
    set(CPACK_GENERATOR "${_PSI_CPACK_GENERATORS}")
    #Set psi-im/psi-plus-im as a dependency for all other components
    if(PSI_CPACK_DPKG_DEB_EXECUTABLE)
        if(NOT BUNDLED_IRIS)
            set(_iris_pkg_version "${Iris_VERSION}")
            if(PSI_IRIS_MIN_VERSION)
                set(_iris_pkg_version "${PSI_IRIS_MIN_VERSION}")
            endif()
            list(APPEND CPACK_DEBIAN_IM_PACKAGE_DEPENDS "libiris-qt6-1 (>=${_iris_pkg_version})")
        endif()
        if(ENABLE_PLUGINS)
            list(APPEND CPACK_DEBIAN_PLUGINS_PACKAGE_DEPENDS "${CPACK_PACKAGE_NAME}-im")
            if(BUILD_PSIMEDIA)
                list(APPEND CPACK_DEBIAN_PLUGINS_PACKAGE_DEPENDS
                "gstreamer1.0-plugins-bad"
                "gstreamer1.0-plugins-base"
                "gstreamer1.0-plugins-good"
                )
            endif()
        endif()
        if(TRANSLATIONS_DIR AND EXISTS "${TRANSLATIONS_DIR}")
            list(APPEND CPACK_DEBIAN_L10N_PACKAGE_DEPENDS "${CPACK_PACKAGE_NAME}-im")
        endif()
        if(INSTALL_PLUGINS_SDK)
            list(APPEND CPACK_DEBIAN_DEV_PACKAGE_DEPENDS "${CPACK_PACKAGE_NAME}-im")
        endif()
    endif()
    #Create Main component - im
    include(CPackComponent)
    set(CPACK_COMPONENTS_ALL im)
    cpack_add_component(im
        DISPLAY_NAME "Main"
        DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION_SUMMARY} main program files"
        REQUIRED
    )
    #Create plugins component - plugins
    if(ENABLE_PLUGINS)
        list(APPEND CPACK_COMPONENTS_ALL plugins)
        cpack_add_component(plugins
            DISPLAY_NAME "Plugins"
            DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION_SUMMARY} useful plugins"
        )
        message(STATUS "CPack: plugins included as plugins component")
    endif()
    #Create tarnslations component - l10n
    if(EXISTS "${TRANSLATIONS_DIR}")
        list(APPEND CPACK_COMPONENTS_ALL "l10n")
        cpack_add_component(l10n
            DISPLAY_NAME "Translations"
            DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION_SUMMARY} translation files"
        )
        message(STATUS "CPack: translations included as l10n component")
    endif()
    #Create Plugins SDK component - dev
    if(INSTALL_PLUGINS_SDK)
        list(APPEND CPACK_COMPONENTS_ALL "dev")
        cpack_add_component(dev
            DISPLAY_NAME "SDK"
            DESCRIPTION "Plugins SDK"
        )
        message(STATUS "CPack: plugins SDK included as dev component")
    endif()
else()
    message(WARNING "USE_CPACK is enabled, but neither dpkg-deb nor rpmbuild was found")
endif()

include(CPack)

unset(_PSI_CPACK_GENERATORS)
unset(_PSI_CPACK_VERSION)
unset(_CPACK_PACKAGE_NAME)
unset(PSI_CPACK_DPKG_DEB_EXECUTABLE)
unset(PSI_CPACK_RPMBUILD_EXECUTABLE)
