# CPACK packaging support for Linux-like systems.
# Included from src/CMakeLists.txt after the project metadata and install rules
# have been defined.

find_program(PSI_CPACK_DPKG_DEB_EXECUTABLE dpkg-deb)
find_program(PSI_CPACK_RPMBUILD_EXECUTABLE rpmbuild)

if(PSI_PLUS)
    set(CPACK_PACKAGE_NAME "psi-plus")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Psi+ XMPP client")
else()
    set(CPACK_PACKAGE_NAME "psi")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Psi XMPP client")
endif()

if(NOT DEFAULT_VER)
    set(DEFAULT_VER "1.5")
endif()

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

set(_PSI_CPACK_GENERATORS)

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

if(_PSI_CPACK_GENERATORS)
    set(CPACK_GENERATOR "${_PSI_CPACK_GENERATORS}")
    if(ENABLE_PLUGINS AND PSI_CPACK_DPKG_DEB_EXECUTABLE)
        list(APPEND CPACK_DEBIAN_PLUGINS_PACKAGE_DEPENDS "${CPACK_PACKAGE_NAME}-im")
    endif()
    if(LANGS_EXISTS AND PSI_CPACK_DPKG_DEB_EXECUTABLE)
        list(APPEND CPACK_DEBIAN_L10N_PACKAGE_DEPENDS "${CPACK_PACKAGE_NAME}-im")
    endif()
    if(INSTALL_PLUGINS_SDK AND PSI_CPACK_DPKG_DEB_EXECUTABLE)
        list(APPEND CPACK_DEBIAN_DEV_PACKAGE_DEPENDS "${CPACK_PACKAGE_NAME}-im")
    endif()

    include(CPackComponent)
    set(CPACK_COMPONENTS_ALL im)
    cpack_add_component(im
        DISPLAY_NAME "Main"
        DESCRIPTION "Main program files"
        REQUIRED
    )
    if(ENABLE_PLUGINS)
        list(APPEND CPACK_COMPONENTS_ALL plugins)
        cpack_add_component(plugins
            DISPLAY_NAME "Plugins"
            DESCRIPTION "Useful plugins for program"
        )
        message(STATUS "CPack: plugins included as plugins component")
    endif()
    if(EXISTS "${TRANSLATIONS_DIR}")
        list(APPEND CPACK_COMPONENTS_ALL "l10n")
        cpack_add_component(l10n
            DISPLAY_NAME "Translations"
            DESCRIPTION "Program translation files"
        )
        message(STATUS "CPack: translations included as l10n component")
    endif()
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
unset(PSI_CPACK_DPKG_DEB_EXECUTABLE)
unset(PSI_CPACK_RPMBUILD_EXECUTABLE)
