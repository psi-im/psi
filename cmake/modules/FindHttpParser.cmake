#=============================================================================
# Copyright 2020 Psi+ Project, Vitaly Tonkacheyev
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

if (HttpParser_INCLUDE_DIR AND HttpParser_LIBRARY)
    # in cache already
    set(HttpParser_FIND_QUIETLY TRUE)
endif()

find_path(
    HttpParser_INCLUDE_DIR http_parser.h
    HINTS
    ${HTTP_PARSER_ROOT}/include
)

find_library(
    HttpParser_LIBRARY
    NAMES http_parser
    HINTS
    ${HTTP_PARSER_ROOT}/lib
    ${HTTP_PARSER_ROOT}/bin
)

#Obtain library version
if(HttpParser_INCLUDE_DIR)
    set(INC_FILE "${HttpParser_INCLUDE_DIR}/http_parser.h")
    file(STRINGS "${INC_FILE}" VER_LINES)
    string(REGEX MATCH "HTTP_PARSER_VERSION_MAJOR ([0-9]+)" VER_LINE_MAJOR ${VER_LINES})
    if(CMAKE_MATCH_1)
        set(HttpParser_VERSION_MAJOR "${CMAKE_MATCH_1}")
    endif()
    string(REGEX MATCH "HTTP_PARSER_VERSION_MINOR ([0-9]+)" VER_LINE_MINOR ${VER_LINES})
    if(CMAKE_MATCH_1)
        set(HttpParser_VERSION_MINOR "${CMAKE_MATCH_1}")
    endif()
    string(REGEX MATCH "HTTP_PARSER_VERSION_PATCH ([0-9]+)" VER_LINE_PATCH ${VER_LINES})
    if(CMAKE_MATCH_1)
        set(HttpParser_VERSION_PATCH "${CMAKE_MATCH_1}")
    endif()
    if(HttpParser_VERSION_MAJOR AND (HttpParser_VERSION_MINOR AND HttpParser_VERSION_PATCH))
        set(HttpParser_VERSION "${HttpParser_VERSION_MAJOR}.${HttpParser_VERSION_MINOR}.${HttpParser_VERSION_PATCH}")
    endif()
endif()


include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
                HttpParser
                DEFAULT_MSG
                HttpParser_LIBRARY
                HttpParser_INCLUDE_DIR
                HttpParser_VERSION
)

if (HttpParser_FOUND)
    set ( HttpParser_LIBRARIES ${HttpParser_LIBRARY} )
    set ( HttpParser_INCLUDE_DIRS ${HttpParser_INCLUDE_DIR} )
endif()

mark_as_advanced( HttpParser_INCLUDE_DIR HttpParser_LIBRARY HttpParser_VERSION )
