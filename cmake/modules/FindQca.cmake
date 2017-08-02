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
if (Qca_INCLUDE_DIR AND Qca_LIBRARY)
	# in cache already
	set(Qca_FIND_QUIETLY TRUE)
endif ()

set ( LIBINCS 
	qca.h
)

if(NOT Qca_SUFFIX)
	set(Qca_SUFFIX "")
endif()

if("${Qca_SUFFIX}" STREQUAL "-qt5")
	set(EXTRA_PATH_SUFFIXES
		qt5/Qca-qt5/QtCrypto
		Qca-qt5/QtCrypto
		qt5/QtCrypto
	)
endif()

find_path(
	Qca_INCLUDE_DIR ${LIBINCS}
	HINTS
	${QCA_DIR}/include
	PATH_SUFFIXES
	QtCrypto
	${EXTRA_PATH_SUFFIXES}
)
set(Qca_NAMES
	qca${Qca_SUFFIX}${D}
)
find_library(
	Qca_LIBRARY
	NAMES ${Qca_NAMES}
	HINTS 
	${QCA_DIR}/lib
	${QCA_DIR}/bin
)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
				Qca
				DEFAULT_MSG
				Qca_LIBRARY
				Qca_INCLUDE_DIR
)
if ( Qca_FOUND )
	set ( Qca_LIBRARIES ${Qca_LIBRARY} )
	set ( Qca_INCLUDE_DIRS ${Qca_INCLUDE_DIR} )
endif ( Qca_FOUND )

mark_as_advanced( Qca_INCLUDE_DIR Qca_LIBRARY )

