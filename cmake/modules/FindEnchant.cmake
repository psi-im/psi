#=============================================================================
# Copyright 2016-2020 Psi+ Project, Vitaly Tonkacheyev
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
if (Enchant_INCLUDE_DIR AND Enchant_LIBRARY)
    # in cache already
    set(Enchant_FIND_QUIETLY TRUE)
endif()

if ( UNIX AND NOT( APPLE OR CYGWIN ) )
    find_package( PkgConfig QUIET )
    if(PkgConfig_FOUND)
        #Searching for enchant-1.x
        pkg_check_modules( PC_Enchant QUIET enchant )
        if(NOT PC_Enchant_FOUND)
            #If enchant-1.x not found searching for enchant-2.x
            pkg_check_modules( PC_Enchant QUIET enchant-2 )
        endif()
        #set package variables
        if(PC_Enchant_FOUND)
            set(Enchant_DEFINITIONS
                ${PC_Enchant_CFLAGS}
                ${PC_Enchant_CFLAGS_OTHER}
            )
            if(PC_Enchant_VERSION)
                set(Enchant_VERSION ${PC_Enchant_VERSION})
            endif()
        endif()
    endif()
endif()

set ( LIBINCS
    enchant.h
)

find_path(
    Enchant_INCLUDE_DIR ${LIBINCS}
    HINTS
    ${PC_Enchant_INCLUDE_DIRS}
    ${Enchant_ROOT}/include
    PATH_SUFFIXES
    enchant
    enchant-2
)

find_library(
    Enchant_LIBRARY
    NAMES enchant enchant-2
    HINTS
    ${PC_Enchant_LIBRARY_DIRS}
    ${Enchant_ROOT}/lib
    ${Enchant_ROOT}/bin
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
                Enchant
                DEFAULT_MSG
                Enchant_LIBRARY
                Enchant_INCLUDE_DIR
)

if ( Enchant_FOUND )
    set ( Enchant_LIBRARIES ${Enchant_LIBRARY} )
    set ( Enchant_INCLUDE_DIRS ${Enchant_INCLUDE_DIR} )
endif()

if( Enchant_VERSION )
    mark_as_advanced( Enchant_INCLUDE_DIR Enchant_LIBRARY Enchant_VERSION )
else ()
    message(WARNING "No enchant version found. For use enchant-2 library you should set HAVE_ENCHANT2 definition manually")
    mark_as_advanced( Enchant_INCLUDE_DIR Enchant_LIBRARY )
endif()
