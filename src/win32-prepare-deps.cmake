cmake_minimum_required(VERSION 2.8.12)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND WIN32)
	set(D "d")
endif()
if(WIN32)
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
				copy("${_library}" "${_OUTPUT_PATH}" prepare-bin-libs)
			endif()
		endforeach()
		set(_LIBS "")
		set(_PATHES "")
		set(_OUTPUT_PATH "")
	endfunction()
	set(SDK_PREFIX "")
	set(QCA_LIB_SUFF "-qt5")
	get_filename_component(QT_DIR ${QT_BIN_DIR} DIRECTORY)
	set(QT_PLUGINS_DIR ${QT_DIR}/plugins)
	set(QT_TRANSLATIONS_DIR ${QT_DIR}/translations)
	find_program(WINDEPLOYQTBIN windeployqt ${QT_BIN_DIR})
	if(NOT "${WINDEPLOYQTBIN}" STREQUAL "WINDEPLOYQTBIN-NOTFOUND")
		message(STATUS "WinDeployQt utility - FOUND")
		if(CMAKE_BUILD_TYPE STREQUAL "Debug")
			list(APPEND WDARGS --debug)
		elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
			list(APPEND WDARGS --release-with-debug-info)
		elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
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

		# required libraries
		set( ICU_LIBS_PREFIXES
			icudt5
			icuin5
			icuuc5
		)
		set( ICU_LIBS "" )
		foreach( icu_prefix ${ICU_LIBS_PREFIXES} )
			foreach( icu_counter RANGE 9 )
			find_file( ${icu_prefix}${icu_counter} "${icu_prefix}${icu_counter}.dll" )
				if( NOT "${${icu_prefix}${icu_counter}}" STREQUAL "${icu_prefix}${icu_counter}-NOTFOUND" )
					list(APPEND ICU_LIBS
						"${icu_prefix}${icu_counter}.dll"
					)
				endif()
			endforeach()
		endforeach()
		find_psi_lib("${ICU_LIBS}" "${QT_BIN_DIR}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/")
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
		)
		find_psi_lib("${QT_LIBAS}" "${QT_BIN_DIR}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/")
		find_psi_lib(qtaudio_windows${D}.dll ${QT_PLUGINS_DIR}/audio/ ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/audio/)
		set(PLATFORM_PLUGS
			qminimal${D}.dll
			qoffscreen${D}.dll
			qwindows${D}.dll
		)
		find_psi_lib("${PLATFORM_PLUGS}" "${QT_PLUGINS_DIR}/platforms/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/platforms/")
		set(STYLE_PLUGS
			qwindowsvistastyle${D}.dll
		)
		find_psi_lib("${STYLE_PLUGS}" "${QT_PLUGINS_DIR}/styles/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/styles/")
		set(BEARER_PLUGS
			qgenericbearer${D}.dll
			qnativewifibearer${D}.dll
		)
		find_psi_lib("${BEARER_PLUGS}" "${QT_PLUGINS_DIR}/bearer/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bearer/")
		set(IMAGE_PLUGS
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
		find_psi_lib("${IMAGE_PLUGS}" "${QT_PLUGINS_DIR}/imageformats/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imageformats/")
		find_psi_lib("windowsprintersupport${D}.dll" "${QT_PLUGINS_DIR}/printsupport/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/printsupport/")
		find_psi_lib("qsqlite${D}.dll" "${QT_PLUGINS_DIR}/sqldrivers/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sqldrivers/")
		# Qt translations
		file(GLOB QT_TRANSLATIONS "${QT_TRANSLATIONS_DIR}/qt_*.qm")
		foreach(FILE ${QT_TRANSLATIONS})
			if(NOT FILE MATCHES "_help_")
				copy(${FILE} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/translations/" prepare-bin-libs)
			endif()
		endforeach()
		file(GLOB QT_TRANSLATIONS "${QT_TRANSLATIONS_DIR}/qtbase_*.qm")
		foreach(FILE ${QT_TRANSLATIONS})
			copy(${FILE} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/translations/" prepare-bin-libs)
		endforeach()
	endif()
	# psimedia
	if(EXISTS "${PSIMEDIA_DIR}")
		find_program(PSIMEDIA_PATH libgstprovider${D}.dll PATHS ${PSIMEDIA_DIR}/plugins )
		copy(${PSIMEDIA_DIR} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	endif()
	# psimedia deps
	find_program(PSIMEDIA_DEPS_PATH libgstvideo-0.10-0.dll PATHS ${GST_SDK}/bin )
	get_filename_component(PSIMEDIA_DEPS_DIR ${PSIMEDIA_DEPS_PATH} DIRECTORY)
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
	find_psi_lib("${PSIMEDIA_DEPS}" "${PSIMEDIA_DEPS_DIR}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/")
	# streamer plugins
	set(GSTREAMER_PLUGINS_DIR "${PSIMEDIA_DIR}/gstreamer-0.10")
	file(GLOB GSTREAMER_PLUGINS "${GSTREAMER_PLUGINS_DIR}/*.dll")
	find_psi_lib("${GSTREAMER_PLUGINS}" "${GSTREAMER_PLUGINS_DIR}/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/gstreamer-0.10/")
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
		libotr.dll
		libotr-5.dll
		libtidy.dll
		libzlib.dll
		zlib1.dll
		libgpg-error-0.dll
		libidn-11.dll
		libhunspell.dll
		libhunspell-1.3-0.dll
		libhunspell-1.4-0.dll
		libhunspell-1.5-0.dll
		libhunspell-1.6-0.dll
		libeay32.dll
		ssleay32.dll
	)
	set(QCA_PATHES
		"${QT_BIN_DIR}"
		"${QCA_DIR}bin"
		"${QT_PLUGINS_DIR}/crypto"
		"${QCA_DIR}lib/qca${QCA_LIB_SUFF}/crypto"
	)
	find_psi_lib("libqca${QCA_LIB_SUFF}${D}.dll" "${QCA_PATHES}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/")
	if(MSVC)
		list(APPEND LIBRARIES_LIST
			otr${D}.dll
			tidy${D}.dll
			zlib${D}.dll
			idn${D}.dll
		)
		find_psi_lib("qca${QCA_LIB_SUFF}${D}.dll" "${QCA_PATHES}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/")
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
			libwebpdecoder-1.dll
			libwebpdemux-1.dll
			libxml2-2.dll
                )
	endif()
	if(SEPARATE_QJDNS)
		list(APPEND LIBRARIES_LIST
			libqjdns.dll
			libjdns.dll
		)
	endif()
	if(EXISTS "${SDK_PATH}")
		set(PATHES
			"${IDN_ROOT}bin"
			"${HUNSPELL_ROOT}bin"
			"${LIBGCRYPT_ROOT}bin"
			"${LIBGPGERROR_ROOT}bin"
			"${LIBOTR_ROOT}bin"
			"${LIBTIDY_ROOT}bin"
			"${QJSON_ROOT}bin"
			"${ZLIB_ROOT}bin"
		)
		if(MSVC)
			list(APPEND PATHES "${SDK_PATH}/bin")
		else()
			list(APPEND PATHES "${SDK_PATH}/openssl/bin")
		endif()
		if(SEPARATE_QJDNS)
			list(APPEND PATHES
				"${QJDNS_DIR}bin"
			)
		endif()
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
	find_psi_lib("${QCA_PLUGINS}" "${QCA_PATHES}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/crypto/")
endif()
