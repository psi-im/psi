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
if( LIBTIDY_INCLUDE_DIR AND LIBTIDY_LIBRARY )
	# in cache already
	set(LIBTIDY_FIND_QUIETLY TRUE)
endif( LIBTIDY_INCLUDE_DIR AND LIBTIDY_LIBRARY )

if( UNIX AND NOT( APPLE OR CYGWIN ) )
	find_package( PkgConfig QUIET )
	pkg_check_modules( PC_LIBTIDY QUIET libtidy )
	if( PC_LIBTIDY_FOUND )
		set( LIBTIDY_DEFINITIONS ${PC_LIBTIDY_CFLAGS} ${PC_LIBTIDY_CFLAGS_OTHER} )
	endif( PC_LIBTIDY_FOUND )
endif( UNIX AND NOT( APPLE OR CYGWIN ) )

set( LIBTIDY_ROOT "" CACHE STRING "Path to libtidy library" )

find_path(
	LIBTIDY_INCLUDE_DIR tidy.h
	HINTS
	${PC_LIBTIDY_INCLUDEDIR}
	${PC_LIBTIDY_INCLUDE_DIRS}
	PATHS
	${LIBTIDY_ROOT}/include
	PATH_SUFFIXES
	""
	tidy
)

if( EXISTS "${LIBTIDY_INCLUDE_DIR}/tidybuffio.h" OR (EXISTS "${LIBTIDY_INCLUDE_DIR}/tidy/tidybuffio.h") )
	message("-- Tidy-html5 detected")
else()
	message("-- Tidy-html legacy detected")
	set( LIBTIDY_DEFINITIONS "${LIBTIDY_DEFINITIONS} -DLEGACY_TIDY" )
endif()
set(LIBTIDY_NAMES
	tidy${D}
	libtidy${D}
	libtidy-0-99-0
	tidy-0-99-0
)
find_library(
	LIBTIDY_LIBRARY
	NAMES ${LIBTIDY_NAMES}
	HINTS
	${PC_LIBTIDY_LIBDIR}
	${PC_LIBTIDY_LIBRARY_DIRS}
	${LIBTIDY_ROOT}/lib
	${LIBTIDY_ROOT}/bin
)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
				LibTidy
				DEFAULT_MSG
				LIBTIDY_LIBRARY
				LIBTIDY_INCLUDE_DIR
)
if( LIBTIDY_FOUND )
	set( LIBTIDY_LIBRARIES ${LIBTIDY_LIBRARY} )
	set( LIBTIDY_INCLUDE_DIRS ${LIBTIDY_INCLUDE_DIR} )
endif( LIBTIDY_FOUND )

mark_as_advanced( LIBTIDY_INCLUDE_DIR LIBTIDY_LIBRARY )
