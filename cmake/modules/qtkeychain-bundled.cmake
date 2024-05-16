cmake_minimum_required(VERSION 3.10.0)

set(QtkeychainRepo "https://github.com/frankosterfeld/qtkeychain.git")

include(GNUInstallDirs)
set(libname qt${QT_DEFAULT_MAJOR_VERSION}keychain)
message(STATUS "Qt${QT_DEFAULT_MAJOR_VERSION}Keychain: using bundled")
set(Qtkeychain_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/${libname})
set(Qtkeychain_BUILD_DIR ${Qtkeychain_PREFIX}/build)
set(Qtkeychain_INSTALL_DIR ${Qtkeychain_PREFIX}/install)
set(Qtkeychain_INCLUDE_DIR ${Qtkeychain_INSTALL_DIR}/include)
set(Qtkeychain_LIBRARY_NAME
${CMAKE_STATIC_LIBRARY_PREFIX}${libname}${CMAKE_STATIC_LIBRARY_SUFFIX}
)
set(Qtkeychain_LIBRARY ${Qtkeychain_INSTALL_DIR}/${CMAKE_INSTALL_LIBDIR}/${Qtkeychain_LIBRARY_NAME})

if(APPLE)
    set(COREFOUNDATION_LIBRARY "-framework CoreFoundation")
    set(COREFOUNDATION_LIBRARY_SECURITY "-framework Security")
    list(APPEND Qtkeychain_LIBRARY ${COREFOUNDATION_LIBRARY} ${COREFOUNDATION_LIBRARY_SECURITY})
endif()

if(${QT_DEFAULT_MAJOR_VERSION} VERSION_GREATER "5")
    set(BUILD_WITH_QT6 ON)
else()
    set(BUILD_WITH_QT6 OFF)
endif()

include(FindGit)
find_package(Git REQUIRED)

include(FindPkgConfig)

include(ExternalProject)
#set CMake options and transfer the environment to an external project
set(Qtkeychain_BUILD_OPTIONS
    -DBUILD_SHARED_LIBS=OFF
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX=${Qtkeychain_INSTALL_DIR}
    -DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}
    -DBUILD_TEST_APPLICATION=OFF
    -DBUILD_WITH_QT6=${BUILD_WITH_QT6}
    -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
    -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
    -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
    -DCMAKE_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}
    -DLIBSECRET_SUPPORT=OFF
    -DOSX_FRAMEWORK=OFF
)

ExternalProject_Add(QtkeychainProject
    PREFIX ${Qtkeychain_PREFIX}
    BINARY_DIR ${Qtkeychain_BUILD_DIR}
    GIT_REPOSITORY "${QtkeychainRepo}"
    GIT_TAG main
    CMAKE_ARGS ${Qtkeychain_BUILD_OPTIONS}
    BUILD_BYPRODUCTS ${Qtkeychain_LIBRARY}
    UPDATE_COMMAND ""
)

set(QTKEYCHAIN_LIBRARIES ${Qtkeychain_LIBRARY})
set(QTKEYCHAIN_INCLUDE_DIRS ${Qtkeychain_INCLUDE_DIR})

message(STATUS "LIB=${Qtkeychain_LIBRARY}")
message(STATUS "HEADER=${Qtkeychain_INCLUDE_DIR}")
