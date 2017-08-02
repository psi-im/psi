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
if( LIBGPGERROR_INCLUDE_DIR AND LIBGPGERROR_LIBRARY )
	# in cache already
	set(LIBGPGERROR_FIND_QUIETLY TRUE)
endif( LIBGPGERROR_INCLUDE_DIR AND LIBGPGERROR_LIBRARY )

if( UNIX AND NOT( APPLE OR CYGWIN ) )
	find_package( PkgConfig QUIET )
	pkg_check_modules( PC_LIBGPGERROR QUIET libgpg-error )
	if( PC_LIBGPGERROR_FOUND )
		set( LIBGPGERROR_DEFINITIONS ${PC_LIBGPGERROR_CFLAGS} )
	endif( PC_LIBGPGERROR_FOUND )
endif( UNIX AND NOT( APPLE OR CYGWIN ) )

if( WIN32 )
	FIND_PROGRAM(LIBGPGERRORCONFIG_EXECUTABLE NAMES libgpg-error-config PATHS ${LIBGPGERROR_ROOT}/bin)
	IF(LIBGPGERRORCONFIG_EXECUTABLE)
		execute_process(COMMAND sh "${LIBGPGERRORCONFIG_EXECUTABLE}" --prefix OUTPUT_VARIABLE PREFIX)
		set(LIBGPGERROR_LIB_HINT "${PREFIX}/lib")
		set(LIBGPGERROR_INCLUDE_HINT "${PREFIX}/include")
	ENDIF(LIBGPGERRORCONFIG_EXECUTABLE)
endif( WIN32 )

set( LIBGPGERROR_ROOT "" CACHE STRING "Path to libgpg-error library" )

find_path(
	LIBGPGERROR_INCLUDE_DIR gpg-error.h
	HINTS
	${LIBGPGERROR_ROOT}/include
	${PC_LIBGPGERROR_INCLUDEDIR}
	${PC_LIBGPGERROR_INCLUDE_DIRS}
	${LIBGPGERROR_INCLUDE_HINT}
)
set(LIBGPGERROR_NAMES
	gpg-error${D}
	libgpg-error${D}
	gpg-error-0
	libgpg-error-0
	gpg-error6-0
	libgpg-error6-0
)
find_library(
	LIBGPGERROR_LIBRARY
	NAMES ${LIBGPGERROR_NAMES}
	HINTS 
	${PC_LIBGPGERROR_LIBDIR}
	${PC_LIBGPGERROR_LIBRARY_DIRS}
	${LIBGPGERROR_LIB_HINT}
	${LIBGPGERROR_ROOT}/lib
	${LIBGPGERROR_ROOT}/bin
)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
				LibGpgError
				DEFAULT_MSG
				LIBGPGERROR_LIBRARY
				LIBGPGERROR_INCLUDE_DIR
)
if( LIBGPGERROR_FOUND )
	set( LIBGPGERROR_LIBRARIES ${LIBGPGERROR_LIBRARY} )
	set( LIBGPGERROR_INCLUDE_DIRS ${LIBGPGERROR_INCLUDE_DIR} )
endif( LIBGPGERROR_FOUND )
mark_as_advanced( LIBGPGERROR_INCLUDE_DIR LIBGPGERROR_LIBRARY )
