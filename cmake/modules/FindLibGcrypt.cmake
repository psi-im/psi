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
if( LIBGCRYPT_INCLUDE_DIR AND LIBGCRYPT_LIBRARY )
	# in cache already
	set(libgcrypt_FIND_QUIETLY TRUE)
endif( LIBGCRYPT_INCLUDE_DIR AND LIBGCRYPT_LIBRARY )

if( UNIX AND NOT( APPLE OR CYGWIN ) )
	find_package( PkgConfig QUIET )
	pkg_check_modules( PC_LIBGCRYPT QUIET libgcrypt )
	if( PC_LIBGCRYPT_FOUND )
		set( LIBGCRYPT_DEFINITIONS ${PC_LIBGCRYPT_CFLAGS} )
	endif( PC_LIBGCRYPT_FOUND )
endif( UNIX AND NOT( APPLE OR CYGWIN ) )

if( WIN32 )
	FIND_PROGRAM(LIBGCRYPTCONFIG_EXECUTABLE NAMES libgcrypt-config PATHS ${LIBGCRYPT_ROOT}/bin)
	IF(NOT "${LIBGCRYPTCONFIG_EXECUTABLE}" STREQUAL "LIBGCRYPTCONFIG_EXECUTABLE-NOTFOUND" )
		execute_process(COMMAND sh "${LIBGCRYPTCONFIG_EXECUTABLE}" --prefix OUTPUT_VARIABLE PREFIX)
		set(LIBGCRYPT_LIB_HINT "${PREFIX}/lib")
		set(LIBGCRYPT_INCLUDE_HINT "${PREFIX}/include")
	ENDIF()
endif( WIN32 )

set( LIBGCRYPT_ROOT "" CACHE STRING "Path to libgcrypt library" )

find_path(
	LIBGCRYPT_INCLUDE_DIR gcrypt.h
	HINTS
	${LIBGCRYPT_ROOT}/include
	${PC_LIBGCRYPT_INCLUDEDIR}
	${PC_LIBGCRYPT_INCLUDE_DIRS}
	${LIBGCRYPT_INCLUDE_HINT}
)
set(LIBGCRYPT_NAMES
	gcrypt${D}
	libgcrypt${D}
	gcrypt-11
	libgcrypt-11
	gcrypt-20
	libgcrypt-20
)
find_library(
	LIBGCRYPT_LIBRARY
	NAMES ${LIBGCRYPT_NAMES}
	HINTS 
	${PC_LIBGCRYPT_LIBDIR}
	${PC_LIBGCRYPT_LIBRARY_DIRS}
	${LIBGCRYPT_LIB_HINT}
	${LIBGCRYPT_ROOT}/lib
	${LIBGCRYPT_ROOT}/bin
)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
				LibGcrypt
				DEFAULT_MSG
				LIBGCRYPT_LIBRARY
				LIBGCRYPT_INCLUDE_DIR
)
if( LIBGCRYPT_FOUND )
	set( LIBGCRYPT_LIBRARIES ${LIBGCRYPT_LIBRARY} )
	set( LIBGCRYPT_INCLUDE_DIRS ${LIBGCRYPT_INCLUDE_DIR} )
endif( LIBGCRYPT_FOUND )
mark_as_advanced( LIBGCRYPT_INCLUDE_DIR LIBGCRYPT_LIBRARY )
