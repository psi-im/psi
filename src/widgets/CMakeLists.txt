unset(HEADERS)
unset(FORMS)
unset(SOURCES)
unset(UI_FORMS)
unset(EXTRA_LDFLAGS)

set(SOURCES
    LongListWidget.cpp
    OverlayScrollBarManager.cpp
    ThinSplitter.cpp
    NativeChatThemeRegistry.cpp
    NativeChatRenderers.cpp
    ChatMessageText.cpp
    CodeBlockWidget.cpp
    MessagePreview.cpp
    LinkButton.cpp
    MamChatWidget.cpp
    MamImageViewer.cpp
    MamHistorySource.cpp
    LongListWidgetLayout.cpp
    busywidget.cpp
    emojiregistry.cpp
    eventnotifier.cpp
    fancylabel.cpp
    fancypopup.cpp
    grepshortcutkeydialog.cpp
    iconaction.cpp
    iconselect.cpp
    iconwidget.cpp
    psirichtext.cpp
    psicodehighlight.cpp
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
    LongListWidget.h
    OverlayScrollBarManager.h
    ThinSplitter.h
    NativeChatThemeRegistry.h
    NativeChatRenderers.h
    ChatMessageText.h
    CodeBlockWidget.h
    psicodelanguage.h
    MessagePreview.h
    LinkButton.h
    MamChatWidget.h
    MamImageViewer.h
    MamHistorySource.h
    actionlineedit.h
    busywidget.h
    emojiregistry.h
    emojidb.h
    eventnotifier.h
    fancylabel.h
    fancypopup.h
    fancypopuplist.h
    grepshortcutkeydialog.h
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

if(USE_TASKBARNOTIFIER)
    list(APPEND SOURCES taskbarnotifier.cpp)
    list(APPEND HEADERS taskbarnotifier.h)
    if(WIN32 AND (${QT_DEFAULT_MAJOR_VERSION} VERSION_LESS "6"))
        find_package(Qt${QT_DEFAULT_MAJOR_VERSION} 5.10 REQUIRED COMPONENTS WinExtras)
        list(APPEND QT_LIBRARIES Qt${QT_DEFAULT_MAJOR_VERSION}::WinExtras)
    endif()
endif()

set(FORMS
    fancypopup.ui
    grepshortcutkeydialog.ui
)

qt_wrap_ui(UI_FORMS ${FORMS})
add_library(widgets STATIC ${SOURCES} ${HEADERS} ${UI_FORMS})
# Share qite's property range with the application, which builds qite itself.
# Its QTextFormat properties must not overlap Psi's rich-text properties.
target_compile_definitions(widgets PUBLIC QITE_FIRST_USER_PROPERTY=0x1000)
if(WIN32)
    target_link_libraries(widgets shlwapi)
endif()
target_link_libraries(widgets ${QT_LIBRARIES} ${iris_LIB} tools qsourcehighlite)
target_include_directories(widgets PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_LIST_DIR})
if(LINUX AND USE_X11)
    target_compile_definitions(widgets PRIVATE HAVE_X11)
endif()

if(BUILD_TESTING)
    add_subdirectory(unittest/richtext)
endif()
