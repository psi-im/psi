cmake_minimum_required(VERSION 3.10.0)

if( UNIX OR IS_WEBENGINE )
    include(qhttp.cmake)
endif()
