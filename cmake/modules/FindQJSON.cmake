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
if (QJSON_INCLUDE_DIR AND QJSON_LIBRARY)
	# in cache already
	set(QJSON_FIND_QUIETLY TRUE)
endif ()

if ( UNIX AND NOT( APPLE OR CYGWIN ) )
	find_package( PkgConfig QUIET )
	pkg_check_modules( PC_QJSON QUIET QJson )
	set ( QJSON_DEFINITIONS 
		${PC_QJSON_CFLAGS}
		${PC_QJSON_CFLAGS_OTHER}
	)
endif ( UNIX AND NOT( APPLE OR CYGWIN ) )

set ( LIBINCS 
	parser.h
)

find_path(
	QJSON_INCLUDE_DIR ${LIBINCS}
	HINTS
	${QJSON_ROOT}/include
	${PC_QJSON_INCLUDEDIR}
	${PC_QJSON_INCLUDE_DIRS}
	PATH_SUFFIXES
	""
	if ( NOT ${WIN32} )
	qjson
	endif ( NOT ${WIN32} )
)

find_library(
	QJSON_LIBRARY
	NAMES qjson
	HINTS 
	${PC_QJSON_LIBDIR}
	${PC_QJSON_LIBRARY_DIRS}
	${QJSON_ROOT}/lib
	${QJSON_ROOT}/bin
)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
				QJson
				DEFAULT_MSG
				QJSON_LIBRARY
				QJSON_INCLUDE_DIR
)
if ( QJSON_FOUND )
	set ( QJSON_LIBRARIES ${QJSON_LIBRARY} )
	set ( QJSON_INCLUDE_DIRS ${QJSON_INCLUDE_DIR} )
endif ( QJSON_FOUND )

mark_as_advanced( QJSON_INCLUDE_DIR QJSON_LIBRARY )

