set(PLUGINS_ROOT_DIR "." CACHE STRING "Plugins root path. Path where include directory placed")

if(NOT MAIN_PROGRAM_NAME)
    set(MAIN_PROGRAM_NAME "psi" CACHE STRING "Main program name: psi or psi-plus")
endif()

if(PSI_PLUS)
    set(MAIN_PROGRAM_NAME "psi-plus")
endif()

get_filename_component(ABS_INCLUDES_DIR "${CMAKE_CURRENT_LIST_DIR}/../include" ABSOLUTE)

set(CMAKE_CXX_STANDARD 17)

if( NOT WIN32 )
    set( LIB_SUFFIX "" CACHE STRING "Define suffix of directory name (32/64)" )
    set( PLUGINS_PATH "lib${LIB_SUFFIX}/${MAIN_PROGRAM_NAME}/plugins" CACHE STRING "Install suffix for plugins" )
else()
    function(set_deb_flags FLAG_VALUES FLAG_ITEM)
        foreach(FLAG ${FLAG_VALUES})
            if(NOT ("${${FLAG_ITEM}}" MATCHES "${FLAG}"))
                set(${FLAG_ITEM} "${${FLAG_ITEM}} ${FLAG}" PARENT_SCOPE)
            endif()
        endforeach()
    endfunction()
    set( PLUGINS_PATH "${MAIN_PROGRAM_NAME}/plugins" CACHE STRING "Install suffix for plugins" )
    if(MSVC)
        set_deb_flags("/MP" CMAKE_CXX_FLAGS)
        set(DEFAULT_DEBUG_FLAG "/ENTRY:mainCRTStartup /DEBUG /INCREMENTAL /SAFESEH:NO /MANIFEST:NO")
        set(DEFAULT_LINKER_FLAG "/ENTRY:mainCRTStartup /INCREMENTAL:NO /LTCG")
        set (CMAKE_MODULE_LINKER_FLAGS_DEBUG "/DEBUG /INCREMENTAL /SAFESEH:NO /MANIFEST:NO" CACHE STRING "" FORCE)
        set (CMAKE_MODULE_LINKER_FLAGS_MINSIZEREL "/INCREMENTAL:NO" CACHE STRING "" FORCE)
        set (CMAKE_MODULE_LINKER_FLAGS_RELEASE "/INCREMENTAL:NO" CACHE STRING "" FORCE)
        set (CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO "/DEBUG /INCREMENTAL:NO /MANIFEST:NO" CACHE STRING "" FORCE)
        set (CMAKE_SHARED_LINKER_FLAGS_DEBUG "${DEFAULT_DEBUG_FLAG}" CACHE STRING "" FORCE)
        set (CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL "${DEFAULT_LINKER_FLAG}" CACHE STRING "" FORCE)
        set (CMAKE_SHARED_LINKER_FLAGS_RELEASE "${DEFAULT_LINKER_FLAG}" CACHE STRING "" FORCE)
        set (CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "${DEFAULT_DEBUG_FLAG}" CACHE STRING "" FORCE)
        set(DEBUG_FLAGS "/Zi" "/MDd" "/Ob0" "/Od" "/RTC1")
        if(ISDEBUG AND NO_DEBUG_OPTIMIZATION)
            #Force use debug flags instead of release flags for debug
            set(CompilerFlags
                CMAKE_CXX_FLAGS_DEBUG
                CMAKE_C_FLAGS_DEBUG
                CMAKE_C_FLAGS_RELWITHDEBINFO
                CMAKE_CXX_FLAGS_RELWITHDEBINFO
                )
            foreach(CompilerFlag ${CompilerFlags})
              string(REPLACE "/MD " "/MDd " ${CompilerFlag} "${${CompilerFlag}}")
              string(REPLACE "/Ob1" "/Ob0" ${CompilerFlag} "${${CompilerFlag}}")
              string(REPLACE "/O2" "/Od" ${CompilerFlag} "${${CompilerFlag}}")
              string(REPLACE "/DNDEBUG" "" ${CompilerFlag} "${${CompilerFlag}}")
            endforeach()
            set_deb_flags("${DEBUG_FLAGS}" CMAKE_C_FLAGS_RELWITHDEBINFO)
            set_deb_flags("${DEBUG_FLAGS}" CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        elseif(ISDEBUG)
            set(CompilerFlags
                CMAKE_CXX_FLAGS_DEBUG
                CMAKE_C_FLAGS_DEBUG
                )
            foreach(CompilerFlag ${CompilerFlags})
              string(REPLACE "/MTd " "/MDd " ${CompilerFlag} "${${CompilerFlag}}")
            endforeach()
        endif()
        set_deb_flags("${DEBUG_FLAGS}" CMAKE_CXX_FLAGS_DEBUG)
        set_deb_flags("${DEBUG_FLAGS}" CMAKE_C_FLAGS_DEBUG)
        add_definitions(-DNOMINMAX)
        add_definitions(-D_CRT_SECURE_NO_WARNINGS)
        add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
        add_definitions(-D_CRT_NON_CONFORMING_SWPRINTFS)
        add_definitions(-D_SCL_SECURE_NO_WARNINGS)
        add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS)
        add_definitions(-D_UNICODE)
    else()
        set(FLAGS_DEBUG "-O0")
        if(ISDEBUG AND NO_DEBUG_OPTIMIZATION)
            #Force build without optimizations
            set(CompilerFlags
                CMAKE_CXX_FLAGS_DEBUG
                CMAKE_C_FLAGS_DEBUG
                CMAKE_C_FLAGS_RELWITHDEBINFO
                CMAKE_CXX_FLAGS_RELWITHDEBINFO
                )
            foreach(CompilerFlag ${CompilerFlags})
              string(REPLACE "-O3" "-O0" ${CompilerFlag} "${${CompilerFlag}}")
              string(REPLACE "-O2" "-O0" ${CompilerFlag} "${${CompilerFlag}}")
              string(REPLACE "-DNDEBUG" "" ${CompilerFlag} "${${CompilerFlag}}")
            endforeach()
            set_deb_flags(${FLAGS_DEBUG} CMAKE_C_FLAGS_RELWITHDEBINFO)
            set_deb_flags(${FLAGS_DEBUG} CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        endif()
        set_deb_flags(${FLAGS_DEBUG} CMAKE_CXX_FLAGS_DEBUG)
        set_deb_flags(${FLAGS_DEBUG} CMAKE_C_FLAGS_DEBUG)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Wextra")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
    endif()
endif()

add_definitions( -DQT_PLUGIN )
include_directories("${ABS_INCLUDES_DIR}")
