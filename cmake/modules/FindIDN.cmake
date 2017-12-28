#=============================================================================
# Copyright 2016-2017 Psi+ Project, Vitaly Tonkacheyev
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
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND WIN32)
    set(D "d")
endif()
if( IDN_INCLUDE_DIR AND IDN_LIBRARY )
    # in cache already
    set(IDN_FIND_QUIETLY TRUE)
endif( IDN_INCLUDE_DIR AND IDN_LIBRARY )

if( UNIX AND NOT( APPLE OR CYGWIN ) )
    find_package( PkgConfig QUIET )
    pkg_check_modules( PC_IDN QUIET libidn )
    if( PC_IDN_FOUND )
        set( IDN_DEFINITIONS ${PC_IDN_CFLAGS} ${PC_IDN_CFLAGS_OTHER} )
    endif( PC_IDN_FOUND )
endif( UNIX AND NOT( APPLE OR CYGWIN ) )

set( IDN_ROOT "" CACHE STRING "Path to libidn library" )

find_path(
    IDN_INCLUDE_DIR idna.h
    HINTS
    ${IDN_ROOT}/include
    ${PC_IDN_INCLUDEDIR}
    ${PC_IDN_INCLUDE_DIRS}
)
set(IDN_NAMES
    idn${D}
    libidn${D}
    idn-11${D}
    libidn-11${D}
)
find_library(
    IDN_LIBRARY
    NAMES ${IDN_NAMES}
    HINTS
    ${PC_IDN_LIBDIR}
    ${PC_IDN_LIBRARY_DIRS}
    ${IDN_ROOT}/lib
    ${IDN_ROOT}/bin
)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
                IDN
                DEFAULT_MSG
                IDN_LIBRARY
                IDN_INCLUDE_DIR
)
if( IDN_FOUND )
    set( IDN_LIBRARIES ${IDN_LIBRARY} )
    set( IDN_INCLUDE_DIRS ${IDN_INCLUDE_DIR} )
endif( IDN_FOUND )

mark_as_advanced( IDN_INCLUDE_DIR IDN_LIBRARY )
