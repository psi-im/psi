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
if( LIBOTR_INCLUDE_DIR AND LIBOTR_LIBRARY )
	# in cache already
	set(LIBOTR_FIND_QUIETLY TRUE)
endif( LIBOTR_INCLUDE_DIR AND LIBOTR_LIBRARY )

if( UNIX AND NOT( APPLE OR CYGWIN ) )
	find_package( PkgConfig QUIET )
	pkg_check_modules( PC_LIBOTR QUIET libotr )
	if( PC_LIBOTR_FOUND )
		set( LIBOTR_DEFINITIONS ${PC_LIBOTR_CFLAGS} ${PC_LIBOTR_CFLAGS_OTHER} )
	endif( PC_LIBOTR_FOUND )
endif( UNIX AND NOT( APPLE OR CYGWIN ) )

set( LIBOTR_ROOT "" CACHE STRING "Path to libotr library" )

find_path(
	LIBOTR_INCLUDE_DIR libotr/privkey.h
	HINTS
	${LIBOTR_ROOT}/include
	${PC_LIBOTR_INCLUDEDIR}
	${PC_LIBOTR_INCLUDE_DIRS}
	PATH_SUFFIXES
	""
	libotr
)
set(LIBOTR_NAMES
	otr${D}
	libotr${D}
	otr-5
)
find_library(
	LIBOTR_LIBRARY
	NAMES ${LIBOTR_NAMES}
	HINTS 
	${PC_LIBOTR_LIBDIR}
	${PC_LIBOTR_LIBRARY_DIRS}
	${LIBOTR_ROOT}/lib
	${LIBOTR_ROOT}/bin
)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
				LibOtr
				DEFAULT_MSG
				LIBOTR_LIBRARY
				LIBOTR_INCLUDE_DIR
)
if( LIBOTR_FOUND )
	set( LIBOTR_LIBRARIES ${LIBOTR_LIBRARY} )
	set( LIBOTR_INCLUDE_DIRS ${LIBOTR_INCLUDE_DIR} )
endif( LIBOTR_FOUND )

mark_as_advanced( LIBOTR_INCLUDE_DIR LIBOTR_LIBRARY )
