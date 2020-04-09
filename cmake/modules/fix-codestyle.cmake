cmake_minimum_required( VERSION 3.1.0 )

#Find clang-format binary
find_program(CLF_BIN clang-format DOC "Path to clang-format binary")
if(CLF_BIN)
    #Obtain list of source files
    file(GLOB_RECURSE SRC_LIST
        *.c
        *.cc
        *.cpp
        *.hpp
        *.h
        *.mm
        ../qcm/*.qcm
    )
    foreach(src_file ${SRC_LIST})
        #Exclude libpsi
        if("${src_file}" MATCHES ".*/libpsi/.*")
            list(REMOVE_ITEM SRC_LIST ${src_file})
        endif()
    endforeach()
    add_custom_target(fix-codestyle
        COMMAND ${CLF_BIN}
        --verbose
        -style=file
        -i ${SRC_LIST}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Fix codestyle with clang-format"
        VERBATIM
    )
endif()
