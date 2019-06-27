cmake_minimum_required(VERSION 3.1.0)

if( IS_WEBENGINE )
    include(qhttp.cmake)
endif()
include(qite/libqite/libqite.cmake)
list(APPEND HEADERS ${qite_HEADERS})
list(APPEND SOURCES ${qite_SOURCES})
