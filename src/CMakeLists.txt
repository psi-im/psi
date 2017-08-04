set( CMAKE_MODULE_PATH
	"${CMAKE_MODULE_PATH}"
	"${CMAKE_CURRENT_SOURCE_DIR}"
	"${PROJECT_SOURCE_DIR}/cmake/modules" )
set(CMAKE_AUTOMOC OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND WIN32)
	set(D "d")
	add_definitions(-DALLOW_QT_PLUGINS_DIR)
endif()

set(LIB_SUFFIX "" CACHE STRING "Define suffix of directory name (32/64)")
set(TRANSLATIONS_DIR "${PROJECT_SOURCE_DIR}/translations")

if(USE_QT5)
	find_package(Qt5 COMPONENTS Widgets Core Gui Xml Network Concurrent Multimedia Svg REQUIRED)
	if(ENABLE_WEBKIT)
		if(NOT USE_WEBENGINE)
			find_package(Qt5 COMPONENTS WebKit WebKitWidgets REQUIRED)
			set(WEBKIT_LIBS Qt5::WebKit Qt5::WebKitWidgets)
		else()
			find_package(Qt5 COMPONENTS WebEngine WebEngineWidgets WebChannel REQUIRED)
			set(WEBKIT_LIBS Qt5::WebEngine Qt5::WebEngineWidgets Qt5::WebChannel)
		endif()
	endif()
	if(ENABLE_SQL)
		find_package(Qt5Sql REQUIRED)
		set(SQL_LIBS Qt5::Sql)
	endif()

	if(LINUX)
		find_package(Qt5DBus REQUIRED)
		find_package(Qt5X11Extras REQUIRED)
		find_package(XCB REQUIRED)
		set(EXTRA_LIBS
			${XCB_LIBRARY}
			)
		include_directories(${LIBXCB_INCLUDE_DIR})
		add_definitions(${LIBXCB_DEFINITIONS})
	endif()
	
	if(APPLE)
		find_package(Qt5MacExtras REQUIRED)
		find_package(Sparkle REQUIRED)
		include_directories(${SPARKLE_INCLUDE_DIR})
	endif()

	add_definitions(-DHAVE_QT5 ${LIBXCB_DEFINITIONS})
	
	if(EXISTS "${TRANSLATIONS_DIR}")
		set(LANGS_EXISTS ON)

		find_package(Qt5 COMPONENTS LinguistTools REQUIRED)

		macro(QT4_CREATE_TRANSLATION)
			QT5_CREATE_TRANSLATION(${ARGN})
		endmacro()

		macro(QT4_ADD_TRANSLATION)
			qt5_add_translation(${ARGN})
		endmacro()
	endif()

	macro(qt4_add_resources)
		qt5_add_resources(${ARGN})
	endmacro()

	macro(qt4_wrap_ui)
		qt5_wrap_ui(${ARGN})
	endmacro()

	macro(qt4_generate_moc)
		qt5_generate_moc(${ARGN})
	endmacro()

	set(QT_LUPDATE_EXECUTABLE ${Qt5_LUPDATE_EXECUTABLE})
	set(QT_LIBRARIES Qt5::Widgets Qt5::Core Qt5::Network Qt5::Xml ${WEBKIT_LIBS} Qt5::Concurrent Qt5::Multimedia Qt5::Svg ${SQL_LIBS})
	set(Qca_SUFFIX -qt5)
	if(LINUX)
		list(APPEND QT_LIBRARIES Qt5::DBus Qt5::X11Extras ${LIBXCB_LIBRARIES})
	endif()
	if(USE_WEBENGINE)
		set(EXTRA_LIBS
			${EXTRA_LIBS}
			qhttp
		)
	endif()
	if(APPLE)
		list(APPEND QT_LIBRARIES Qt5::MacExtras)
		list(APPEND EXTRA_LDFLAGS ${SPARKLE_LIBRARY} "-framework Carbon" "-framework AppKit" "-framework IOKit" "-framework CoreAudio" "-framework AudioToolbox")
	endif()
else()
	if(LINUX)
		set(QT_EXTRA_MODULES QtDBus)
	endif()
	find_package(Qt4 COMPONENTS QtCore QtGui QtNetwork QtXml QtSvg ${QT_EXTRA_MODULES} REQUIRED)
	if(ENABLE_WEBKIT)
		find_package(Qt4 COMPONENTS QtWebKit REQUIRED)
	endif()
	if(ENABLE_SQL)
		find_package(Qt4 COMPONENTS QtSql REQUIRED)
	endif()

	unset(QT_EXTRA_MODULES)
	include(${QT_USE_FILE})
	if(ENABLE_SQL)
		find_package(QJSON REQUIRED)
		set(EXTRA_LIBS
			${EXTRA_LIBS}
			${QJSON_LIBRARY}
			)
		include_directories(${QJSON_INCLUDE_DIR})
		if( WIN32 )
			include_directories( ${QJSON_INCLUDE_DIR}/../ )
		endif()
	endif()
endif()

find_package( Qca REQUIRED )

if(LINUX)
	find_package(X11 REQUIRED)
	include_directories(${X11_INCLUDE_DIR})
endif()

if(USE_ENCHANT)
	find_package(Enchant REQUIRED)
	set(EXTRA_LIBS
		${EXTRA_LIBS}
		${Enchant_LIBRARY}
		)
	include_directories(${Enchant_INCLUDE_DIR})
endif()
if(USE_HUNSPELL)
	find_package(Hunspell REQUIRED)
	set(EXTRA_LIBS
		${EXTRA_LIBS}
		${HUNSPELL_LIBRARY}
		)
	include_directories(${HUNSPELL_INCLUDE_DIR})
	if( WIN32 )
		include_directories( ${HUNSPELL_INCLUDE_DIR}/../ )
	endif()
endif()

macro(qt_wrap_cpp)
	set(_SOURCES ${ARGN})
	list(REMOVE_AT _SOURCES 0)
	foreach(SOURCE ${_SOURCES})
		if(SOURCE MATCHES "^(.*/|)([^/.]+)\\.h$")
			set(DEST "moc_${CMAKE_MATCH_2}.cpp")
			qt4_generate_moc(${SOURCE} ${DEST} ${PROJECT_NAME})
			list(APPEND ${ARGV0} ${DEST})
		elseif(SOURCE MATCHES "^(.*/|)([^/.]+)\\.cpp$")
			set(DEST "${CMAKE_MATCH_2}.moc")
			qt4_generate_moc(${SOURCE} ${DEST} TARGET ${PROJECT_NAME})
			list(APPEND ${ARGV0} ${DEST})
		else()
			message(WARNING "Wrong source ${SOURCE}")
		endif()
	endforeach()
	unset(_SOURCES)
endmacro()

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
	${PROJECT_SOURCE_DIR}
	${Qca_INCLUDE_DIR}
)

if( BUNDLED_IRIS )
	include_directories(
		${PROJECT_SOURCE_DIR}/iris/src
		${PROJECT_SOURCE_DIR}/iris/include
		${PROJECT_SOURCE_DIR}/iris/include/iris
	)
else()
	include_directories(${Iris_INCLUDE_DIR})
endif()

string(TIMESTAMP PSI_COMPILATION_DATE "%Y-%m-%d")
string(TIMESTAMP PSI_COMPILATION_TIME "%H:%M:%S")

if(ENABLE_WEBKIT)
	if(NOT USE_WEBENGINE)
		set(PSI_VER_SUFFIX ", webkit")
	else()
		set(PSI_VER_SUFFIX ", webengine")
	endif()
endif()
if(ENABLE_SQL)
	set( PSI_VER_SUFFIX "${PSI_VER_SUFFIX}, sql" )
endif()

if( NOT IS_PSIPLUS )
	set(CLIENT_NAME "Psi")
else()
	set(CLIENT_NAME "Psi+")
endif()

if(NOT PSI_VERSION)
	if( PSIPLUS_VERSION_OBTAINED )
		set(PSI_VERSION "${APP_VERSION} \(${PSI_COMPILATION_DATE}, Psi:${PSI_REVISION}, Psi+:${PSI_PLUS_REVISION}${PSI_VER_SUFFIX}\)")
	elseif( PSI_VERSION_OBTAINED OR APP_VERSION )
		set(PSI_VERSION "${APP_VERSION}-dev \(${PSI_COMPILATION_DATE}${PSI_VER_SUFFIX}\)")
	endif()
endif()

message(STATUS "${CLIENT_NAME} version set: ${PSI_VERSION}")

if(LINUX)
	if(LIB_INSTALL_DIR)
		set(PSI_LIBDIR "${LIB_INSTALL_DIR}/${PROJECT_NAME}")
	else()
		set(PSI_LIBDIR "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}/${PROJECT_NAME}")
	endif()

	if(SHARE_INSTALL_PREFIX)
		set(PSI_DATADIR "${SHARE_INSTALL_PREFIX}/${PROJECT_NAME}")
	else()
		set(PSI_DATADIR "${CMAKE_INSTALL_PREFIX}/share/${PROJECT_NAME}")
	endif()
endif()

configure_file(config.h.in config.h @ONLY)

add_definitions(
	-DQT_STATICPLUGIN
	-DHAVE_CONFIG
)

if(WIN32)
	add_definitions(
		-DUNICODE
		-D_UNICODE
	)
endif()

include(src.cmake)

# Only headers or very small sources
include(irisprotocol/irisprotocol.cmake)
include(protocol/protocol.cmake)
include(plugins/plugins.cmake)

add_subdirectory(AutoUpdater)
add_subdirectory(options)
add_subdirectory(tabs)
add_subdirectory(privacy)
add_subdirectory(Certificates)
add_subdirectory(avcall)
add_subdirectory(psimedia)
add_subdirectory(contactmanager)
add_subdirectory(tools)
add_subdirectory(libpsi/dialogs)
add_subdirectory(libpsi/tools)
add_subdirectory(widgets)
add_subdirectory(sxe)
add_subdirectory(whiteboarding)

if(USE_WEBENGINE)
	include_directories(
		${PROJECT_SOURCE_DIR}/3rdparty
		${PROJECT_SOURCE_DIR}/3rdparty/http-parser
		${PROJECT_SOURCE_DIR}/3rdparty/qhttp/src
		${PROJECT_SOURCE_DIR}/3rdparty/qhttp/src/private
	)
endif()

if(EXISTS "${TRANSLATIONS_DIR}")
	set(TRANSLATIONS
		${TRANSLATIONS_DIR}/psi_be.ts
		${TRANSLATIONS_DIR}/psi_bg.ts
		${TRANSLATIONS_DIR}/psi_ca.ts
		${TRANSLATIONS_DIR}/psi_cs.ts
		${TRANSLATIONS_DIR}/psi_de.ts
		${TRANSLATIONS_DIR}/psi_en.ts
		${TRANSLATIONS_DIR}/psi_eo.ts
		${TRANSLATIONS_DIR}/psi_es.ts
		${TRANSLATIONS_DIR}/psi_et.ts
		${TRANSLATIONS_DIR}/psi_fa.ts
		${TRANSLATIONS_DIR}/psi_fi.ts
		${TRANSLATIONS_DIR}/psi_fr.ts
		${TRANSLATIONS_DIR}/psi_he.ts
		${TRANSLATIONS_DIR}/psi_hu.ts
		${TRANSLATIONS_DIR}/psi_it.ts
		${TRANSLATIONS_DIR}/psi_ja.ts
		${TRANSLATIONS_DIR}/psi_kk.ts
		${TRANSLATIONS_DIR}/psi_mk.ts
		${TRANSLATIONS_DIR}/psi_nl.ts
		${TRANSLATIONS_DIR}/psi_pl.ts
		${TRANSLATIONS_DIR}/psi_pt_BR.ts
		${TRANSLATIONS_DIR}/psi_pt.ts
		${TRANSLATIONS_DIR}/psi_ru.ts
		${TRANSLATIONS_DIR}/psi_sk.ts
		${TRANSLATIONS_DIR}/psi_sl.ts
		${TRANSLATIONS_DIR}/psi_sr@latin.ts
		${TRANSLATIONS_DIR}/psi_sv.ts
		${TRANSLATIONS_DIR}/psi_sw.ts
		${TRANSLATIONS_DIR}/psi_uk.ts
		${TRANSLATIONS_DIR}/psi_ur_PK.ts
		${TRANSLATIONS_DIR}/psi_vi.ts
		${TRANSLATIONS_DIR}/psi_zh_CN.ts
		${TRANSLATIONS_DIR}/psi_zh_TW.ts
	)

	set_property(SOURCE ${TRANSLATIONS}
		PROPERTY OUTPUT_LOCATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/translations)

	add_custom_target(lupdate)
	foreach(_LANG ${TRANSLATIONS})
		if(EXISTS "${_LANG}")
			get_filename_component(_SHORT_LANG ${_LANG} NAME_WE)
			string(REPLACE "@" "_" _SHORT_LANG ${_SHORT_LANG})
			add_custom_target(${_SHORT_LANG}
				COMMAND ${QT_LUPDATE_EXECUTABLE}
				ARGS -no-obsolete -recursive . ../iris -ts ${_LANG}
				WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
				VERBATIM
			)
			add_dependencies(lupdate ${_SHORT_LANG})
		endif()
	endforeach()

	QT4_ADD_TRANSLATION(QM ${TRANSLATIONS})
endif()

set(RESOURCES
	${PROJECT_SOURCE_DIR}/psi.qrc
	${PROJECT_SOURCE_DIR}/iconsets.qrc
)
qt4_add_resources(QRC_SOURCES ${RESOURCES})

qt4_wrap_ui(UI_FORMS ${FORMS})
add_custom_target(build_ui_files DEPENDS "${UI_FORMS}" SOURCES "${FORMS}")

qt_wrap_cpp(MOC_SOURCES ${SOURCES} ${HEADERS})

if(WIN32)
	# win resource compilation
	if(BUILD_ARCH STREQUAL "i386" OR (BUILD_ARCH STREQUAL "win32"))
		set(RC_SUFFIX "32")
	else()
		set(RC_SUFFIX "64")
	endif()
	set(WINRC_FILE_NAME ${PROJECT_SOURCE_DIR}/win32/psi_win${RC_SUFFIX}.rc)
	set(RC_FILE ${CMAKE_CURRENT_BINARY_DIR}/psi_win32.o)
	if(NOT MSVC)
		set(CMD_ARG
			--include=${CMAKE_CURRENT_SOURCE_DIR}
			--input=${WINRC_FILE_NAME}
			--output=${RC_FILE}
		)
	else()
		set(CMD_ARG
			/fo
			${RC_FILE}
			${WINRC_FILE_NAME}
		)
	endif()
	add_custom_command(OUTPUT ${RC_FILE}
			COMMAND ${CMAKE_RC_COMPILER}
			ARGS ${CMD_ARG}
			WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/win32
			VERBATIM
	)
endif()

add_executable(${PROJECT_NAME}
	WIN32
	MACOSX_BUNDLE
	${SOURCES}
	${HEADERS}
	${MOC_SOURCES}
	${UI_FORMS}
	${PLAIN_SOURCES}
	${PLAIN_HEADERS}
	${QRC_SOURCES}
	${QM}
	${RC_FILE}
)

#Experimental feature
if(VERBOSE_PROGRAM_NAME)
	if(ENABLE_WEBKIT AND (NOT USE_WEBENGINE))
		set(VERBOSE_SUFFIX "-webkit")
	elseif(USE_WEBENGINE)
		set(VERBOSE_SUFFIX "-webengine")
	endif()
	if(ENABLE_SQL)
		set(VERBOSE_SUFFIX "${VERBOSE_SUFFIX}-sql")
	endif()
endif()
#

if(ENABLE_PORTABLE OR VERBOSE_PROGRAM_NAME)
	if(ENABLE_PORTABLE)
		set(PORTABLE_SUFFIX "-portable")
	endif()
	set_target_properties(${PROJECT_NAME}
				PROPERTIES
				OUTPUT_NAME
				"${PROJECT_NAME}${VERBOSE_SUFFIX}${PORTABLE_SUFFIX}"
	)
endif()

if(USE_WEBENGINE)
	set(STANDARD_NUM 14)
else()
	set(STANDARD_NUM 11)
endif()

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD ${STANDARD_NUM})
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)

add_dependencies(${PROJECT_NAME} iris)
if(USE_WEBENGINE)
	add_dependencies(${PROJECT_NAME} qhttp)
endif()
add_dependencies(${PROJECT_NAME} build_ui_files)

if(WIN32 AND MSVC)
	list(APPEND EXTRA_LIBS
		wsock32
		user32
		shell32
		gdi32
		advapi32
	)
	add_definitions(
		-DQT_STATICPLUGIN
	)
endif()

target_link_libraries(${PROJECT_NAME}
	${QT_LIBRARIES}
	${EXTRA_LDFLAGS}
	${X11_LIBRARIES}
	${EXTRA_LIBS}
	iris
	${Qca_LIBRARY}
)
target_link_libraries(${PROJECT_NAME}
	options
	Certificates
	psimedia
	contactmanager
	avcall
	whiteboarding
	sxe
	tools
	libpsi_dialogs
	libpsi_tools
	widgets
	privacy
	tabs
	AutoUpdater
)

set(OTHER_FILES
	${PROJECT_SOURCE_DIR}/certs
	${PROJECT_SOURCE_DIR}/iconsets
	${PROJECT_SOURCE_DIR}/sound
)
#PREPARE SECTION
copy("${PROJECT_SOURCE_DIR}/certs/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/certs/" prepare-bin)
copy("${PROJECT_SOURCE_DIR}/iconsets/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/iconsets/" prepare-bin)
if(ENABLE_WEBKIT)
	copy("${PROJECT_SOURCE_DIR}/themes/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/themes/" prepare-bin)
endif()
copy("${PROJECT_SOURCE_DIR}/sound/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sound/" prepare-bin)
if(ENABLE_PLUGINS)
	copy("${PROJECT_SOURCE_DIR}/skins/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/skins/" prepare-bin)
endif()
copy("${PROJECT_SOURCE_DIR}/client_icons.txt" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin)

#INSTALL SECTION
if(LINUX)
	if(IS_PSIPLUS)
		set(SHARE_SUFF "share/psi-plus")
		install(FILES ${PROJECT_SOURCE_DIR}/psi.desktop DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications RENAME psi-plus.desktop)
		install(FILES ${PROJECT_SOURCE_DIR}/iconsets/system/default/psiplus/logo_128.png DESTINATION ${CMAKE_INSTALL_PREFIX}/share/pixmaps RENAME psi-plus.png)
	else()
		set(SHARE_SUFF "share/psi")
		install(FILES ${PROJECT_SOURCE_DIR}/psi.desktop DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications)
		install(FILES ${PROJECT_SOURCE_DIR}/iconsets/system/default/logo_128.png DESTINATION ${CMAKE_INSTALL_PREFIX}/share/pixmaps RENAME psi.png)
	endif()
	install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
	install(FILES ${PROJECT_SOURCE_DIR}/client_icons.txt DESTINATION ${CMAKE_INSTALL_PREFIX}/${SHARE_SUFF})
	install(DIRECTORY ${OTHER_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX}/${SHARE_SUFF})
	if(ENABLE_WEBKIT)
		install(DIRECTORY ${PROJECT_SOURCE_DIR}/themes DESTINATION ${CMAKE_INSTALL_PREFIX}/${SHARE_SUFF})
	endif()
	if(LANGS_EXISTS)
		install(FILES ${QM} DESTINATION ${CMAKE_INSTALL_PREFIX}/${SHARE_SUFF}/translations)
	endif()
endif()

if(WIN32)
	install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX})
	install(FILES ${PROJECT_SOURCE_DIR}/client_icons.txt DESTINATION ${CMAKE_INSTALL_PREFIX})
	install(DIRECTORY ${OTHER_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX})
	if(ENABLE_WEBKIT)
		install(DIRECTORY ${PROJECT_SOURCE_DIR}/themes DESTINATION ${CMAKE_INSTALL_PREFIX})
	endif()
	if(LANGS_EXISTS)
		install(FILES ${QM} DESTINATION ${CMAKE_INSTALL_PREFIX}/translations)
	endif()
	copy("${PROJECT_SOURCE_DIR}/myspell/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/myspell/" prepare-bin)
	if(PRODUCTION)
		include(win32-prepare-deps.cmake)
	endif()
endif()
#INSTALL SECTION END

if(ENABLE_PLUGINS)
	add_subdirectory(plugins)
endif()


