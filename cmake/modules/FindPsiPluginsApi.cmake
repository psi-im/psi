#=============================================================================
# Copyright 2018 Psi+ Project
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#=============================================================================

#Prevent extra messages on searching
if(PsiPluginsApi_INCLUDE_DIR AND PsiPluginsApi_DIR)
    # in cache already
    set(PsiPluginsApi_FIND_QUIETLY TRUE)
endif()

if(PLUGINS_ROOT_DIR)
    get_filename_component(ABS_PLUGINS_ROOT_DIR "${PLUGINS_ROOT_DIR}" ABSOLUTE)
endif()
get_filename_component(ABS_CURRENT_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
get_filename_component(ABS_PARENT_DIR "${ABS_CURRENT_DIR}/.." ABSOLUTE)

if(EXISTS "${ABS_PLUGINS_ROOT_DIR}/cmake/modules/variables.cmake")
    set(PLUGINS_DIRECTORY "${ABS_PLUGINS_ROOT_DIR}")
elseif(EXISTS "${ABS_CURRENT_DIR}/src/plugins/cmake/modules/variables.cmake")
    set(PLUGINS_DIRECTORY "${ABS_CURRENT_DIR}/src/plugins")
elseif(EXISTS "${ABS_PARENT_DIR}/src/plugins/cmake/modules/variables.cmake")
    set(PLUGINS_DIRECTORY "${ABS_CURRENT_DIR}/src/plugins")
endif()

if(CMAKE_CROSSCOMPILING OR CMAKE_CROSS_COMPILING OR (EXISTS "${ABS_PLUGINS_ROOT_DIR}/include"))
    set(SEARCH_FLAG NO_CMAKE_SYSTEM_PATH CMAKE_FIND_ROOT_PATH_BOTH)
endif()

if(PLUGINS_PATH)
    if(EXISTS "${PLUGINS_DIRECTORY}/cmake/modules/variables.cmake")
        set(PsiPluginsApi_DIR "${PLUGINS_DIRECTORY}/cmake/modules")
    endif()
    if(EXISTS "${PLUGINS_DIRECTORY}/include/applicationinfoaccessor.h")
        set(PsiPluginsApi_INCLUDE_DIR "${PLUGINS_DIRECTORY}/include")
    endif()
endif()

#Double check in case while api was found in local repo
if(PsiPluginsApi_INCLUDE_DIR AND PsiPluginsApi_DIR)
    # in cache already
    set(PsiPluginsApi_FIND_QUIETLY TRUE)
endif()

if(NOT PsiPluginsApi_DIR)
find_path(
    PsiPluginsApi_DIR
    NAMES
    "variables.cmake"
    PATHS
    ${ABS_PLUGINS_ROOT_DIR}/cmake/modules
    ${ABS_CURRENT_DIR}
    ${ABS_PARENT_DIR}/psi
    PATH_SUFFIXES
    src/plugins/cmake/modules
    share/psi/plugins
    share/psi-plus/plugins
    ${SEARCH_FLAG}
)
endif()
if(NOT PsiPluginsApi_INCLUDE_DIR)
find_path(
    PsiPluginsApi_INCLUDE_DIR
    NAMES
    "applicationinfoaccessor.h"
    PATHS
    ${ABS_PLUGINS_ROOT_DIR}/include
    ${ABS_CURRENT_DIR}
    ${ABS_PARENT_DIR}/psi
    PATH_SUFFIXES
    src/plugins/include
    include/psi/plugins
    include/psi-plus/plugins
    ${SEARCH_FLAG}
)
endif()
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
                PsiPluginsApi
                DEFAULT_MSG
                PsiPluginsApi_DIR
                PsiPluginsApi_INCLUDE_DIR
)

mark_as_advanced( PsiPluginsApi_DIR PsiPluginsApi_INCLUDE_DIR )
