This file contains the CMake files description
(useful for developers)

./CMakeLists.txt - root file (the main script):
    contains a list of general options;
    determines the program type Psi/Psi+;
    determines chatlog type Basic/Webkit/Webengine;
    contains general compile definitions;
    enables ccache, mxe;
    contains useful copy function;
    adds iris, 3rdparty, src, plugins subdirectories;
    in case of only plugins build - enables plugins;
    if plugins/generic/psimedia directory exists and BUILD_PSIMEDIA flag is
    enabled - builds psimedia library.

./icondef.xml.in - template-file to generate icondef.xml file
./iconsets.qrc.in - template-file to generate iconsets.qrc file

./3rdparty/CMakeLists.txt - builds qhttp static library
./3rdparty/qite/libqite/libqite.cmake - contains qite project file list

./cmake/modules - directory contains modules for searching libraries, for determining a
    program version, for work with sources and generation of additional targets and files.
./cmake/modules/COPYING-CMAKE-SCRIPTS - license file (to solve problems with mainaining)
./cmake/modules/FindLibGpgError.cmake - finds libgpg-error library
./cmake/modules/FindQJDns.cmake - finds qjdns library
./cmake/modules/FindEnchant.cmake - finds enchant library
./cmake/modules/FindLibOtr.cmake - finds otr library
./cmake/modules/FindQJSON.cmake - finds qjson library (disabled as outdated)
./cmake/modules/FindHunspell.cmake - finds hunspell library
./cmake/modules/FindLibTidy.cmake - finds tidy or html-tidy library
./cmake/modules/FindSparkle.cmake - finds sparkle (not yet tested)
./cmake/modules/FindMINIZIP.cmake - finds minizip library
./cmake/modules/FindXCB.cmake - finds xcb library
./cmake/modules/FindLibGcrypt.cmake - finds libgctypt or libgcrypt2 library
./cmake/modules/FindQca.cmake - finds qca-qt5 library
./cmake/modules/FindZLIB.cmake - finds zlib library
./cmake/modules/FindPsiPluginsApi.cmake - module to search files useful at build plugins
./cmake/modules/get-version.cmake - determines clien version using git utility or by parsing
    a ../version file
./cmake/modules/win32-prepare-deps.cmake - generates a list of files to install
    with make prepare-bin-libs command, which install dependency libraries
    into output build directory. Or using windeployqt utility if available
    with make windeploy command
./cmake/modules/generate_desktopfile.cmake - generates .desktop file
./cmake/modules/fix-codestyle.cmake - module for codestyle fix using clang-format

./iris/CMakeLists.txt - builds iris library:
    contains options for the iris library (duplicated in a main script)
    enables qjdns (if option enabled)
    adds src/irisnet and src/xmpp subdirectories

/***
./iris - this directory contains an additional files to build iris library outside of the Psi
    project. This functionality is partially realized. Needs reworking.
***/

./iris/cmake/modules - modules for libraries searching (copies of Psi modules)

./iris/src/irisnet/CMakeLists.txt - builds irisnet static library

./iris/src/xmpp/CMakeLists.txt - builds iris static library

./src/CMakeLists.txt - builds and installs the Psi/Psi+ project files:
    adds ../translations directory (if exists) and compiles translations files
    finds an additional libraries and static libraries and adds them to project
    sets the main program name based on the main script data
    generates config.h file based on the config.h.in template file
    generates psi_win.rc based on the ../win32/psi_win.rc.in template file and
        compiles psi_win.o file
    adds src.cmake file
    adds ../3rdparty/qite/libqite/libqite.cmake file
    adds irisprotocol/irisprotocol.cmake file
    adds protocol/protocol.cmake file
    adds ../plugins/plugins.cmake file
    adds AutoUpdater, options, tabs, privacy, Certificates, avcall, psimedia,
        contactmanager, tools, libpsi/dialogs, libpsi/tools, widgets, sxe,
        whiteboarding subdirectories
    substitutes the output program name with an extended name (if enabled)
    generates description files for default iconsets
    if name is extended generates the .desktop file using generate_desktopfile.cmake module
    creates an installation rules
    if DEV_MODE option is enabled generates an installation rules for dependency
        libraries to install into binary output directory using win32-prepare-deps.cmake
        module

./src/config.h.in - template-file for config.h file creation

./src/src.cmake:
    contains the default compile definitions;
    contains a lists with the main source files:
        FORMS list - contains ui files;
        HEADERS list - contains C/C++ header files
        SOURCES list - contains C/C++ source files

./src/AutoUpdater/CMakeLists.txt - builds AutoUpdater static library

./src/avcall/CMakeLists.txt - builds avcall static library

./src/Certificates/CMakeLists.txt - builds Certificates static library

./src/contactmanager/CMakeLists.txt - builds contactmanager static library

./src/irisprotocol/irisprotocol.cmake - a list of source files which will be added into
    the main sources list

./src/libpsi/dialogs/CMakeLists.txt - builds libpsi_dialogs static library

./src/libpsi/tools/CMakeLists.txt - builds libpsi_tools static library:
    adds zip subdirectory

./src/libpsi/tools/zip/CMakeLists.txt - builds zip static library

./src/options/CMakeLists.txt - builds options static library

./src/privacy/CMakeLists.txt - builds privacy static library

./src/protocol/protocol.cmake - a list of source files which will be added into
    the main sources list

./src/psimedia/CMakeLists.txt - build psimedia static library

./src/sxe/CMakeLists.txt - builds sxe static library

./src/tabs/CMakeLists.txt - build tabs static library

./src/tools/CMakeLists.txt - builds tools static library

./src/whiteboarding/CMakeLists.txt - builds whiteboarding static library

./src/widgets/CMakeLists.txt - builds widgets static library

./plugins/plugins.cmake - a list of header files which will be added into
    the main headers list

./plugins/variables.cmake.in - template-file, contains the main variables useful
    to build plugins, general for all plugins

./plugins/pluginsconf.pri.cmake.in - template-file to generate pluginsconf.pri file

./plugins/CMakeLists.txt - main script to rule the plugins
    contains the main build rules for all plugins
    adds generic, unix, dev subdirectories

./plugins/generic/CMakeLists.txt:
    adds subrirectory for each plugin, if BUILD_PLUGINS variable is set it adds only
    plugins from BUILD_PLUGINS list (or exclude plugins)

./plugins/dev/CMakeLists.txt:
    adds subrirectory for each plugin, if BUILD_PLUGINS variable is set it adds only
    plugins from BUILD_PLUGINS list (or exclude plugins)

./plugins/unix/CMakeLists.txt:
    adds subrirectory for each plugin, if BUILD_PLUGINS variable is set it adds only
    plugins from BUILD_PLUGINS list (or exclude plugins)

./plugins/*TYPE*/*PLUGIN*/CMakeLists.txt - builds and installs a plugin
    whit "*TYPE*" type and "*PLUGIN*" name. The structure of these files are mostly
    the same

./win32/win32_definitions.cmake - provides the next features:
    determines PSI_SDK and GSTREAMER_SDK pathes
    determines the MinGW or MSVC compilation flags
    contains the main definitions for OS Windows

./win32/psi_win.rc.in - template-file to generate psi_win.rc file

/***
USEFUL TIPS
***/

/***
For a comfortable plugins packages creation and to aviod downloading all the Psi
sources or copying of these sources if INSTALL_PLUGINS_SDK flag is enabled
while installing Psi using CMake-scripts could be installed an additional files
together with the main program files such as:
Plugins API (includes, *.pri files, variables.cmake)
(path $prefix/share/name_of_program/plugins)
And also could be installed Plugins API search module
FindPsiPluginsApi.cmake
(path $prefix/share/cmake/Modules)
In this case the variables.cmake generates at Psi build and contains:
 - path to the plugins install directory
 - path to the psi/psi+ data directory
 - name of a program (psi or psi-plus)
 - plugins defenitions
 Also the CMake-script generates *.pri files, needed to build plugins using qmake utility
***/

/***
If there is a plguin debug necessity in OS Linux you can do next:
- download psi-plus-snapshots repository
- open CMakeLists.txt from root directory using qtcreator
- choose profile and build type
- enable both ENABLE_PLUGINS and DEV_MODE flags
- clean CMake cache if needed and run CMake (using qtcreator build menu)
- build project
That's all. You can debug plugin with the psi/psi+ program.
***/
