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
if (HUNSPELL_INCLUDE_DIR AND HUNSPELL_LIBRARY)
	# in cache already
	set(HUNSPELL_FIND_QUIETLY TRUE)
endif ()

if ( UNIX AND NOT( APPLE OR CYGWIN ) )
	find_package( PkgConfig QUIET )
	pkg_check_modules( PC_HUNSPELL QUIET hunspell )
	set ( HUNSPELL_DEFINITIONS 
		${PC_HUNSPELL_CFLAGS}
		${PC_HUNSPELL_CFLAGS_OTHER}
	)
endif ( UNIX AND NOT( APPLE OR CYGWIN ) )

set ( LIBINCS 
	hunspell.hxx
)

find_path(
	HUNSPELL_INCLUDE_DIR ${LIBINCS}
	HINTS
	${HUNSPELL_ROOT}/include
	${PC_HUNSPELL_INCLUDEDIR}
	${PC_HUNSPELL_INCLUDE_DIRS}
	PATH_SUFFIXES
	""
	if ( NOT ${WIN32} )
	hunspell
	endif ( NOT ${WIN32} )
)
set(HUNSPELL_NAMES
	hunspell${d}
	hunspell-1.3
	hunspell-1.4
	hunspell-1.5
	hunspell-1.6
	libhunspell${d}
)
find_library(
	HUNSPELL_LIBRARY
	NAMES ${HUNSPELL_NAMES}
	HINTS 
	${PC_HUNSPELL_LIBDIR}
	${PC_HUNSPELL_LIBRARY_DIRS}
	${HUNSPELL_ROOT}/lib
	${HUNSPELL_ROOT}/bin
)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
				Hunspell
				DEFAULT_MSG
				HUNSPELL_LIBRARY
				HUNSPELL_INCLUDE_DIR
)
if ( HUNSPELL_FOUND )
	set ( HUNSPELL_LIBRARIES ${HUNSPELL_LIBRARY} )
	set ( HUNSPELL_INCLUDE_DIRS ${HUNSPELL_INCLUDE_DIR} )
endif ( HUNSPELL_FOUND )

mark_as_advanced( HUNSPELL_INCLUDE_DIR HUNSPELL_LIBRARY )

