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

if(PsiPluginsApi_INCLUDE_DIR)
    # in cache already
    set(PsiSdk_FIND_QUIETLY TRUE)
endif()

find_path(
    PsiPluginsApi_DIR variables.cmake
    HINTS
    share/psi/plugins
    share/psi-plus/plugins
)

find_path(
    PsiPluginsApi_INCLUDE_DIR applicationinfoaccessor.h
    HINTS
    share/psi/plugins/include
    share/psi-plus/plugins/include
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
                PsiPluginsApi
                DEFAULT_MSG
                PluginsApi_DIR
                PluginsApi_INCLUDE_DIR
)

mark_as_advanced( PsiPluginsApi_DIR PsiPluginsApi_INCLUDE_DIR )
