#=============================================================================
# Copyright 2016-2017 Psi+ Project
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

if (QJDns_INCLUDE_DIR AND QJDns_LIBRARY)
	# in cache already
	set(QJDns_FIND_QUIETLY TRUE)
endif ()

if ( UNIX AND NOT( APPLE OR CYGWIN ) )
	find_package( PkgConfig QUIET )
	pkg_check_modules( PC_QJDns QUIET jdns )
	set ( QJDns_DEFINITIONS 
		${PC_QJDns_CFLAGS}
		${PC_QJDns_CFLAGS_OTHER}
	)
endif ( UNIX AND NOT( APPLE OR CYGWIN ) )

set ( LIBINCS 
	qjdns.h
)

if(NOT QJDns_SUFFIX)
	set( QJDns_SUFFIX "")
endif()

find_path(
	QJDns_INCLUDE_DIR ${LIBINCS}
	HINTS
	${QJDNS_DIR}/include
	${PC_QJDns_INCLUDEDIR}
	${PC_QJDns_INCLUDE_DIRS}
	PATH_SUFFIXES
	""
	if( NOT WIN32 )
		jdns
	endif( NOT WIN32 )
)

find_library(
	QJDns_LIBRARY
	NAMES qjdns qjdns${QJDns_SUFFIX}
	HINTS 
	${PC_QJDns_LIBDIR}
	${PC_QJDns_LIBRARY_DIRS}
	${QJDNS_DIR}/lib
	${QJDNS_DIR}/bin
)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
				QJDns
				DEFAULT_MSG
				QJDns_LIBRARY
				QJDns_INCLUDE_DIR
)
if ( QJDns_FOUND )
	set ( QJDns_LIBRARIES ${QJDns_LIBRARY} )
	set ( QJDns_INCLUDE_DIRS ${QJDns_INCLUDE_DIR} )
endif ( QJDns_FOUND )

mark_as_advanced( QJDns_INCLUDE_DIR QJDns_LIBRARY )
