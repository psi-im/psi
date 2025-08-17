#=============================================================================
# Copyright 2016-2020 Psi+ Project, Vitaly Tonkacheyev
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors
#    may be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS  “AS IS” AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS  BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#=============================================================================
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND WIN32)
    set(D "d")
endif()
if (Qca_INCLUDE_DIR AND Qca_LIBRARY)
    # in cache already
    set(Qca_FIND_QUIETLY TRUE)
endif()

set(EXTRA_PATH_SUFFIXES
    qt${QT_DEFAULT_MAJOR_VERSION}/Qca-qt${QT_DEFAULT_MAJOR_VERSION}/QtCrypto
    Qca-qt${QT_DEFAULT_MAJOR_VERSION}/QtCrypto
    qt${QT_DEFAULT_MAJOR_VERSION}/QtCrypto
    qt/Qca-qt${QT_DEFAULT_MAJOR_VERSION}/QtCrypto
    lib/qca-qt${QT_DEFAULT_MAJOR_VERSION}.framework/Versions/2/Headers
)

find_path(
    Qca_INCLUDE_DIR qca.h
    HINTS
    ${QCA_DIR}/include
    PATH_SUFFIXES
    QtCrypto
    ${EXTRA_PATH_SUFFIXES}
)

find_library(
    Qca_LIBRARY
    NAMES qca-qt${QT_DEFAULT_MAJOR_VERSION}${D}
    HINTS
    ${Qca_ROOT}/lib
    ${Qca_ROOT}/bin
    ${Qca_DIR}/lib
    ${Qca_DIR}/bin
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
                Qca
                DEFAULT_MSG
                Qca_LIBRARY
                Qca_INCLUDE_DIR
)

if (Qca_FOUND)
    set ( Qca_LIBRARIES ${Qca_LIBRARY} )
    set ( Qca_INCLUDE_DIRS ${Qca_INCLUDE_DIR} )
endif()

mark_as_advanced( Qca_INCLUDE_DIR Qca_LIBRARY )
