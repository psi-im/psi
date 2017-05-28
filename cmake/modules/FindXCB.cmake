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
if (XCB_INCLUDE_DIR AND XCB_LIBRARY)
	# in cache already
	set(XCB_FIND_QUIETLY TRUE)
endif ()

if ( UNIX AND NOT( APPLE OR CYGWIN ) )
	find_package( PkgConfig QUIET )
	pkg_check_modules( PC_XCB QUIET xcb )
	set ( XCB_DEFINITIONS 
		${PC_XCB_CFLAGS}
		${PC_XCB_CFLAGS_OTHER}
	)
endif ( UNIX AND NOT( APPLE OR CYGWIN ) )

set ( LIBINCS 
	xcb.h
)

find_path(
	XCB_INCLUDE_DIR ${LIBINCS}
	HINTS
	${XCB_ROOT}/include
	${PC_XCB_INCLUDEDIR}
	${PC_XCB_INCLUDE_DIRS}
	PATH_SUFFIXES
	"xcb"
)

find_library(
	XCB_LIBRARY
	NAMES xcb
	HINTS 
	${PC_XCB_LIBDIR}
	${PC_XCB_LIBRARY_DIRS}
	${XCB_ROOT}/lib
	${XCB_ROOT}/bin
)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
				XCB
				DEFAULT_MSG
				XCB_LIBRARY
				XCB_INCLUDE_DIR
)
if ( XCB_FOUND )
	set ( XCB_LIBRARIES ${XCB_LIBRARY} )
	set ( XCB_INCLUDE_DIRS ${XCB_INCLUDE_DIR} )
endif ( XCB_FOUND )

mark_as_advanced( XCB_INCLUDE_DIR XCB_LIBRARY )

