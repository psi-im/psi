cmake_minimum_required(VERSION 3.10.0)
if(WIN32)
    set(LIBS_TARGET prepare-bin-libs)
    set(DEPS_INSTALL_PREFIX ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    if(PRODUCTION)
        set(LIBS_TARGET install-deps)
        set(DEPS_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
    endif()
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(D "d")
    endif()
    # Get Qt installation path
    string(REGEX REPLACE "([^ ]+)[/\\].*" "\\1" QT_BIN_DIR_TMP "${QT_MOC_EXECUTABLE}")
    string(REGEX REPLACE "\\\\" "/" QT_BIN_DIR "${QT_BIN_DIR_TMP}")
    unset(QT_BIN_DIR_TMP)
    # find_psi_lib function
    function(find_psi_lib LIBLIST PATHES OUTPUT_PATH)
        set(_LIBS ${LIBLIST})
        set(_PATHES ${PATHES})
        set(_OUTPUT_PATH ${OUTPUT_PATH})
        set(FULL_LIBNAMES "")
        # Generate list with libnames templates
        foreach(_path ${_PATHES})
            foreach(_libname ${_LIBS})
                set(_tmpname "${_path}/${_libname}")
                string(REGEX REPLACE "/+" "/" _tmpname_fixed "${_tmpname}")
                list(APPEND FULL_LIBNAMES ${_tmpname_fixed})
            endforeach()
        endforeach()
        list(SORT FULL_LIBNAMES)
        list(REMOVE_DUPLICATES FULL_LIBNAMES)
        # Get real libraries names for each template
        foreach(_liba ${FULL_LIBNAMES})
            set(_library _library-NOTFOUND)
            # Get list of libraries
            file(GLOB _library
                LIST_DIRECTORIES false
                ${_liba}
            )
            # Add each existing library to copy target
            foreach(_fname ${_library})
                if(EXISTS "${_fname}")
                    message("library found: ${_fname}")
                    copy("${_fname}" "${_OUTPUT_PATH}" "${LIBS_TARGET}")
                endif()
            endforeach()
        endforeach()
        unset(_LIBS)
        unset(_PATHES)
        unset(_OUTPUT_PATH)
        unset(FULL_LIBNAMES)
    endfunction()
    set(SDK_PREFIX "")
    find_package(Qt${QT_DEFAULT_MAJOR_VERSION} COMPONENTS Core)
    if((${QT_DEFAULT_MAJOR_VERSION} GREATER_EQUAL 6) AND ("${QT_BIN_DIR}" STREQUAL ""))
        set(QT_BIN_DIR "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}")
        message(STATUS "Qt${QT_DEFAULT_MAJOR_VERSION} bin directory found at ${QT_BIN_DIR}")
        set(QT_DIR "${QT6_INSTALL_PREFIX}")
        if(NOT EXISTS ${QT_BIN_DIR})
            message(FATAL_ERROR "No Qt bin directory found")
        endif()
    else()
        get_filename_component(QT_DIR ${QT_BIN_DIR} DIRECTORY)
    endif()
    message(STATUS "Qt${QT_DEFAULT_MAJOR_VERSION} directory found at ${QT_DIR}")
    set(QT_PLUGINS_DIR ${QT_DIR}/plugins)
    set(QT_TRANSLATIONS_DIR ${QT_DIR}/translations)
    #Output pathes
    set(QT_PLUGINS_OUTPUT "${DEPS_INSTALL_PREFIX}/qtplugins")
    set(QT_LIBS_OUTPUT "${DEPS_INSTALL_PREFIX}/")
    if(BUILD_PSIMEDIA)
        set(PSIMEDIA_LIBS_OUTPUT "${DEPS_INSTALL_PREFIX}/")
    endif()
    set(PSI_LIBS_OUTPUT "${DEPS_INSTALL_PREFIX}/")
    set(PSIMEDIA_FOUND OFF)
    #Set paths
    list(APPEND PATHES
        ${QT_BIN_DIR}
        ${Qca_DIR}bin
        ${Qca_DIR}/bin
        ${QT_PLUGINS_DIR}/crypto
        ${Qca_DIR}lib/qca-qt${QT_DEFAULT_MAJOR_VERSION}/crypto
        ${Qca_DIR}lib/Qca-qt${QT_DEFAULT_MAJOR_VERSION}/crypto
        ${Qca_DIR}lib/qca3-qt${QT_DEFAULT_MAJOR_VERSION}/crypto
        ${Qca_DIR}lib/Qca3-qt${QT_DEFAULT_MAJOR_VERSION}/crypto
        )
    if(USE_MXE)
        list(APPEND PATHES
            ${CMAKE_PREFIX_PATH}/bin
            ${CMAKE_PREFIX_PATH}/lib
            ${CMAKE_PREFIX_PATH}/lib/ossl-modules
            ${CMAKE_PREFIX_PATH}/lib/qca3-qt${QT_DEFAULT_MAJOR_VERSION}/crypto
            ${CMAKE_PREFIX_PATH}/lib/Qca3-qt${QT_DEFAULT_MAJOR_VERSION}/crypto
            )
    endif()
    if(EXISTS "${SDK_PATH}")
        list(APPEND PATHES
            "${HUNSPELL_ROOT}bin"
            "${LIBGCRYPT_ROOT}bin"
            "${LIBGPGERROR_ROOT}bin"
            "${LIBOTR_ROOT}bin"
            "${LIBTIDY_ROOT}bin"
            "${QJSON_ROOT}bin"
            "${ZLIB_ROOT}bin"
            "${SDK_PATH}openssl/bin"
            )
        if(MSVC)
            list(APPEND PATHES
                "${SDK_PATH}bin"
                "${SDK_PATH}lib/qca-qt${QT_DEFAULT_MAJOR_VERSION}/crypto"
                "${SDK_PATH}/lib/qca-qt${QT_DEFAULT_MAJOR_VERSION}/crypto"
                "${SDK_PATH}lib/qca3-qt${QT_DEFAULT_MAJOR_VERSION}/crypto"
                "${SDK_PATH}/lib/qca3-qt${QT_DEFAULT_MAJOR_VERSION}/crypto"
                "${SDK_PATH}plugins/crypto"
                "${SDK_PATH}/plugins/crypto"
                )
        endif()
        if(SEPARATE_QJDNS)
            list(APPEND PATHES
                "${QJDNS_DIR}bin"
                )
        endif()
    endif()
    #Find windeployqt prorgam and add windeploy target
    find_program(WINDEPLOYQTBIN windeployqt ${QT_BIN_DIR})
    if((NOT USE_MXE) AND (NOT "${WINDEPLOYQTBIN}" STREQUAL "WINDEPLOYQTBIN-NOTFOUND"))
        message(STATUS "WinDeployQt utility - FOUND")
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            list(APPEND WDARGS --debug)
        else()
            list(APPEND WDARGS --release)
        endif()
        list(APPEND WDARGS --libdir "${QT_LIBS_OUTPUT}")
        list(APPEND WDARGS --plugindir "${QT_PLUGINS_OUTPUT}")
        add_custom_target(windeploy
            COMMAND ${WINDEPLOYQTBIN}
            ${WDARGS}
            $<TARGET_FILE:${PROJECT_NAME}>
            WORKING_DIRECTORY
            ${DEPS_INSTALL_PREFIX}
            COMMENT
            "Preparing Qt runtime dependencies"
            )
        # Make windeploy targer run prepare-bin-libs target
        add_dependencies(windeploy ${LIBS_TARGET})
    else()
        set( ICU_LIBS
            icudt*.dll
            icuin*.dll
            icuuc*.dll
        )
        find_psi_lib("${ICU_LIBS}" "${PATHES}" "${QT_LIBS_OUTPUT}/")
        unset(ICU_LIBS)
        # Qt5 / Qt6 libraries
        set(QT_LIBAS
            Qt${QT_DEFAULT_MAJOR_VERSION}Concurrent${D}.dll
            Qt${QT_DEFAULT_MAJOR_VERSION}Core${D}.dll
            Qt${QT_DEFAULT_MAJOR_VERSION}Gui${D}.dll
            Qt${QT_DEFAULT_MAJOR_VERSION}Multimedia${D}.dll
            Qt${QT_DEFAULT_MAJOR_VERSION}MultimediaWidgets${D}.dll
            Qt${QT_DEFAULT_MAJOR_VERSION}Network${D}.dll
            Qt${QT_DEFAULT_MAJOR_VERSION}OpenGL${D}.dll
            Qt${QT_DEFAULT_MAJOR_VERSION}Positioning${D}.dll
            Qt${QT_DEFAULT_MAJOR_VERSION}PrintSupport${D}.dll
            Qt${QT_DEFAULT_MAJOR_VERSION}Qml${D}.dll
            Qt${QT_DEFAULT_MAJOR_VERSION}QmlModels${D}.dll
            Qt${QT_DEFAULT_MAJOR_VERSION}Quick${D}.dll
            Qt${QT_DEFAULT_MAJOR_VERSION}Script${D}.dll
            Qt${QT_DEFAULT_MAJOR_VERSION}Sensors${D}.dll
            Qt${QT_DEFAULT_MAJOR_VERSION}Sql${D}.dll
            Qt${QT_DEFAULT_MAJOR_VERSION}Svg${D}.dll
            Qt${QT_DEFAULT_MAJOR_VERSION}Svg${D}.dll
            Qt${QT_DEFAULT_MAJOR_VERSION}Widgets${D}.dll
            Qt${QT_DEFAULT_MAJOR_VERSION}Xml${D}.dll
            Qt${QT_DEFAULT_MAJOR_VERSION}XmlPatterns${D}.dll
            )
        if(${QT_DEFAULT_MAJOR_VERSION} LESS 6)
            list(APPEND QT_LIBAS Qt5WinExtras${D}.dll)
        endif()
        if(IS_WEBENGINE OR IS_WEBKIT)
            list(APPEND QT_LIBAS
                Qt${QT_DEFAULT_MAJOR_VERSION}WebKit${D}.dll
                Qt${QT_DEFAULT_MAJOR_VERSION}WebKitWidgets${D}.dll
                Qt${QT_DEFAULT_MAJOR_VERSION}WebChannel${D}.dll
            )
        endif()
        find_psi_lib("${QT_LIBAS}" "${QT_BIN_DIR}" "${QT_LIBS_OUTPUT}/")
        #AUDIO PLUGINS
        find_psi_lib(qtaudio_windows${D}.dll "${QT_PLUGINS_DIR}/audio" "${QT_PLUGINS_OUTPUT}/audio/")
        set(PLATFORMS_PLUGS
            qdirect2d${D}.dll
            qminimal${D}.dll
            qoffscreen${D}.dll
            qwindows${D}.dll
            )
        find_psi_lib("${PLATFORMS_PLUGS}" "${QT_PLUGINS_DIR}/platforms" "${QT_PLUGINS_OUTPUT}/platforms/")
        #PLATFORMS PLUGINS
        set(PLATFORMTHEMES_PLUGS
            qxdgdesktopportal${D}.dll
            )
        find_psi_lib("${PLATFORMTHEMES_PLUGS}" "${QT_PLUGINS_DIR}/platformthemes" "${QT_PLUGINS_OUTPUT}/platformthemes/")
        #PLATFORMTHEMES PLUGINS
        set(STYLES_PLUGS
            qwindowsvistastyle${D}.dll
            )
        find_psi_lib("${STYLES_PLUGS}" "${QT_PLUGINS_DIR}/styles" "${QT_PLUGINS_OUTPUT}/styles/")
        #STYLES PLUGINS
        set(BEARER_PLUGS
            qgenericbearer${D}.dll
            qnativewifibearer${D}.dll
            )
        find_psi_lib("${BEARER_PLUGS}" "${QT_PLUGINS_DIR}/bearer" "${QT_PLUGINS_OUTPUT}/bearer/")
        #BEARER PLUGINS
        set(GENERIC_PLUGS
            qtuiotouchplugin${D}.dll
            )
        find_psi_lib("${GENERIC_PLUGS}" "${QT_PLUGINS_DIR}/generic" "${QT_PLUGINS_OUTPUT}/generic/")
        #GENERIC PLUGINS
        set(ICONENGINES_PLUGS
            qsvgicon${D}.dll
            )
        find_psi_lib("${ICONENGINES_PLUGS}" "${QT_PLUGINS_DIR}/iconengines" "${QT_PLUGINS_OUTPUT}/iconengines/")
        #ICONENGINES PLUGINS
        set(IMAGEFORMATS_PLUGS
            qdds${D}.dll
            qgif${D}.dll
            qicns${D}.dll
            qico${D}.dll
            qjp2${D}.dll
            qjpeg${D}.dll
            qmng${D}.dll
            qsvg${D}.dll
            qtga${D}.dll
            qtiff${D}.dll
            qwbmp${D}.dll
            qwebp${D}.dll
            )
        find_psi_lib("${IMAGEFORMATS_PLUGS}" "${QT_PLUGINS_DIR}/imageformats" "${QT_PLUGINS_OUTPUT}/imageformats/")
        #IMAGEFORMATS PLUGINS
        set(MEDIASERVICE_PLUGS
            dsengine${D}.dll
            qtmedia_audioengine${D}.dll
            wmfengine${D}.dll
            )
        find_psi_lib("${MEDIASERVICE_PLUGS}" "${QT_PLUGINS_DIR}/mediaservice" "${QT_PLUGINS_OUTPUT}/mediaservice/")
        #MEDIASERVICE PLUGINS
        set(MULTIMEDIA_PLUGS
            ffmpegmediaplugin${D}.dll
            windowsmediaplugin${D}.dll
            )
        find_psi_lib("${MULTIMEDIA_PLUGS}" "${QT_PLUGINS_DIR}/multimedia" "${QT_PLUGINS_OUTPUT}/multimedia/")
        #MULTIMEDIA PLUGINS
        set(NETWORKINFORMATION_PLUGS
            qnetworklistmanager${D}.dll
            )
        find_psi_lib("${NETWORKINFORMATION_PLUGS}" "${QT_PLUGINS_DIR}/networkinformation" "${QT_PLUGINS_OUTPUT}/networkinformation/")
        #NETWORKINFORMATION PLUGINS
        set(POSITION_PLUGS
            qtposition_nmea${D}.dll
            qtposition_positionpoll${D}.dll
            qtposition_winrt${D}.dll
            )
        find_psi_lib("${POSITION_PLUGS}" "${QT_PLUGINS_DIR}/position" "${QT_PLUGINS_OUTPUT}/position/")
        #POSITION PLUGINS
        set(PLAYLISTFORMATS_PLUGS
            qtmultimedia_m3u${D}.dll
            )
        find_psi_lib("${PLAYLISTFORMATS_PLUGS}" "${QT_PLUGINS_DIR}/playlistformats" "${QT_PLUGINS_OUTPUT}/playlistformats/")
        #PLAYLISTFORMATS PLUGINS
        set(PRINTSUPPORT_PLUGS
            windowsprintersupport${D}.dll
            )
        find_psi_lib("${PRINTSUPPORT_PLUGS}" "${QT_PLUGINS_DIR}/printsupport" "${QT_PLUGINS_OUTPUT}/printsupport/")
        #PRINTSUPPORT PLUGINS
        set(SQLDRIVERS_PLUGS
            qsqlite${D}.dll
            qsqlmimer${D}.dll
            qsqlmysql${D}.dll
            qsqlodbc${D}.dll
            qsqlpsql${D}.dll
            qsqltds${D}.dll
            )
        find_psi_lib("${SQLDRIVERS_PLUGS}" "${QT_PLUGINS_DIR}/sqldrivers" "${QT_PLUGINS_OUTPUT}/sqldrivers/")
        #SQLDRIVERS PLUGINS
        set(TLS_PLUGS
            qcertonlybackend${D}.dll
            qopensslbackend${D}.dll
            qschannelbackend${D}.dll
            )
        find_psi_lib("${TLS_PLUGS}" "${QT_PLUGINS_DIR}/tls" "${QT_PLUGINS_OUTPUT}/tls/")
        #TLS PLUGINS
        if(KEYCHAIN_LIBS AND NOT BUNDLED_KEYCHAIN)
            set(KEYCHAIN_LIBS
                *qt${QT_DEFAULT_MAJOR_VERSION}keychain${D}.dll
                )
            find_psi_lib("${KEYCHAIN_LIBS}" "${PATHES}" "${QT_LIBS_OUTPUT}/")
        endif()
    endif()
    # psimedia deps
    if(BUILD_PSIMEDIA)
        set(PSIMEDIA_DEPS
            ${CMAKE_SHARED_LIBRARY_PREFIX}ffi-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}fontconfig*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gio-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}glib-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gmodule-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gobject-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstapp-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstaudio-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstbadaudio-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstbadbase-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstbase-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstnet-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstpbutils-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstreamer-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstriff-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstrtp-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstrtsp-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gsttag-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gthread-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstvideo-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstwebrtc-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstwinrt-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gthread-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}harfbuzz.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}intl-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}jpeg8.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}jpeg-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}png16-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}vorbis-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}vorbisenc-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}winpthread-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}x264-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}xml2-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}ogg-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}opus-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}orc-[0-9]*-[0-9].dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}pcre2-[0-9]*-[0-9].dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}png16.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}xml2-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}z-*.dll
        )
        set(GSTREAMER_PLUGINS
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstapp.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstaudioconvert.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstaudiomixer.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstaudioresample.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstcoreelements.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstdirectsound.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstdirectsoundsrc.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstjpeg.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstlevel.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstogg.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstopus.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstopusparse.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstplayback.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstrtp.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstrtpmanager.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstvideoconvertscale.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstvideoconvert.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstvideoscale.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstvideorate.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstvolume.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstvorbis.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstvpx.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstwasapi*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}gstwinks.dll
        )
        if(MSVC)
            set(PSIMEDIA_DEPS_DIR "${GST_SDK}/bin")
            set(GSTREAMER_PLUGINS_DIR "${GST_SDK}/lib/gstreamer-1.0")
        endif()
        if(USE_MXE)
            set(PSIMEDIA_DEPS_DIR "${CMAKE_PREFIX_PATH}/bin")
            set(GSTREAMER_PLUGINS_DIR "${CMAKE_PREFIX_PATH}/bin/gstreamer-1.0")
        endif()
        set(GST_PLUGINS_OUTPUT "${PSIMEDIA_LIBS_OUTPUT}/gstreamer-1.0/")
        find_psi_lib("${PSIMEDIA_DEPS}" "${PSIMEDIA_DEPS_DIR}" "${PSIMEDIA_LIBS_OUTPUT}/")
        # streamer plugins
        find_psi_lib("${GSTREAMER_PLUGINS}" "${GSTREAMER_PLUGINS_DIR}/" "${GST_PLUGINS_OUTPUT}")
    endif()
    list(APPEND HUNSPELL_LIBS
        ${CMAKE_SHARED_LIBRARY_PREFIX}hunspell*.dll
    )
    find_psi_lib("${HUNSPELL_LIBS}" "${PATHES}" "${PSI_LIBS_OUTPUT}")
    unset(HUNSPELL_LIBS)
    # other libs and executables
    set(LIBRARIES_LIST
        gpg.exe
        ${CMAKE_SHARED_LIBRARY_PREFIX}crypto-*.dll
        ${CMAKE_SHARED_LIBRARY_PREFIX}eay32.dll
        ${CMAKE_SHARED_LIBRARY_PREFIX}gcc_s_*.dll
        ${CMAKE_SHARED_LIBRARY_PREFIX}omemo-c.dll
        ${CMAKE_SHARED_LIBRARY_PREFIX}ssl-*.dll
        ${CMAKE_SHARED_LIBRARY_PREFIX}stdc++-*.dll
        ${CMAKE_SHARED_LIBRARY_PREFIX}winpthread-*.dll
        ${CMAKE_SHARED_LIBRARY_PREFIX}xslt-*.dll
        ${CMAKE_SHARED_LIBRARY_PREFIX}zlib*.dll
        ${CMAKE_SHARED_LIBRARY_PREFIX}zstd.dll
        legacy.dll
        ${CMAKE_SHARED_LIBRARY_PREFIX}protobuf-c${D}.dll
        ${CMAKE_SHARED_LIBRARY_PREFIX}ssleay32.dll
        ${CMAKE_SHARED_LIBRARY_PREFIX}z${D}.dll
        )
    if(USE_MXE)
        list(APPEND LIBRARIES_LIST
            ${CMAKE_SHARED_LIBRARY_PREFIX}brotlicommon.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}brotlidec.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}bz2.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}dl.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}ffi-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}freetype-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}glib-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}harfbuzz-[0-9].dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}harfbuzz-icu-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}iconv-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}intl-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}jasper*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}jpeg-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}lcms2-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}lzma-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}lzo2-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}minizip.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}mng-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}pcre2-[0-9]*-[0-9].dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}pcre16-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}png16-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}sharpyuv-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}sqlite3-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}ssp-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}tiff-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}webp-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}webpdecoder-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}webpdemux-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}webpmux-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}xml2-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}xslt-*.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}zstd.dll
            )
    endif()
    if(SEPARATE_QJDNS)
        list(APPEND LIBRARIES_LIST
            ${CMAKE_SHARED_LIBRARY_PREFIX}jdns.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}qjdns.dll
            )
    endif()
    find_psi_lib("${LIBRARIES_LIST}" "${PATHES}" "${PSI_LIBS_OUTPUT}/")
    if(NOT IRIS_BUNDLED_QCA)
        set(QCA_LIB
            ${CMAKE_SHARED_LIBRARY_PREFIX}qca-qt${QT_DEFAULT_MAJOR_VERSION}${D}.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}qca3-qt${QT_DEFAULT_MAJOR_VERSION}${D}.dll
            )
        # qca and plugins
        set(QCA_PLUGINS
            ${CMAKE_SHARED_LIBRARY_PREFIX}qca-gnupg${D}.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}qca-ossl${D}.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}qca3-gnupg${D}.dll
            ${CMAKE_SHARED_LIBRARY_PREFIX}qca3-ossl${D}.dll
            )
        find_psi_lib("${QCA_LIB}" "${PATHES}" "${PSI_LIBS_OUTPUT}/")
        find_psi_lib("${QCA_PLUGINS}" "${PATHES}" "${QT_PLUGINS_OUTPUT}/crypto/")
    endif()
    if (NOT IRIS_BUNDLED_USRSCTP)
        set(USRSCTP_LIB *usrsctp${D}.dll)
        find_psi_lib("${USRSCTP_LIB}" "${PATHES}" "${PSI_LIBS_OUTPUT}/")
    endif()
    unset(LIBRARIES_LIST)
    copy("${PROJECT_SOURCE_DIR}/win32/qt.conf" "${DEPS_INSTALL_PREFIX}/" "${LIBS_TARGET}")
endif()
