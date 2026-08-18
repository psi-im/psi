# Simple libnice cmake find

if (NOT TARGET SctpLab::UsrSCTP)
    #set(USRSCTP_DEFINITIONS INET INET6)
    find_path(USRSCTP_INCLUDE usrsctp.h HINTS ${USRSCTP_INCLUDE_DIR} PATH_SUFFIXES usrsctp)
    find_library(USRSCTP_LIBRARY NAMES usrsctp libusrsctp HINTS ${USRSCTP_LIB_DIR})

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(UsrSCTP DEFAULT_MSG USRSCTP_LIBRARY USRSCTP_INCLUDE)

    mark_as_advanced(USRSCTP_INCLUDE USRSCTP_LIBRARY)

    set(USRSCTP_LIBRARIES ${USRSCTP_LIBRARY})
    set(USRSCTP_INCLUDES ${USRSCTP_INCLUDE})

    if (UsrSCTP_FOUND)
        add_library(SctpLab::UsrSCTP UNKNOWN IMPORTED)
        set_target_properties(SctpLab::UsrSCTP PROPERTIES
                IMPORTED_LOCATION "${USRSCTP_LIBRARY}"
                INTERFACE_COMPILE_DEFINITIONS "${USRSCTP_DEFINITIONS}"
                INTERFACE_INCLUDE_DIRECTORIES "${USRSCTP_INCLUDES}"
                IMPORTED_LINK_INTERFACE_LANGUAGES "C")
    endif ()
endif ()
