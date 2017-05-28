set( CMAKE_MODULE_PATH
	"${CMAKE_MODULE_PATH}"
	"${CMAKE_CURRENT_SOURCE_DIR}"
	"${PROJECT_SOURCE_DIR}/cmake/modules" )
set(CMAKE_AUTOMOC OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND WIN32)
	set(D "d")
	add_definitions(-DALLOW_QT_PLUGINS_DIR)
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 ${EXTRA_FLAG}")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 ${EXTRA_FLAG}")
endif()

set( LIB_SUFFIX "" CACHE STRING "Define suffix of directory name (32/64)" )

if(USE_QT5)
	find_package(Qt5 COMPONENTS Widgets Core Gui Xml Network Concurrent Multimedia Svg LinguistTools REQUIRED)
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

	add_definitions(-DHAVE_QT5 ${LIBXCB_DEFINITIONS})

	macro(QT4_CREATE_TRANSLATION)
		QT5_CREATE_TRANSLATION(${ARGN})
	endmacro()

	macro(QT4_ADD_TRANSLATION)
		qt5_add_translation(${ARGN})
	endmacro()

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
	find_package( Qca-qt5 REQUIRED )
	set(QCA_INCLUDES ${Qca-qt5_INCLUDE_DIR})
	set(qca_LIB ${Qca-qt5_LIBRARY})
	if(LINUX)
		list(APPEND QT_LIBRARIES Qt5::DBus Qt5::X11Extras ${LIBXCB_LIBRARIES})
	endif()
	if(USE_WEBENGINE)
		set(EXTRA_LIBS
			${EXTRA_LIBS}
			qhttp
		)
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
	find_package( Qca REQUIRED )
	set(QCA_INCLUDES ${Qca_INCLUDE_DIR})
	set(qca_LIB ${Qca_LIBRARY})
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
	${QCA_INCLUDES}
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

include(getdate.cmake)
if(ENABLE_WEBKIT)
	#if(NOT USE_WEBENGINE)
		set(PSI_VER_SUFFIX -webkit)
	#else()
	#	set(PSI_VER_SUFFIX -webengine)
	#endif()
endif()
if(NOT PSI_PLUS_VERSION)
	set(PSI_PLUS_VERSION "${APP_VERSION}-dev" CACHE STRING "Version string of Psi")
endif()
add_definitions(-DPSI_VERSION="${PSI_PLUS_VERSION}${PSI_VER_SUFFIX} \(${PSI_COMPILATION_DATE}\)")

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


if(EXISTS "${PROJECT_SOURCE_DIR}/translations")
	set(LANGS_EXISTS ON)
	set(TRANSLATIONS
		../translations/psi_be.ts
		../translations/psi_bg.ts
		../translations/psi_ca.ts
		../translations/psi_cs.ts
		../translations/psi_de.ts
		../translations/psi_en.ts
		../translations/psi_eo.ts
		../translations/psi_es.ts
		../translations/psi_et.ts
		../translations/psi_fa.ts
		../translations/psi_fi.ts
		../translations/psi_fr.ts
		../translations/psi_he.ts
		../translations/psi_hu.ts
		../translations/psi_it.ts
		../translations/psi_ja.ts
		../translations/psi_kk.ts
		../translations/psi_mk.ts
		../translations/psi_nl.ts
		../translations/psi_pl.ts
		../translations/psi_pt_BR.ts
		../translations/psi_pt.ts
		../translations/psi_ru.ts
		../translations/psi_sk.ts
		../translations/psi_sl.ts
		../translations/psi_sr@latin.ts
		../translations/psi_sv.ts
		../translations/psi_sw.ts
		../translations/psi_uk.ts
		../translations/psi_ur_PK.ts
		../translations/psi_vi.ts
		../translations/psi_zh_CN.ts
		../translations/psi_zh_TW.ts
		)

	set_property(SOURCE ${TRANSLATIONS} PROPERTY OUTPUT_LOCATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/translations)

	add_custom_target(lupdate)
	foreach(_LANG ${TRANSLATIONS})
		get_filename_component(_SHORT_LANG ${_LANG} NAME_WE)
		string(REPLACE "@" "_" _SHORT_LANG ${_SHORT_LANG})
		add_custom_target(${_SHORT_LANG}
			COMMAND ${QT_LUPDATE_EXECUTABLE} "-no-obsolete" "-recursive" "." "../iris" "-ts" ${_LANG}
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" VERBATIM)
		add_dependencies(lupdate ${_SHORT_LANG})
	endforeach()

	QT4_ADD_TRANSLATION(QM ${TRANSLATIONS})
endif()

set(RESOURCES ../psi.qrc ../iconsets.qrc)
qt4_add_resources(QRC_SOURCES ${RESOURCES})

qt4_wrap_ui(UI_FORMS ${FORMS})
add_custom_target(build_ui_files DEPENDS "${UI_FORMS}" SOURCES "${FORMS}")

qt_wrap_cpp(MOC_SOURCES ${SOURCES} ${HEADERS})

if(WIN32)
	# resource compilation for MinGW
	if(NOT MSVC)
		if(BUILD_ARCH STREQUAL "i386")
			add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/psi_win32.o
				COMMAND ${CMAKE_RC_COMPILER} -I${CMAKE_CURRENT_SOURCE_DIR} -i${CMAKE_SOURCE_DIR}/win32/psi_win32.rc
				-o ${CMAKE_CURRENT_BINARY_DIR}/psi_win32.o)
		else()
			add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/psi_win32.o
				COMMAND ${CMAKE_RC_COMPILER} -I${CMAKE_CURRENT_SOURCE_DIR} -i${CMAKE_SOURCE_DIR}/win32/psi_win64.rc
				-o ${CMAKE_CURRENT_BINARY_DIR}/psi_win32.o)
		endif()
	else()
		if(BUILD_ARCH STREQUAL "win32")
			add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/psi_win32.o
				COMMAND ${CMAKE_RC_COMPILER} /fo ${CMAKE_CURRENT_BINARY_DIR}/psi_win32.o ${CMAKE_SOURCE_DIR}/win32/psi_win32.rc)
		else()
			add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/psi_win32.o
				COMMAND ${CMAKE_RC_COMPILER} /fo ${CMAKE_CURRENT_BINARY_DIR}/psi_win32.o ${CMAKE_SOURCE_DIR}/win32/psi_win64.rc)
		endif()
	endif()
	set(RC_FILE ${CMAKE_CURRENT_BINARY_DIR}/psi_win32.o)
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

target_link_libraries(${PROJECT_NAME}
	${QT_LIBRARIES}
	${EXTRA_LDFLAGS}
	${X11_LIBRARIES}
	${EXTRA_LIBS}
	iris
	${qca_LIB}
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
	../certs
	../iconsets
	../sound
	)
#INSTALL SECTION
copy("${PROJECT_SOURCE_DIR}/iconsets/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/iconsets/" prepare-bin)
copy("${PROJECT_SOURCE_DIR}/themes/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/themes/" prepare-bin)
copy("${PROJECT_SOURCE_DIR}/sound/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sound/" prepare-bin)
copy("${PROJECT_SOURCE_DIR}/skins/" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/skins/" prepare-bin)
copy("${PROJECT_SOURCE_DIR}/client_icons.txt" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" prepare-bin)

if(LINUX)
	if(IS_PSIPLUS)
		set(SHARE_SUFF "share/psi-plus")
		install(FILES ../psi.desktop DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications RENAME psi-plus.desktop)
		install(FILES ../iconsets/system/default/psiplus/logo_128.png DESTINATION ${CMAKE_INSTALL_PREFIX}/share/pixmaps RENAME psi-plus.png)
	else()
		set(SHARE_SUFF "share/psi")
		install(FILES ../psi.desktop DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications)
		install(FILES ../iconsets/system/default/logo_128.png DESTINATION ${CMAKE_INSTALL_PREFIX}/share/pixmaps RENAME psi.png)
	endif()
	install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
	install(FILES ../client_icons.txt DESTINATION ${CMAKE_INSTALL_PREFIX}/${SHARE_SUFF})
	install(DIRECTORY ${OTHER_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX}/${SHARE_SUFF})
	if(ENABLE_WEBKIT)
		install(DIRECTORY ../themes DESTINATION ${CMAKE_INSTALL_PREFIX}/${SHARE_SUFF})
	endif()
	if(LANGS_EXISTS)
		install(FILES ${QM} DESTINATION ${CMAKE_INSTALL_PREFIX}/${SHARE_SUFF}/translations)
	endif()
endif()

if(WIN32)
	install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX})
	install(FILES ../client_icons.txt DESTINATION ${CMAKE_INSTALL_PREFIX})
	install(DIRECTORY ${OTHER_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX})
	if(ENABLE_WEBKIT)
		install(DIRECTORY ../themes DESTINATION ${CMAKE_INSTALL_PREFIX})
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


