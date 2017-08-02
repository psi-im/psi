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
if( ZLIB_INCLUDE_DIR AND ZLIB_LIBRARY )
	# in cache already
	set(ZLIB_FIND_QUIETLY TRUE)
endif( ZLIB_INCLUDE_DIR AND ZLIB_LIBRARY )

if( UNIX AND NOT( APPLE OR CYGWIN ) )
	find_package( PkgConfig QUIET )
	pkg_check_modules( PC_ZLIB QUIET zlib )
	if( PC_ZLIB_FOUND )
		set( ZLIB_DEFINITIONS ${PC_ZLIB_CFLAGS} ${PC_ZLIB_CFLAGS_OTHER} )
	endif( PC_ZLIB_FOUND )
endif( UNIX AND NOT( APPLE OR CYGWIN ) )

set( ZLIB_ROOT "" CACHE STRING "Path to libidn library" )

find_path(
	ZLIB_INCLUDE_DIR zlib.h
	HINTS
	${ZLIB_ROOT}/include
	${PC_ZLIB_INCLUDEDIR}
	${PC_ZLIB_INCLUDE_DIRS}
)
set(ZLIB_NAMES
	z${D}
	lz${D}
	zlib${D}
	zlib1
)
find_library(
	ZLIB_LIBRARY
	NAMES ${ZLIB_NAMES}
	HINTS
	${PC_ZLIB_LIBDIR}
	${PC_ZLIB_LIBRARY_DIRS}
	${ZLIB_ROOT}/lib
	${ZLIB_ROOT}/bin
)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
				ZLIB
				DEFAULT_MSG
				ZLIB_LIBRARY
				ZLIB_INCLUDE_DIR
)
if( ZLIB_FOUND )
	set( ZLIB_LIBRARIES ${ZLIB_LIBRARY} )
	set( ZLIB_INCLUDE_DIRS ${ZLIB_INCLUDE_DIR} )
endif( ZLIB_FOUND )

mark_as_advanced( ZLIB_INCLUDE_DIR ZLIB_LIBRARY )
