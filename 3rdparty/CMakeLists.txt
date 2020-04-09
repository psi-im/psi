cmake_minimum_required(VERSION 3.1.0)

if( UNIX OR IS_WEBENGINE )
    include(qhttp.cmake)
endif()
