set(CMAKE_CXX_STANDARD 17)

unset(HEADERS)
unset(FORMS)
unset(SOURCES)
unset(UI_FORMS)
unset(EXTRA_LDFLAGS)

if(APPLE)
    if(USE_GROWL)
        list(APPEND EXTRA_LDFLAGS
            "-framework Growl -framework Cocoa"
            )
        list(APPEND HEADERS
            growlnotifier/growlnotifier.h
            )
        list(APPEND SOURCES
            growlnotifier/growlnotifier.mm
            )
    endif()
    if(USE_MAC_DOC)
        list(APPEND EXTRA_LDFLAGS
            "-framework Carbon"
            )
        list(APPEND HEADERS
            mac_dock/mac_dock.h
            mac_dock/privateqt_mac.h
            )
        list(APPEND SOURCES
            mac_dock/mac_dock.mm
            mac_dock/privateqt_mac.mm
            )
    endif()
endif()

list(APPEND HEADERS
    # tools
    priorityvalidator.h

    # idle
    idle/idle.h

    # systemwatch
    systemwatch/systemwatch.h

    # globalshortcut
    globalshortcut/globalshortcuttrigger.h

    # tools
    maybe.h
    iodeviceopener.h
    languagemanager.h

    # atomicxmlfile
    atomicxmlfile/atomicxmlfile.h

    # globalshortcut
    globalshortcut/globalshortcutmanager.h

    # simplecli
    simplecli/simplecli.h

    # spellchecker
    spellchecker/spellchecker.h
    spellchecker/spellhighlighter.h
    )

list(APPEND SOURCES
    # tools
    priorityvalidator.cpp
    languagemanager.cpp

    # spellchecker
    spellchecker/spellchecker.cpp
    spellchecker/spellhighlighter.cpp

    # tools
    iodeviceopener.cpp

    # idle
    idle/idle.cpp

    # atomicxmlfile
    atomicxmlfile/atomicxmlfile.cpp

    # globalshortcut
    globalshortcut/globalshortcutmanager.cpp

    # systemwatch
    systemwatch/systemwatch.cpp

    # simplecli
    simplecli/simplecli.cpp
    )

if(APPLE)
    list(APPEND HEADERS
        mac_dock/mac_dock.h
        mac_dock/privateqt_mac.h
        )

    list(APPEND SOURCES
        # globalshortcut
        globalshortcut/globalshortcutmanager_mac.mm
        globalshortcut/NDKeyboardLayout.m

        mac_dock/mac_dock.mm
        mac_dock/privateqt_mac.mm
        )

    list(APPEND HEADERS
        # systemwatch
        systemwatch/systemwatch_mac.h

        # globalshortcut
        globalshortcut/NDKeyboardLayout.h
        )

    list(APPEND SOURCES
        #idle
        idle/idle_mac.cpp

        # systemwatch
        systemwatch/systemwatch_mac.cpp
        )
elseif(WIN32)
    list(APPEND HEADERS
        # spellchecker
        spellchecker/hunspellchecker.h
        )

    list(APPEND SOURCES
        #idle
        idle/idle_win.cpp

        # systemwatch
        systemwatch/systemwatch_win.cpp

        # globalshortcut
        globalshortcut/globalshortcutmanager_win.cpp

        # spellchecker
        spellchecker/hunspellchecker.cpp
        )
elseif(HAIKU)
        list(APPEND HEADERS
        # systemwatch
        systemwatch/systemwatch_unix.h
        )

        list(APPEND SOURCES
        #idle
        idle/idle_x11.cpp

        # systemwatch
        systemwatch/systemwatch_unix.cpp

        # globalshortcut
        globalshortcut/globalshortcutmanager_haiku.cpp
        )
elseif(USE_X11)
    list(APPEND HEADERS
        # systemwatch
        systemwatch/systemwatch_unix.h
        )

    list(APPEND SOURCES
        #idle
        idle/idle_x11.cpp

        # systemwatch
        systemwatch/systemwatch_unix.cpp

        # globalshortcut
        globalshortcut/globalshortcutmanager_x11.cpp
        )
else()
    list(APPEND HEADERS
        # systemwatch
        systemwatch/systemwatch_unix.h
        )

    list(APPEND SOURCES
        #idle
        idle/idle_x11.cpp

        # systemwatch
        systemwatch/systemwatch_unix.cpp

        # globalshortcut
        globalshortcut/globalshortcutmanager_stub.cpp
        )
endif()

# spellchecker
if(USE_ENCHANT)
    if(Enchant_VERSION)
        if(${Enchant_VERSION} VERSION_LESS "2.0")
            add_definitions(-DHAVE_ENCHANT)
        else()
            add_definitions(-DHAVE_ENCHANT2)
        endif()
        message(STATUS "Enchant version - ${Enchant_VERSION}")
    else()
        add_definitions(-DHAVE_ENCHANT)
    endif()

    include_directories(
        ${Enchant_INCLUDE_DIR}
        )

    list(APPEND EXTRA_LDFLAGS
        ${Enchant_LIBRARY}
        )

    list(APPEND HEADERS
        spellchecker/enchantchecker.h
        )

    list(APPEND SOURCES
        spellchecker/enchantchecker.cpp
        )
elseif(USE_HUNSPELL)
    add_definitions(-DHAVE_HUNSPELL)

    if(MSVC)
        add_definitions(-DHUNSPELL_STATIC)
    endif()

    include_directories(
        ${HUNSPELL_INCLUDE_DIR}
        )

    list(APPEND EXTRA_LDFLAGS
        ${HUNSPELL_LIBRARY}
        )

    list(APPEND HEADERS
        spellchecker/hunspellchecker.h
        )

    list(APPEND SOURCES
        spellchecker/hunspellchecker.cpp
        )
elseif(APPLE)
    list(APPEND SOURCES
        spellchecker/macspellchecker.mm
        )

    list(APPEND HEADERS
        spellchecker/macspellchecker.h
        )
elseif(USE_ASPELL)
    add_definitions(-DHAVE_ASPELL)
    include_directories(
        ${ASPELL_INCLUDE_DIR}
        )
    list(APPEND EXTRA_LDFLAGS
        ${ASPELL_LIBRARIES}
        )
    list(APPEND HEADERS
        spellchecker/aspellchecker.h
        )
    list(APPEND SOURCES
        spellchecker/aspellchecker.cpp
        )
endif()

if(LINUX AND USE_XSS)
    find_package(X11 COMPONENTS Xss REQUIRED)
    include_directories(${X11_Xscreensaver_INCLUDE_PATH})
    list(APPEND EXTRA_LDFLAGS ${X11_Xscreensaver_LIB})
endif()

qt_wrap_ui(UI_FORMS ${FORMS})
add_library(libpsi_tools STATIC  ${SOURCES} ${HEADERS} ${UI_FORMS})
target_link_libraries(libpsi_tools iris ${QT_LIBRARIES} tools ${EXTRA_LDFLAGS})
target_include_directories(libpsi_tools PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(zip)
