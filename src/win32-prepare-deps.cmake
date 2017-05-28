cmake_minimum_required(VERSION 2.8.12)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND WIN32)
	set(D "d")
	add_definitions(-DALLOW_QT_PLUGINS_DIR)
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 ${EXTRA_FLAG}")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 ${EXTRA_FLAG}")
endif()
if(WIN32)
	# Get Qt installation path
	string(REGEX REPLACE "([^ ]+)[/\\].*" "\\1" QT_BIN_DIR_TMP "${QT_MOC_EXECUTABLE}")
	string(REGEX REPLACE "\\\\" "/" QT_BIN_DIR "${QT_BIN_DIR_TMP}")
	unset(QT_BIN_DIR_TMP)
	function(find_psi_lib LIBLIST PATHES)
		set( inc 1 )
		set(FIXED_PATHES "")
		foreach(_path ${PATHES})
			string(REGEX REPLACE "//bin" "/bin" tmp_path "${_path}")
			list(APPEND FIXED_PATHES ${tmp_path})
		endforeach()
		foreach(_liba ${LIBLIST})
			find_file( ${_liba}${inc} ${_liba} PATHS ${FIXED_PATHES})
			if( NOT "${${_liba}${inc}}" STREQUAL "${_liba}${inc}-NOTFOUND" )
				message("library found ${${_liba}${inc}}")
				copy("${${_liba}${inc}}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
			endif()
			math( EXPR inc ${inc}+1)
		endforeach()
	endfunction()
	get_filename_component(QT_DIR ${QT_BIN_DIR} DIRECTORY)
	set(QT_PLUGINS_DIR ${QT_DIR}/plugins)
	set(QT_TRANSLATIONS_DIR ${QT_DIR}/translations)

	if(USE_QT5)
		set(SDK_PREFIX "Qt5/")
		set(QCA_LIB_SUFF "-qt5")
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
					copy("${${icu_prefix}${icu_counter}}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
				endif()
			endforeach()
		endforeach()

		# Qt5 libraries
		copy("${QT_BIN_DIR}/Qt5Core${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
		copy("${QT_BIN_DIR}/Qt5Gui${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
		copy("${QT_BIN_DIR}/Qt5Widgets${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
		copy("${QT_BIN_DIR}/Qt5Svg${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
		copy("${QT_BIN_DIR}/Qt5Network${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
		copy("${QT_BIN_DIR}/Qt5Svg${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
		copy("${QT_BIN_DIR}/Qt5Script${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
		copy("${QT_BIN_DIR}/Qt5Xml${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
		copy("${QT_BIN_DIR}/Qt5XmlPatterns${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
		copy("${QT_BIN_DIR}/Qt5Sql${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
		copy("${QT_BIN_DIR}/Qt5WebKit${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
		copy("${QT_BIN_DIR}/Qt5WebKitWidgets${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
		copy("${QT_BIN_DIR}/Qt5Qml${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
		copy("${QT_BIN_DIR}/Qt5Quick${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
		copy("${QT_BIN_DIR}/Qt5Positioning${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
		copy("${QT_BIN_DIR}/Qt5WebChannel${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
		copy("${QT_BIN_DIR}/Qt5Multimedia${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
		copy("${QT_BIN_DIR}/Qt5MultimediaWidgets${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
		copy("${QT_BIN_DIR}/Qt5Concurrent${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
		copy("${QT_BIN_DIR}/Qt5Sensors${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
		copy("${QT_BIN_DIR}/Qt5OpenGL${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
		copy("${QT_BIN_DIR}/Qt5PrintSupport${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)

		#audio
		copy("${QT_PLUGINS_DIR}/audio/qtaudio_windows${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/audio/" prepare-bin-libs)
		# platforms
		copy("${QT_PLUGINS_DIR}/platforms/qminimal${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/platforms/" prepare-bin-libs)
		copy("${QT_PLUGINS_DIR}/platforms/qoffscreen${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/platforms/" prepare-bin-libs)
		copy("${QT_PLUGINS_DIR}/platforms/qwindows${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/platforms/" prepare-bin-libs)

		# bearer
		copy("${QT_PLUGINS_DIR}/bearer/qgenericbearer${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bearer/" prepare-bin-libs)
		copy("${QT_PLUGINS_DIR}/bearer/qnativewifibearer${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bearer/" prepare-bin-libs)

		# imageformats
		copy("${QT_PLUGINS_DIR}/imageformats/qdds${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imageformats/" prepare-bin-libs)
		copy("${QT_PLUGINS_DIR}/imageformats/qgif${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imageformats/" prepare-bin-libs)
		copy("${QT_PLUGINS_DIR}/imageformats/qicns${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imageformats/" prepare-bin-libs)
		copy("${QT_PLUGINS_DIR}/imageformats/qico${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imageformats/" prepare-bin-libs)
		copy("${QT_PLUGINS_DIR}/imageformats/qjp2${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imageformats/" prepare-bin-libs)
		copy("${QT_PLUGINS_DIR}/imageformats/qjpeg${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imageformats/" prepare-bin-libs)
		copy("${QT_PLUGINS_DIR}/imageformats/qmng${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imageformats/" prepare-bin-libs)
		copy("${QT_PLUGINS_DIR}/imageformats/qsvg${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imageformats/" prepare-bin-libs)
		copy("${QT_PLUGINS_DIR}/imageformats/qtga${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imageformats/" prepare-bin-libs)
		copy("${QT_PLUGINS_DIR}/imageformats/qtiff${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imageformats/" prepare-bin-libs)
		copy("${QT_PLUGINS_DIR}/imageformats/qwbmp${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imageformats/" prepare-bin-libs)
		copy("${QT_PLUGINS_DIR}/imageformats/qwebp${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imageformats/" prepare-bin-libs)

		# printsupport
		copy("${QT_PLUGINS_DIR}/printsupport/windowsprintersupport${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/printsupport/" prepare-bin-libs)

		# sqldrivers
		copy("${QT_PLUGINS_DIR}/sqldrivers/qsqlite${D}.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sqldrivers/" prepare-bin-libs)

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
	else()
		# Qt4 libs
		copy("${QT_BIN_DIR}/QtCore${D}4.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
		copy("${QT_BIN_DIR}/QtGui${D}4.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
		copy("${QT_BIN_DIR}/QtNetwork${D}4.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
		copy("${QT_BIN_DIR}/QtSvg${D}4.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
		copy("${QT_BIN_DIR}/QtXml${D}4.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
		copy("${QT_BIN_DIR}/QtSql${D}4.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
		copy("${QT_BIN_DIR}/QtWebKit${D}4.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)

		# bearer
		copy("${QT_PLUGINS_DIR}/bearer/qgenericbearer${D}4.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bearer/" prepare-bin-libs)
		copy("${QT_PLUGINS_DIR}/bearer/qnativewifibearer${D}4.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bearer/" prepare-bin-libs)

		# plugins
		copy("${QT_PLUGINS_DIR}/imageformats/qgif${D}4.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imageformats/" prepare-bin-libs)
		copy("${QT_PLUGINS_DIR}/imageformats/qico${D}4.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imageformats/" prepare-bin-libs)
		copy("${QT_PLUGINS_DIR}/imageformats/qjpeg${D}4.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imageformats/" prepare-bin-libs)
		copy("${QT_PLUGINS_DIR}/imageformats/qmng${D}4.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imageformats/" prepare-bin-libs)
		copy("${QT_PLUGINS_DIR}/imageformats/qtga4.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imageformats/" prepare-bin-libs)
		copy("${QT_PLUGINS_DIR}/imageformats/qtiff${D}4.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imageformats/" prepare-bin-libs)
		copy("${QT_PLUGINS_DIR}/imageformats/qsvg${D}4.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imageformats/" prepare-bin-libs)

		# sqldrivers
		copy("${QT_PLUGINS_DIR}/sqldrivers/qsqlite${D}4.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sqldrivers/" prepare-bin-libs)

		# Qt translations
		file(GLOB QT_TRANSLATIONS "${QT_TRANSLATIONS_DIR}/qt_*.qm")
		foreach(FILE ${QT_TRANSLATIONS})
			if(NOT FILE MATCHES "_help_")
				copy(${FILE} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/translations/" prepare-bin-libs)
			endif()
		endforeach()

	endif()

	# psimedia
	find_program(PSIMEDIA_PATH libgstprovider${D}.dll PATHS ${PSIMEDIA_DIR}/plugins )
	copy(${PSIMEDIA_DIR} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)

	# psimedia deps
	find_program(PSIMEDIA_DEPS_PATH libgstvideo-0.10-0.dll PATHS ${GST_SDK}/bin )
	get_filename_component(PSIMEDIA_DEPS_DIR ${PSIMEDIA_DEPS_PATH} DIRECTORY)
	copy(${PSIMEDIA_DEPS_DIR}/libjpeg-9.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libgettextlib-0-19-6.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libogg-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libtheoradec-1.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libgettextpo-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/liborc-0.4-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libtheoraenc-1.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libasprintf-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libgettextsrc-0-19-6.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/liborc-test-0.4-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libvorbis-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libcharset-1.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libgio-2.0-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libspeex-1.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libvorbisenc-2.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libglib-2.0-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libgthread-2.0-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libspeexdsp-1.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libvorbisfile-3.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libffi-6.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libgmodule-2.0-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libgobject-2.0-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libintl-8.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libtheora-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libgstapp-0.10-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libgstaudio-0.10-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libgstbase-0.10-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libgstcdda-0.10-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libgstcontroller-0.10-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libgstdataprotocol-0.10-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libgstfft-0.10-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libgstinterfaces-0.10-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libgstnet-0.10-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libgstnetbuffer-0.10-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libgstpbutils-0.10-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libgstreamer-0.10-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libgstriff-0.10-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libgstrtp-0.10-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libgstrtsp-0.10-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libgstsdp-0.10-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libgsttag-0.10-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	copy(${PSIMEDIA_DEPS_DIR}/libgstvideo-0.10-0.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)

	# streamer plugins
	set(GSTREAMER_PLUGINS_DIR "${PSIMEDIA_DIR}/gstreamer-0.10")
	file(GLOB GSTREAMER_PLUGINS "${GSTREAMER_PLUGINS_DIR}/*.dll")
	foreach(FILE ${GSTREAMER_PLUGINS})
		copy(${FILE} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/gstreamer-0.10/" prepare-bin-libs)
	endforeach()

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
		libeay32.dll
		ssleay32.dll
	)
	if(NOT USE_QT5)
		list(APPEND LIBRARIES_LIST libqjson.dll)
	endif()

	if(USE_MXE)
		list(APPEND LIBRARIES_LIST
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
			libpcre-1.dll
			libpng16-16.dll
			libssp-0.dll
			libsqlite3-0.dll
			libtiff-5.dll
			libwebp-5.dll
			libwebpdecoder-1.dll
			libwebpdemux-1.dll
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
			"${SDK_PATH}/openssl/bin"
		)
	endif()
	find_psi_lib("${LIBRARIES_LIST}" "${PATHES}")

	if(SEPARATE_QJDNS)
		copy(${QJDNS_DIR}/libqjdns${D}.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
		copy(${QJDNS_DIR}/libjdns${D}.dll "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	endif()

	# qca and plugins
	find_file( QCA_LIB_FILE libqca${QCA_LIB_SUFF}${D}.dll PATHES ${QCA_DIR}/bin ${QT_BIN_DIR}/)
	if( NOT "${QCA_LIB_FILE}" STREQUAL "QCA_LIB_FILE-NOTFOUND" )
		copy(${QCA_LIB_FILE} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin-libs)
	endif()
	find_file( QCA_OSSL_PLUGIN libqca-ossl${D}.dll PATHES ${QCA_DIR}/lib/qca${QCA_LIB_SUFF}/crypto ${QT_PLUGINS_DIR}/crypto )
	if( NOT "${QCA_OSSL_PLUGIN}" STREQUAL "QCA_OSSL_PLUGIN-NOTFOUND" )
		copy(${QCA_OSSL_PLUGIN} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/crypto/" prepare-bin-libs)
	endif()
	find_file( QCA_GPG_PLUGIN libqca-gnupg${D}.dll PATHES ${QCA_DIR}/lib/qca${QCA_LIB_SUFF}/crypto ${QT_PLUGINS_DIR}/crypto )
	if( NOT "${QCA_GPG_PLUGIN}" STREQUAL "QCA_GPG_PLUGIN-NOTFOUND" )
		copy(${QCA_GPG_PLUGIN} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/crypto/" prepare-bin-libs)
	endif()
endif()
