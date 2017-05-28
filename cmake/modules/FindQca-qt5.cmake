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
if (Qca-qt5_INCLUDE_DIR AND Qca-qt5_LIBRARY)
	# in cache already
	set(Qca-qt5_FIND_QUIETLY TRUE)
endif ()

set ( LIBINCS 
	qca.h
)

find_path(
	Qca-qt5_INCLUDE_DIR ${LIBINCS}
	HINTS
	${QCA_DIR}/include
	PATH_SUFFIXES
	qt5/Qca-qt5/QtCrypto
	Qca-qt5/QtCrypto
	qt5/QtCrypto
)

find_library(
	Qca-qt5_LIBRARY
	NAMES qca-qt5
	HINTS 
	${QCA_DIR}/lib
	${QCA_DIR}/bin
)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
				Qca-qt5
				DEFAULT_MSG
				Qca-qt5_LIBRARY
				Qca-qt5_INCLUDE_DIR
)
if ( Qca-qt5_FOUND )
	set ( Qca-qt5_LIBRARIES ${Qca-qt5_LIBRARY} )
	set ( Qca-qt5_INCLUDE_DIRS ${Qca-qt5_INCLUDE_DIR} )
endif ( Qca-qt5_FOUND )

mark_as_advanced( Qca-qt5_INCLUDE_DIR Qca-qt5_LIBRARY )

