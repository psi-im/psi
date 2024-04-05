cmake_minimum_required(VERSION 3.10.0)

#detect MXE ervironment
macro(check_MXE RESULT)
    set(_USE_MXE OFF)
    if(EXISTS "${CMAKE_TOOLCHAIN_FILE}")
        string(TOLOWER ${CMAKE_TOOLCHAIN_FILE} TOOLCHAIN_FILE)
        string(REGEX MATCH "mxe-conf" MXE_DETECTED "${TOOLCHAIN_FILE}")
        if(MXE_DETECTED)
            message(STATUS "MXE environment detected")
            set(_USE_MXE ON)
            message(STATUS "MXE toolchain: ${CMAKE_TOOLCHAIN_FILE}")
            message(STATUS "MXE root path: ${CMAKE_PREFIX_PATH}")
            if(IS_WEBENGINE)
                message(FATAL_ERROR "Webengine is not available in MXE. Please set the CHAT_TYPE variable to Webkit or Basic")
            endif()
        endif()
    endif()
    set(${RESULT} ${_USE_MXE})
endmacro()


# Copy a list of files from one directory to another. Only full paths.
function(copy SOURCE DEST TARGET)
    if(EXISTS ${SOURCE})
        set(OUT_TARGET_FILE "${CMAKE_BINARY_DIR}/${TARGET}.cmake")

        string(REGEX REPLACE "\\\\+" "/" DEST "${DEST}")
        string(REGEX REPLACE "\\\\+" "/" SOURCE "${SOURCE}")

        if(NOT TARGET ${TARGET})
            file(REMOVE "${OUT_TARGET_FILE}")
            add_custom_target(${TARGET} COMMAND ${CMAKE_COMMAND} -P "${OUT_TARGET_FILE}")
        endif()

        if(IS_DIRECTORY ${SOURCE})
            # copy directory
            file(GLOB_RECURSE FILES "${SOURCE}/*")
            get_filename_component(SOURCE_DIR_NAME ${SOURCE} NAME)

            foreach(FILE ${FILES})
                file(RELATIVE_PATH REL_PATH ${SOURCE} ${FILE})
                set(REL_PATH "${SOURCE_DIR_NAME}/${REL_PATH}")
                get_filename_component(REL_PATH ${REL_PATH} DIRECTORY)
                set(DESTIN "${DEST}/${REL_PATH}")

                string(REGEX REPLACE "/+" "/" DESTIN ${DESTIN})
                string(REGEX REPLACE "/+" "/" FILE ${FILE})

                file(APPEND
                    "${OUT_TARGET_FILE}"
                    "file(INSTALL \"${FILE}\" DESTINATION \"${DESTIN}\" USE_SOURCE_PERMISSIONS)\n")
            endforeach()
        else()
            string(REPLACE "//" "/" DEST ${DEST})
            if(DEST MATCHES "/$")
                set(DIR "${DEST}")
                string(REGEX REPLACE "^(.+)/$" "\\1" DIR ${DIR})
            else()
                # need to copy and rename
                get_filename_component(DIR ${DEST} DIRECTORY)
                get_filename_component(FILENAME ${DEST} NAME)
                get_filename_component(SOURCE_FILENAME ${SOURCE} NAME)
            endif()
            file(APPEND
                "${OUT_TARGET_FILE}"
                "file(INSTALL \"${SOURCE}\" DESTINATION \"${DIR}\" USE_SOURCE_PERMISSIONS)\n")
            if(DEFINED FILENAME)
                file(APPEND
                    "${OUT_TARGET_FILE}"
                    "file(RENAME \"${DIR}/${SOURCE_FILENAME}\" \"${DIR}/${FILENAME}\")\n")
            endif()
        endif()
    endif()
endfunction()

#Copy default iconsets to build directory and add jisp files to prepare-bin target
function(prepare_iconsets ACTION)
    file(GLOB_RECURSE all_iconsets "${PROJECT_SOURCE_DIR}/iconsets/*")
    message(STATUS "Processing iconsets for ${ACTION}")
    foreach(_ITEM ${all_iconsets})
        get_filename_component(FNAME ${_ITEM} NAME)
        file(RELATIVE_PATH FREL ${PROJECT_SOURCE_DIR} ${_ITEM})
        get_filename_component(FDIR ${FREL} DIRECTORY)
        if(NOT ${ACTION} STREQUAL "prepare-bin")
            if("${_ITEM}" MATCHES ".*/default/.*" AND (NOT "${_ITEM}" MATCHES ".*/system/default/icondef.xml"))
                configure_file(${_ITEM} "${CMAKE_CURRENT_BINARY_DIR}/${FDIR}/${FNAME}" COPYONLY)
            endif()
        else()
            if(NOT "${_ITEM}" MATCHES ".*/default/.*|.*README")
                copy(${_ITEM} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${FDIR}/${FNAME}" ${ACTION})
            endif()
        endif()
        unset(_ITEM)
        unset(FNAME)
        unset(FREL)
        unset(FDIR)
    endforeach()
endfunction()

if(WIN32)
    function(compile_rc_file RC_FILE_NAME RC_OUTPUT_NAME)
        if(NOT MSVC)
            set(CMD_ARG
                --include=${CMAKE_CURRENT_SOURCE_DIR}
                --input=${RC_FILE_NAME}
                --output=${RC_OUTPUT_NAME}
        )
        else()
            set(CMD_ARG
                /fo
                ${RC_OUTPUT_NAME}
                ${RC_FILE_NAME}
            )
        endif()
        add_custom_command(OUTPUT ${RC_OUTPUT_NAME}
            COMMAND ${CMAKE_RC_COMPILER}
            ARGS ${CMD_ARG}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/win32
            VERBATIM
        )
    endfunction()
endif()
