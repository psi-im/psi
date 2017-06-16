unset(MOC_SOURCES)
unset(HEADERS)
unset(PLAIN_HEADERS)
unset(FORMS)
unset(SOURCES)
unset(PLAIN_SOURCES)
unset(UI_FORMS)

list(APPEND HEADERS
	privacydlg.h
	privacymanager.h
	privacyruledlg.h
	psiprivacymanager.h
)

list(APPEND PLAIN_HEADERS
	privacylist.h
	privacylistblockedmodel.h
	privacylistitem.h
	privacylistmodel.h
)

list(APPEND SOURCES
	psiprivacymanager.cpp
)

list(APPEND PLAIN_SOURCES
	privacydlg.cpp
	privacylist.cpp
	privacylistblockedmodel.cpp
	privacylistitem.cpp
	privacylistmodel.cpp
	privacyruledlg.cpp
)

list(APPEND FORMS
	privacy.ui
	privacyrule.ui
)

qt4_wrap_ui(UI_FORMS ${FORMS})
qt_wrap_cpp(MOC_SOURCES ${SOURCES} ${HEADERS})
add_library(privacy STATIC ${SOURCES} ${HEADERS} ${SOURCES} ${MOC_SOURCES} ${UI_FORMS} ${PLAIN_SOURCES})
target_link_libraries(privacy ${QT_LIBRARIES} ${iris_LIB} libpsi_tools widgets tabs)
target_include_directories(privacy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
