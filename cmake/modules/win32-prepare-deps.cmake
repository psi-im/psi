cmake_minimum_required(VERSION 3.1.0)
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
        set(_LIBS "")
        set(_PATHES "")
        set(_OUTPUT_PATH "")
    endfunction()
    set(SDK_PREFIX "")
    get_filename_component(QT_DIR ${QT_BIN_DIR} DIRECTORY)
    set(QT_PLUGINS_DIR ${QT_DIR}/plugins)
    set(QT_TRANSLATIONS_DIR ${QT_DIR}/translations)
    set(PSIMEDIA_FOUND OFF)
    #Set paths
    list(APPEND PATHES
        ${QT_BIN_DIR}
        ${QCA_DIR}bin
        ${QCA_DIR}/bin
        ${QT_PLUGINS_DIR}/crypto
        ${QCA_DIR}lib/qca-qt5/crypto
        ${QCA_DIR}lib/Qca-qt5/crypto
    )
    if(USE_MXE)
        list(APPEND PATHES
            ${CMAKE_PREFIX_PATH}/bin
            ${CMAKE_PREFIX_PATH}/lib
        )
    endif()
    if(EXISTS "${SDK_PATH}")
        list(APPEND PATHES
            "${IDN_ROOT}bin"
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
                "${SDK_PATH}lib/qca-qt5/crypto"
                "${SDK_PATH}/lib/qca-qt5/crypto"
            )
        endif()
        if(SEPARATE_QJDNS)
            list(APPEND PATHES
                "${QJDNS_DIR}bin"
            )
        endif()
    endif()
    #Find windeployqt prorgam
    find_program(WINDEPLOYQTBIN windeployqt ${QT_BIN_DIR})
    if(NOT "${WINDEPLOYQTBIN}" STREQUAL "WINDEPLOYQTBIN-NOTFOUND")
        message(STATUS "WinDeployQt utility - FOUND")
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            list(APPEND WDARGS --debug)
        else()
            list(APPEND WDARGS --release)
        endif()
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${WINDEPLOYQTBIN}
            ARGS
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
            icuin5
            icuuc5
        )
        set( ICU_LIBS "" )
        foreach( icu_prefix ${ICU_LIBS_PREFIXES} )
            foreach( icu_counter RANGE 9 )
                    list(APPEND ICU_LIBS
                        "${icu_prefix}${icu_counter}.dll"
                    )
            endforeach()
        endforeach()
        find_psi_lib("${ICU_LIBS}" "${PATHES}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/")
        # Qt5 libraries
        set(QT_LIBAS
            Qt5Core${D}.dll
            Qt5Gui${D}.dll
            Qt5Widgets${D}.dll
            Qt5Svg${D}.dll
            Qt5Network${D}.dll
            Qt5Svg${D}.dll
            Qt5Script${D}.dll
            Qt5Xml${D}.dll
            Qt5XmlPatterns${D}.dll
            Qt5Sql${D}.dll
            Qt5WebKit${D}.dll
            Qt5WebKitWidgets${D}.dll
            Qt5Qml${D}.dll
            Qt5Quick${D}.dll
            Qt5Positioning${D}.dll
            Qt5WebChannel${D}.dll
            Qt5Multimedia${D}.dll
            Qt5MultimediaWidgets${D}.dll
            Qt5Concurrent${D}.dll
            Qt5Sensors${D}.dll
            Qt5OpenGL${D}.dll
            Qt5PrintSupport${D}.dll
            Qt5WinExtras${D}.dll
        )
        find_psi_lib("${QT_LIBAS}" "${QT_BIN_DIR}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/")
        #
        find_psi_lib(qtaudio_windows${D}.dll ${QT_PLUGINS_DIR}/audio/ ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qtplugins/audio/)
        set(PLATFORMS_PLUGS
            qminimal${D}.dll
            qoffscreen${D}.dll
            qwindows${D}.dll
        )
        find_psi_lib("${PLATFORMS_PLUGS}" "${QT_PLUGINS_DIR}/platforms/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qtplugins/platforms/")
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
        )
        find_psi_lib("${SQLDRIVERS_PLUGS}" "${QT_PLUGINS_DIR}/sqldrivers/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qtplugins/sqldrivers/")
        #
        if(KEYCHAIN_LIBS)
            set(KEYCHAIN_LIBS
                qt5keychain.dll
                libqt5keychain.dll
            )
            find_psi_lib("${KEYCHAIN_LIBS}" "${PATHES}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/")
        endif()
        # Qt translations
        if(EXISTS "${QT_TRANSLATIONS_DIR}")
            file(GLOB QT_TRANSLATIONS "${QT_TRANSLATIONS_DIR}/q*.qm")
            foreach(FILE ${QT_TRANSLATIONS})
                copy("${FILE}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/translations/" ${LIBS_TARGET})
            endforeach()
        endif()
    endif()
    # psimedia
    if(EXISTS "${PSIMEDIA_DIR}")
        set(PSIMEDIA_LIB_NAME "libgstprovider${D}.dll")
        if(MSVC)
            set(PSIMEDIA_LIB_NAME "gstprovider${D}.dll")
        endif()
        find_program(PSIMEDIA_PLUGIN ${PSIMEDIA_LIB_NAME} PATHS "${PSIMEDIA_DIR}")
        if(NOT "${PSIMEDIA_PLUGIN}" STREQUAL "PSIMEDIA_PLUGIN-NOTFOUND")
            get_filename_component(PSIMEDIA_PATH "${PSIMEDIA_PLUGIN}" DIRECTORY)
            message("library found ${PSIMEDIA_PLUGIN}")
            copy(${PSIMEDIA_PLUGIN} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" ${LIBS_TARGET})
            set(PSIMEDIA_FOUND ON)
        endif()
    endif()
    # psimedia deps
    if(PSIMEDIA_FOUND)
        if(NOT MSVC)
            find_program(PSIMEDIA_DEPS_PATH "libgstvideo-0.10-0.dll" PATHS ${GST_SDK}/bin )
            if(NOT "${PSIMEDIA_DEPS_PATH}" STREQUAL "PSIMEDIA_DEPS_PATH-NOTFOUND")
                get_filename_component(PSIMEDIA_DEPS_DIR ${PSIMEDIA_DEPS_PATH} DIRECTORY)
            endif()
        endif()

        set(PSIMEDIA_DEPS
            libjpeg-9.dll
            libgettextlib-0-19-6.dll
            libogg-0.dll
            libtheoradec-1.dll
            libgettextpo-0.dll
            liborc-0.4-0.dll
            libtheoraenc-1.dll
            libasprintf-0.dll
            libgettextsrc-0-19-6.dll
            liborc-test-0.4-0.dll
            libvorbis-0.dll
            libcharset-1.dll
            libgio-2.0-0.dll
            libspeex-1.dll
            libvorbisenc-2.dll
            libglib-2.0-0.dll
            libgthread-2.0-0.dll
            libspeexdsp-1.dll
            libvorbisfile-3.dll
            libffi-6.dll
            libgmodule-2.0-0.dll
            libgobject-2.0-0.dll
            libintl-8.dll
            libtheora-0.dll
            libgstapp-0.10-0.dll
            libgstaudio-0.10-0.dll
            libgstbase-0.10-0.dll
            libgstcdda-0.10-0.dll
            libgstcontroller-0.10-0.dll
            libgstdataprotocol-0.10-0.dll
            libgstfft-0.10-0.dll
            libgstinterfaces-0.10-0.dll
            libgstnet-0.10-0.dll
            libgstnetbuffer-0.10-0.dll
            libgstpbutils-0.10-0.dll
            libgstreamer-0.10-0.dll
            libgstriff-0.10-0.dll
            libgstrtp-0.10-0.dll
            libgstrtsp-0.10-0.dll
            libgstsdp-0.10-0.dll
            libgsttag-0.10-0.dll
            libgstvideo-0.10-0.dll
        )
        if(MSVC)
            set(PSIMEDIA_DEPS
                libffi-7.dll
                libgio-2.0-0.dll
                libglib-2.0-0.dll
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
                libgsttag-1.0-0.dll
                libgstvideo-1.0-0.dll
                libgthread-2.0-0.dll
                libharfbuzz-0.dll
                libiconv-2.dll
                libintl-8.dll
                libjpeg-8.dll
                libogg-0.dll
                libopus-0.dll
                liborc-0.4-0.dll
                libpng16-16.dll
                libtheora-0.dll
                libtheoradec-1.dll
                libtheoraenc-1.dll
                libvorbis-0.dll
                libvorbisenc-2.dll
                libwinpthread-1.dll
                libxml2-2.dll
                libz.dll
            )
            set(GSTREAMER_PLUGINS
                libgstapp.dll
                libgstaudioconvert.dll
                libgstaudiomixer.dll
                libgstaudioresample.dll
                libgstcoreelements.dll
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
                libgsttheora.dll
                libgstvideoconvert.dll
                libgstvideorate.dll
                libgstvideoscale.dll
                libgstvolume.dll
                libgstvorbis.dll
                libgstwasapi.dll
                libgstwinks.dll
            )
            set(PSIMEDIA_DEPS_DIR "${GST_SDK}/bin")
            set(GSTREAMER_PLUGINS_DIR "${GST_SDK}/lib/gstreamer-1.0")
            set(GST_PLUGINS_OUTPUT "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/lib/gstreamer-1.0/")
        else()
            set(GSTREAMER_PLUGINS_DIR "${PSIMEDIA_DIR}/gstreamer-0.10")
            file(GLOB GSTREAMER_PLUGINS "${GSTREAMER_PLUGINS_DIR}/*.dll")
            set(GST_PLUGINS_OUTPUT "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/gstreamer-0.10/")
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
                libgsttag-1.0-0.dll
                libgstvideo-1.0-0.dll
                libgthread-2.0-0.dll
                libogg-0.dll
                libopus-0.dll
                libtheora-0.dll
                libtheoradec-1.dll
                libtheoraenc-1.dll
                libvorbis-0.dll
                libvorbisenc-2.dll
            )
            set(GSTREAMER_PLUGINS
                libgstapp.dll
                libgstdirectsoundsrc.dll
                libgstplayback.dll
                libgstvideoscale.dll
                libgstaudioconvert.dll
                libgstjpeg.dll
                libgstrtp.dll
                libgstvolume.dll
                libgstaudiomixer.dll
                libgstlevel.dll
                libgstrtpmanager.dll
                libgstvorbis.dll
                libgstaudioresample.dll
                libgstogg.dll
                libgsttheora.dll
                libgstwasapi.dll
                libgstcoreelements.dll
                libgstopus.dll
                libgstvideoconvert.dll
                libgstwinks.dll
                libgstdirectsoundsink.dll
                libgstopusparse.dll
                libgstvideorate.dll
            )
            set(PSIMEDIA_DEPS_DIR "${CMAKE_PREFIX_PATH}/bin")
            set(GSTREAMER_PLUGINS_DIR "${CMAKE_PREFIX_PATH}/bin/gstreamer-1.0")
            set(GST_PLUGINS_OUTPUT "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/lib/gstreamer-1.0/")
        endif()
        find_psi_lib("${PSIMEDIA_DEPS}" "${PSIMEDIA_DEPS_DIR}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/")
        # streamer plugins
        find_psi_lib("${GSTREAMER_PLUGINS}" "${GSTREAMER_PLUGINS_DIR}/" "${GST_PLUGINS_OUTPUT}")
    endif()
    # other libs and executables
    set( LIBRARIES_LIST
        libgcc_s_sjlj-1.dll
        libgcc_s_dw2-1.dll
        libgcc_s_seh-1.dll
        libstdc++-6.dll
        libwinpthread-1.dll
        gpg.exe
        libgcrypt-11.dll
        libgcrypt-20.dll
        libcrypto-1_1.dll
        libcrypto-1_1-x64.dll
        libssl-1_1.dll
        libssl-1_1-x64.dll
        libotr.dll
        libotr-5.dll
        libsignal-protocol-c.dll
        libtidy.dll
        libzlib.dll
        zlib1.dll
        libgpg-error-0.dll
        libidn-11.dll
        libidn-12.dll
        libhunspell.dll
        libhunspell-1.3-0.dll
        libhunspell-1.4-0.dll
        libhunspell-1.5-0.dll
        libhunspell-1.6-0.dll
        libhunspell-1.7-0.dll
        libhunspell-1.8-0.dll
        libeay32.dll
        libqca-qt5${D}.dll
        ssleay32.dll
        libxslt-1.dll
    )

    if(MSVC)
        list(APPEND LIBRARIES_LIST
            otr${D}.dll
            tidy${D}.dll
            zlib${D}.dll
            idn${D}.dll
            qca-qt5${D}.dll
        )
    endif()

    if(USE_MXE)
        list(APPEND LIBRARIES_LIST
            libgpg-error6-0.dll
            libbz2.dll
            libfreetype-6.dll
            libglib-2.0-0.dll
            libharfbuzz-0.dll
            libharfbuzz-icu-0.dll
            libiconv-2.dll
            libintl-8.dll
            libjasper.dll
            libjasper-1.dll
            libjpeg-9.dll
            liblcms2-2.dll
            liblzma-5.dll
            liblzo2-2.dll
            libmng-2.dll
            libpcre16-0.dll
            libpcre2-16-0.dll
            libpcre-1.dll
            libpng16-16.dll
            libssp-0.dll
            libsqlite3-0.dll
            libtiff-5.dll
            libwebp-5.dll
            libwebp-7.dll
            libwebpdecoder-1.dll
            libwebpdemux-1.dll
            libwebpdecoder-3.dll
            libwebpdemux-2.dll
            libxml2-2.dll
        )
    endif()
    if(SEPARATE_QJDNS)
        list(APPEND LIBRARIES_LIST
            libqjdns.dll
            libjdns.dll
        )
    endif()
    find_psi_lib("${LIBRARIES_LIST}" "${PATHES}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/")
    # qca and plugins
    set(QCA_PLUGINS
        libqca-ossl${D}.dll
        libqca-gnupg${D}.dll
    )
    if(MSVC)
        list(APPEND QCA_PLUGINS
            qca-ossl${D}.dll
            qca-gnupg${D}.dll
        )
    endif()
    find_psi_lib("${QCA_PLUGINS}" "${PATHES}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qtplugins/crypto/")
    copy("${PROJECT_SOURCE_DIR}/win32/qt.conf" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" "${LIBS_TARGET}")
endif()
