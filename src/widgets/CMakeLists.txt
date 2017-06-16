unset(MOC_SOURCES)
unset(HEADERS)
unset(PLAIN_HEADERS)
unset(FORMS)
unset(SOURCES)
unset(PLAIN_SOURCES)
unset(UI_FORMS)
unset(EXTRA_LDFLAGS)

set(SOURCES
	busywidget.cpp
	fancylabel.cpp
	fancypopup.cpp
	iconaction.cpp
	iconselect.cpp
	iconwidget.cpp
	psirichtext.cpp
	psitextview.cpp
	psitooltip.cpp
	urlobject.cpp
)

set(PLAIN_SOURCES
	actionlineedit.cpp
	psitabbar.cpp
	psitabwidget.cpp
	psitiplabel.cpp
	typeaheadfind.cpp
	urllabel.cpp
)

set(HEADERS
	actionlineedit.h
	busywidget.h
	fancylabel.h
	fancypopup.h
	fancypopuplist.h
	iconaction.h
	iconbutton.h
	iconlabel.h
	iconselect.h
	iconsetdisplay.h
	iconsetselect.h
	icontoolbutton.h
	iconwidget.h
	psitabbar.h
	psitabwidget.h
	psitextview.h
	psitiplabel.h
	typeaheadfind.h
	updatingcombobox.h
	urllabel.h
	urlobject.h
)

set(PLAIN_HEADERS
	psirichtext.h
	psitooltip.h
	stretchwidget.h
)

set(FORMS
	fancypopup.ui
)

if(IS_PSIPLUS)
	list(APPEND PLAIN_SOURCES
		psiwindowheader.cpp
	)
	list(APPEND HEADERS
		psiwindowheader.h
	)
	list(APPEND FORMS
		psiwindowheader.ui
	)
	#check for 0280 patch
	if(EXISTS "${PROJECT_SOURCE_DIR}/src/widgets/tabbar.cpp")
		list(APPEND SOURCES
			tabbar.cpp
		)
		list(APPEND HEADERS
			tabbar.h
		)
	endif()
endif()

qt4_wrap_ui(UI_FORMS ${FORMS})
qt_wrap_cpp(MOC_SOURCES ${SOURCES} ${HEADERS})
add_library(widgets STATIC ${SOURCES} ${HEADERS} ${MOC_SOURCES} ${UI_FORMS} ${PLAIN_SOURCES} ${PLAIN_HEADERS})
target_link_libraries(widgets ${QT_LIBRARIES} ${iris_LIB} libpsi_tools)
target_include_directories(widgets PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
