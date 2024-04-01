cmake_minimum_required(VERSION 3.10.0)
if(WIN32)
    set(LIBS_TARGET prepare-bin-libs)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(D "d")
    endif()
    # Get Qt installation path
    string(REGEX REPLACE "([^ ]+)[/\\].*" "\\1" QT_BIN_DIR_TMP "${QT_MOC_EXECUTABLE}")
    string(REGEX REPLACE "\\\\" "/" QT_BIN_DIR "${QT_BIN_DIR_TMP}")
    unset(QT_BIN_DIR_TMP)
    function(find_psi_lib LIBLIST PATHES OUTPUT_PATH)
        set(_LIBS ${LIBLIST})
        set(_PATHES ${PATHES})
        set(_OUTPUT_PATH ${OUTPUT_PATH})
        set(FIXED_PATHES "")
        foreach(_path ${_PATHES})
            string(REGEX REPLACE "//bin" "/bin" tmp_path "${_path}")
            list(APPEND FIXED_PATHES ${tmp_path})
        endforeach()
        if(NOT USE_MXE)
            set(ADDITTIONAL_FLAG "NO_DEFAULT_PATH")
        else()
            set(ADDITTIONAL_FLAG "")
        endif()
        foreach(_liba ${_LIBS})
            set(_library _library-NOTFOUND)
            find_file( _library ${_liba} PATHS ${FIXED_PATHES} ${ADDITTIONAL_FLAG})
            if( NOT "${_library}" STREQUAL "_library-NOTFOUND" )
                message("library found ${_library}")
                copy("${_library}" "${_OUTPUT_PATH}" "${LIBS_TARGET}")
            endif()
        endforeach()
        unset(_LIBS)
        unset(_PATHES)
        unset(_OUTPUT_PATH)
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
    set(PSIMEDIA_FOUND OFF)
    #Set paths
    list(APPEND PATHES
        ${QT_BIN_DIR}
        ${QCA_DIR}bin
        ${QCA_DIR}/bin
        ${QT_PLUGINS_DIR}/crypto
        ${QCA_DIR}lib/qca-qt${QT_DEFAULT_MAJOR_VERSION}/crypto
        ${QCA_DIR}lib/Qca-qt${QT_DEFAULT_MAJOR_VERSION}/crypto
        )
    if(USE_MXE)
        list(APPEND PATHES
            ${CMAKE_PREFIX_PATH}/bin
            ${CMAKE_PREFIX_PATH}/lib
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
    if(NOT "${WINDEPLOYQTBIN}" STREQUAL "WINDEPLOYQTBIN-NOTFOUND")
        message(STATUS "WinDeployQt utility - FOUND")
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            list(APPEND WDARGS --debug)
        else()
            list(APPEND WDARGS --release)
        endif()
        list(APPEND WDARGS --plugindir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qtplugins")
        add_custom_target(windeploy
            COMMAND ${WINDEPLOYQTBIN}
            ${WDARGS}
            $<TARGET_FILE:${PROJECT_NAME}>
            WORKING_DIRECTORY
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
            COMMENT
            "Preparing Qt runtime dependencies"
            )
    else()
        #if windeployqt not found search libs manually
        # required libraries
        set( ICU_LIBS_PREFIXES
            icudt5
            icudt6
            icudt7
            icuin5
            icuin6
            icuin7
            icuuc5
            icuuc6
            icuuc7
            )
        set( ICU_LIBS "" )
        #hack to find icu libs with name template icu\W{2}[1..9]-0.dll
        foreach( icu_prefix ${ICU_LIBS_PREFIXES} )
            foreach( icu_counter RANGE 9 )
                list(APPEND ICU_LIBS
                    "${icu_prefix}${icu_counter}.dll"
                    )
            endforeach()
        endforeach()
        find_psi_lib("${ICU_LIBS}" "${PATHES}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/")
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
            Qt${QT_DEFAULT_MAJOR_VERSION}WebChannel${D}.dll
            Qt${QT_DEFAULT_MAJOR_VERSION}Widgets${D}.dll
            Qt${QT_DEFAULT_MAJOR_VERSION}Xml${D}.dll
            Qt${QT_DEFAULT_MAJOR_VERSION}XmlPatterns${D}.dll
            )
        if(${QT_DEFAULT_MAJOR_VERSION} LESS 6)
            list(APPEND QT_LIBAS
                Qt5WebKit${D}.dll
                Qt5WebKitWidgets${D}.dll
                Qt5WinExtras${D}.dll
            )
        endif()
        find_psi_lib("${QT_LIBAS}" "${QT_BIN_DIR}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/")
        #
        find_psi_lib(qtaudio_windows${D}.dll "${QT_PLUGINS_DIR}/audio/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qtplugins/audio/")
        set(PLATFORMS_PLUGS
            qdirect2d${D}.dll
            qminimal${D}.dll
            qoffscreen${D}.dll
            qwindows${D}.dll
            )
        find_psi_lib("${PLATFORMS_PLUGS}" "${QT_PLUGINS_DIR}/platforms/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qtplugins/platforms/")
        #
        set(PLATFORMTHEMES_PLUGS
            qxdgdesktopportal${D}.dll
            )
        find_psi_lib("${PLATFORMTHEMES_PLUGS}" "${QT_PLUGINS_DIR}/platformthemes/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qtplugins/platformthemes/")
        #
        set(STYLES_PLUGS
            qwindowsvistastyle${D}.dll
            )
        find_psi_lib("${STYLES_PLUGS}" "${QT_PLUGINS_DIR}/styles/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qtplugins/styles/")
        #
        set(BEARER_PLUGS
            qgenericbearer${D}.dll
            qnativewifibearer${D}.dll
            )
        find_psi_lib("${BEARER_PLUGS}" "${QT_PLUGINS_DIR}/bearer/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qtplugins/bearer/")
        #
        set(GENERIC_PLUGS
            qtuiotouchplugin${D}.dll
            )
        find_psi_lib("${GENERIC_PLUGS}" "${QT_PLUGINS_DIR}/generic/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qtplugins/generic/")
        #
        set(ICONENGINES_PLUGS
            qsvgicon${D}.dll
            )
        find_psi_lib("${ICONENGINES_PLUGS}" "${QT_PLUGINS_DIR}/iconengines/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qtplugins/iconengines/")
        #
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
        find_psi_lib("${IMAGEFORMATS_PLUGS}" "${QT_PLUGINS_DIR}/imageformats/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qtplugins/imageformats/")
        #
        set(MEDIASERVICE_PLUGS
            dsengine${D}.dll
            qtmedia_audioengine${D}.dll
            wmfengine${D}.dll
            )
        find_psi_lib("${MEDIASERVICE_PLUGS}" "${QT_PLUGINS_DIR}/mediaservice/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qtplugins/mediaservice/")
        #
        set(MULTIMEDIA_PLUGS
            ffmpegmediaplugin${D}.dll
            windowsmediaplugin${D}.dll
            )
        find_psi_lib("${MULTIMEDIA_PLUGS}" "${QT_PLUGINS_DIR}/multimedia/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qtplugins/multimedia/")
        #
        set(NETWORKINFORMATION_PLUGS
            qnetworklistmanager${D}.dll
            )
        find_psi_lib("${NETWORKINFORMATION_PLUGS}" "${QT_PLUGINS_DIR}/networkinformation/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qtplugins/networkinformation/")
        #
        set(POSITION_PLUGS
            qtposition_nmea${D}.dll
            qtposition_positionpoll${D}.dll
            qtposition_winrt${D}.dll
            )
        find_psi_lib("${POSITION_PLUGS}" "${QT_PLUGINS_DIR}/position/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qtplugins/position/")
        #
        set(PLAYLISTFORMATS_PLUGS
            qtmultimedia_m3u${D}.dll
            )
        find_psi_lib("${PLAYLISTFORMATS_PLUGS}" "${QT_PLUGINS_DIR}/playlistformats/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qtplugins/playlistformats/")
        #
        set(PRINTSUPPORT_PLUGS
            windowsprintersupport${D}.dll
            )
        find_psi_lib("${PRINTSUPPORT_PLUGS}" "${QT_PLUGINS_DIR}/printsupport/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qtplugins/printsupport/")
        #
        set(SQLDRIVERS_PLUGS
            qsqlite${D}.dll
            qsqlmimer${D}.dll
            qsqlodbc${D}.dll
            qsqlpsql${D}.dll
            )
        find_psi_lib("${SQLDRIVERS_PLUGS}" "${QT_PLUGINS_DIR}/sqldrivers/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qtplugins/sqldrivers/")
        #
        set(TLS_PLUGS
            qcertonlybackend${D}.dll
            qopensslbackend${D}.dll
            qschannelbackend${D}.dll
            )
        find_psi_lib("${TLS_PLUGS}" "${QT_PLUGINS_DIR}/tls/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qtplugins/tls/")
        #
        if(KEYCHAIN_LIBS)
            set(KEYCHAIN_LIBS
                qt${QT_DEFAULT_MAJOR_VERSION}keychain${D}.dll
                libqt${QT_DEFAULT_MAJOR_VERSION}keychain${D}.dll
                )
            find_psi_lib("${KEYCHAIN_LIBS}" "${PATHES}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/")
        endif()
    endif()
    # psimedia deps
    if(BUILD_PSIMEDIA)
        if(MSVC)
            set(PSIMEDIA_DEPS
                ffi-7.dll
                gio-2.0-0.dll
                glib-2.0-0.dll
                gmodule-2.0-0.dll
                gobject-2.0-0.dll
                gstapp-1.0-0.dll
                gstaudio-1.0-0.dll
                gstbadaudio-1.0-0.dll
                gstbadbase-1.0-0.dll
                gstbase-1.0-0.dll
                gstnet-1.0-0.dll
                gstpbutils-1.0-0.dll
                gstreamer-1.0-0.dll
                gstriff-1.0-0.dll
                gstrtp-1.0-0.dll
                gstrtsp-1.0.0.dll
                gsttag-1.0-0.dll
                gstvideo-1.0-0.dll
                gstwebrtc-1.0.0.dll
                gstwinrt-1.0.0.dll
                gthread-2.0-0.dll
                libgcc_s_sjlj-1.dll
                libharfbuzz-0.dll
                harfbuzz.dll
                libiconv-2.dll
                intl-8.dll
                libjpeg-8.dll
                libogg-0.dll
                ogg-0.dll
                libopus-0.dll
                opus-0.dll
                orc-0.4-0.dll
                libpng16-16.dll
                libvorbis-0.dll
                libvorbisenc-2.dll
                libwinpthread-1.dll
                libx264-157.dll
                libxml2-2.dll
                z-1.dll
                )
            set(GSTREAMER_PLUGINS
                gstapp.dll
                gstaudioconvert.dll
                gstaudiomixer.dll
                gstaudioresample.dll
                gstcoreelements.dll
                gstdirectsound.dll
                gstdirectsoundsrc.dll
                gstjpeg.dll
                gstlevel.dll
                gstogg.dll
                gstopus.dll
                gstopusparse.dll
                gstplayback.dll
                gstrtp.dll
                gstrtpmanager.dll
                gstvideoconvertscale.dll
                gstvideoconvert.dll
                gstvideorate.dll
                gstvideoscale.dll
                gstvolume.dll
                gstvorbis.dll
                gstvpx.dll
                gstwasapi.dll
                gstwinks.dll
                )
            set(PSIMEDIA_DEPS_DIR "${GST_SDK}/bin")
            set(GSTREAMER_PLUGINS_DIR "${GST_SDK}/lib/gstreamer-1.0")
            set(GST_PLUGINS_OUTPUT "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/gstreamer-1.0/")
        endif()
        if(USE_MXE)
            set(PSIMEDIA_DEPS
                libffi-6.dll
                libfontconfig-1.dll
                libgio-2.0-0.dll
                libgmodule-2.0-0.dll
                libgobject-2.0-0.dll
                libgstapp-1.0-0.dll
                libgstaudio-1.0-0.dll
                libgstbadaudio-1.0-0.dll
                libgstbadbase-1.0-0.dll
                libgstbase-1.0-0.dll
                libgstnet-1.0-0.dll
                libgstpbutils-1.0-0.dll
                libgstreamer-1.0-0.dll
                libgstriff-1.0-0.dll
                libgstrtp-1.0-0.dll
                libgstrtsp-1.0.0.dll
                libgsttag-1.0-0.dll
                libgstvideo-1.0-0.dll
                libgthread-2.0-0.dll
                libgstwebrtc-1.0.0.dll
                libogg-0.dll
                libopus-0.dll
                libvorbis-0.dll
                libvorbisenc-2.dll
                libx264-155.dll
                )
            set(GSTREAMER_PLUGINS
                libgstapp.dll
                libgstaudioconvert.dll
                libgstaudiomixer.dll
                libgstaudioresample.dll
                libgstcoreelements.dll
                libgstdirectsound.dll
                libgstdirectsoundsink.dll
                libgstdirectsoundsrc.dll
                libgstjpeg.dll
                libgstlevel.dll
                libgstogg.dll
                libgstopus.dll
                libgstopusparse.dll
                libgstplayback.dll
                libgstrtp.dll
                libgstrtpmanager.dll
                libgstvideoconvert.dll
                libgstvideorate.dll
                libgstvideoscale.dll
                libgstvolume.dll
                libgstvorbis.dll
                libgstvpx.dll
                libgstwasapi.dll
                libgstwinks.dll
                )
            set(PSIMEDIA_DEPS_DIR "${CMAKE_PREFIX_PATH}/bin")
            set(GSTREAMER_PLUGINS_DIR "${CMAKE_PREFIX_PATH}/bin/gstreamer-1.0")
            set(GST_PLUGINS_OUTPUT "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/gstreamer-1.0/")
        endif()
        find_psi_lib("${PSIMEDIA_DEPS}" "${PSIMEDIA_DEPS_DIR}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/")
        # streamer plugins
        find_psi_lib("${GSTREAMER_PLUGINS}" "${GSTREAMER_PLUGINS_DIR}/" "${GST_PLUGINS_OUTPUT}")
    endif()
    #hack to find hunspell libs with name template libhunspell-1.[1..9]-0.dll
    foreach( hunsp_counter RANGE 9 )
        list(APPEND HUNSPELL_LIBS
            "libhunspell-1.${hunsp_counter}-0.dll"
            )
    endforeach()
    find_psi_lib("${HUNSPELL_LIBS}" "${PATHES}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/")
    unset(HUNSPELL_LIBS)
    # other libs and executables
    set( LIBRARIES_LIST
        gpg.exe
        libcrypto-1_1-x64.dll
        libcrypto-3-x64.dll
        libcrypto-1_1.dll
        libcrypto-3.dll
        libeay32.dll
        libgcc_s_dw2-1.dll
        libgcc_s_seh-1.dll
        libgcc_s_sjlj-1.dll
        libgcrypt-11.dll
        libgcrypt-20.dll
        libgpg-error-0.dll
        libhunspell.dll
        libotr-5.dll
        libotr.dll
        libsignal-protocol-c.dll
        libssl-1_1-x64.dll
        libssl-3-x64.dll
        libssl-1_1.dll
        libssl-3.dll
        libstdc++-6.dll
        libtidy.dll
        libwinpthread-1.dll
        libxslt-1.dll
        libzlib.dll
        libzstd.dll
        ssleay32.dll
        zlib1.dll
        )

    if(MSVC)
        list(APPEND LIBRARIES_LIST
            libotr${D}.dll
            tidy${D}.dll
            zlib1${D}.dll
            )
    endif()

    if(USE_MXE)
        list(APPEND LIBRARIES_LIST
            libbrotlicommon.dll
            libbrotlidec.dll
            libbz2.dll
            libdl.dll
            libffi-7.dll
            libfreetype-6.dll
            libglib-2.0-0.dll
            libgpg-error6-0.dll
            libharfbuzz-0.dll
            libharfbuzz-icu-0.dll
            libiconv-2.dll
            libintl-8.dll
            libjasper-1.dll
            libjasper.dll
            libjpeg-9.dll
            liblcms2-2.dll
            liblzma-5.dll
            liblzo2-2.dll
            libminizip.dll
            libmng-2.dll
            libpcre-1.dll
            libpcre16-0.dll
            libpcre2-16-0.dll
            libpng16-16.dll
            libsharpyuv-0.dll
            libsqlite3-0.dll
            libssp-0.dll
            libtiff-5.dll
            libtiff-6.dll
            libwebp-5.dll
            libwebp-7.dll
            libwebpdecoder-1.dll
            libwebpdecoder-3.dll
            libwebpdemux-1.dll
            libwebpdemux-2.dll
            libwebpmux-3.dll
            libxml2-2.dll
            libzstd.dll
            )
    endif()
    if(SEPARATE_QJDNS)
        list(APPEND LIBRARIES_LIST
            libjdns.dll
            libqjdns.dll
            )
    endif()
    find_psi_lib("${LIBRARIES_LIST}" "${PATHES}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/")
    if(NOT BUNDLED_QCA)
        set(QCA_LIB
            libqca-qt${QT_DEFAULT_MAJOR_VERSION}${D}.dll
            )
        # qca and plugins
        set(QCA_PLUGINS
            libqca-gnupg${D}.dll
            libqca-ossl${D}.dll
            )
        if(MSVC)
            set(QCA_LIB
                qca-qt${QT_DEFAULT_MAJOR_VERSION}${D}.dll
                )
            list(APPEND QCA_PLUGINS
                qca-gnupg${D}.dll
                qca-ossl${D}.dll
                )
        endif()
        find_psi_lib("${QCA_LIB}" "${PATHES}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/")
        find_psi_lib("${QCA_PLUGINS}" "${PATHES}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qtplugins/crypto/")
    endif()
    if (NOT BUNDLED_USRSCTP)
            set(USRSCTP_LIB libusrsctp${D}.dll)
        if(MSVC)
            set(USRSCTP_LIB usrsctp${D}.dll)
        endif()
        find_psi_lib("${USRSCTP_LIB}" "${PATHES}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/")
    endif()
    copy("${PROJECT_SOURCE_DIR}/win32/qt.conf" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" "${LIBS_TARGET}")
endif()
