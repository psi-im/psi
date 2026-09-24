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

set(CPACK_PACKAGE_VENDOR "psi-im.org")
set(CPACK_PACKAGE_CONTACT "psi-im.org")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://psi-im.org/")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/COPYING")

# PSI_VERSION can contain a date or revision suffix. CPack package versions
# must be numeric, so retain only its numeric prefix.
set(_PSI_CPACK_VERSION "${PSI_VERSION}")
string(REGEX MATCH "^[0-9]+(\\.[0-9]+)*" _PSI_CPACK_VERSION "${_PSI_CPACK_VERSION}")
if(NOT _PSI_CPACK_VERSION)
    set(_PSI_CPACK_VERSION "0.0.0")
endif()
set(CPACK_PACKAGE_VERSION "${_PSI_CPACK_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")

set(_PSI_CPACK_GENERATORS)

if(PSI_CPACK_DPKG_DEB_EXECUTABLE)
    list(APPEND _PSI_CPACK_GENERATORS DEB)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_SECTION "net")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://psi-im.org/")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}, designed for experienced users.")
    message(STATUS "CPack: DEB generator enabled (${PSI_CPACK_DPKG_DEB_EXECUTABLE})")
else()
    message(STATUS "CPack: dpkg-deb not found; DEB generator disabled")
endif()

if(PSI_CPACK_RPMBUILD_EXECUTABLE)
    list(APPEND _PSI_CPACK_GENERATORS RPM)
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
    set(CPACK_RPM_PACKAGE_LICENSE "GPL-2.0-or-later")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Internet")
    set(CPACK_RPM_PACKAGE_URL "https://psi-im.org/")
    set(CPACK_RPM_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}, designed for experienced users.")
    # Let rpmbuild detect ELF and shared-library requirements automatically.
    set(CPACK_RPM_PACKAGE_AUTOREQ ON)
    message(STATUS "CPack: RPM generator enabled (${PSI_CPACK_RPMBUILD_EXECUTABLE})")
else()
    message(STATUS "CPack: rpmbuild not found; RPM generator disabled")
endif()

if(_PSI_CPACK_GENERATORS)
    set(CPACK_GENERATOR "${_PSI_CPACK_GENERATORS}")
else()
    message(WARNING "USE_CPACK is enabled, but neither dpkg-deb nor rpmbuild was found")
endif()

# Plugins and translations use the existing install() rules and therefore are
# included in the same package when enabled.
if(ENABLE_PLUGINS)
    message(STATUS "CPack: plugins included in ${CPACK_PACKAGE_NAME}")
endif()
if(LANGS_EXISTS)
    message(STATUS "CPack: translations included in ${CPACK_PACKAGE_NAME}")
endif()

include(CPack)

unset(_PSI_CPACK_GENERATORS)
unset(_PSI_CPACK_VERSION)
unset(PSI_CPACK_DPKG_DEB_EXECUTABLE)
unset(PSI_CPACK_RPMBUILD_EXECUTABLE)
