unset(HEADERS)
unset(FORMS)
unset(SOURCES)
unset(UI_FORMS)
unset(EXTRA_LDFLAGS)

set(SOURCES
    busywidget.cpp
    emojiregistry.cpp
    fancylabel.cpp
    fancypopup.cpp
    iconaction.cpp
    iconselect.cpp
    iconwidget.cpp
    psirichtext.cpp
    psitextview.cpp
    psitooltip.cpp
    tabbar.cpp
    urlobject.cpp

    actionlineedit.cpp
    pixmapratiolabel.cpp
    psitabbar.cpp
    psitabwidget.cpp
    psitiplabel.cpp
    typeaheadfind.cpp
    urllabel.cpp
)

set(HEADERS
    actionlineedit.h
    busywidget.h
    emojiregistry.h
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
    pixmapratiolabel.h
    psitabbar.h
    psitabwidget.h
    psitextview.h
    psitiplabel.h
    tabbar.h
    typeaheadfind.h
    updatingcombobox.h
    urllabel.h
    urlobject.h

    psirichtext.h
    psitooltip.h
    stretchwidget.h
)

set(FORMS
    fancypopup.ui
)

if(PSI_PLUS AND (EXISTS "${CMAKE_CURRENT_LIST_DIR}/psiwindowheader.cpp"))
    list(APPEND SOURCES
        psiwindowheader.cpp
    )
    list(APPEND HEADERS
        psiwindowheader.h
    )
    list(APPEND FORMS
        psiwindowheader.ui
    )
endif()

qt5_wrap_ui(UI_FORMS ${FORMS})
add_library(widgets STATIC ${SOURCES} ${HEADERS} ${UI_FORMS})
target_link_libraries(widgets ${QT_LIBRARIES} ${iris_LIB} libpsi_tools)
target_include_directories(widgets PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
