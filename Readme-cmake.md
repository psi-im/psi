# Howto build Psi/Psi+ using cmake utility

## Prepare sources:

```sh
mkdir build && cd build
cmake FLAGS ..
```

  instead of FLAGS can be the flags from the "Usefull CMAKE FLAGS" section

## Build sources:

```sh
cmake --build . --target all --
```

or

```sh
make
```

## Install Psi/Psi+:

```sh
cmake --build . --target install --
```

or

```sh
make install
```

## To run Psi/Psi+ without installation:

```sh
cmake --build . --target prepare-bin
```

or

```sh
make prepare-bin
cd psi && ./psi && cd .. #For Psi
cd psi && ./psi-plus && cd .. #For Psi+
```

## Usefull CMAKE FLAGS:

> -DPSI_PLUS=ON

  Build Psi+ client instead of Psi (default OFF)

> -DPSI_LIBDIR=${path}

  Path to Psi/Psi+ libraries directory. Path to the directory
  where Psi/Psi+ will search plugins.

> -DPSI_DATADIR=${path}

  Path to Psi/Psi+ data directory. Path to the directory
  where Psi/Psi+ will search datafiles (iconpacks, themes, etc)

> -DCMAKE_INSTALL_PREFIX=prefix

  to set installation prefix

> -DBUNDLED_IRIS=ON

  to build the Iris library bundled (default ON)

> -DBUNDLED_IRIS_ALL=ON

  to build iris library bundled with QCA bundled and USRSCTP bundled (default OFF)

> -DIRIS_BUNDLED_QCA=ON

  to build qca library from https://github.com/psi-im/qca with Psi
  Adds: DTLS, Blake2b and other useful for XMPP crypto-stuff

> -DJINGLE_SCTP=ON

  enable SCTP over ICE Jingle transport / data channels
  adds the BUNDLED_USRSCTP option if ON (default ON)

> -DIRIS_BUNDLED_USRSCTP=ON

  to build usrsctp library from the official GitHub repository,
  Compile compatible usrsctp lib when system one is not available or
  incompatible (required for p2p file transfer. Available only if
  JINGLE_SCTP flag is set to ON)

> -DBUNDLED_KEYCHAIN=ON

  to build QtKeychain library bundled (useful for macOS) (default OFF)

> -DUSE_ASPELL=OFF

  to use Aspell spellchecker (default OFF)

> -DUSE_ENCHANT=ON

  to use Enchant spellchecker (default OFF)

> -DUSE_HUNSPELL=ON

  to use Hunspell spellchecker (default ON)

> -DUSE_HUNSPELL=ON

  to use Aspell spellchecker (default OFF)

> -DCHAT_TYPE=BASIC

   to set a type of chatlog engine. Possible values: `WEBKIT`, `WEBENGINE`, `BASIC`
   default value: `BASIC`

> -DPSI_VERSION=${version}

  to set the Psi/Psi+ version manually
  Example for Psi+: `1.0.40 (2017-06-05, Psi:a7d2d7b8, Psi+:055e945, webkit)`.
  Script sets this flag automatically from `version` file if it exists in sources directory

> -DCMAKE_BUILD_TYPE=Release (default: Release)

  to set a build type. Possible values: `DEBUG`, `RELEASE`, `RELWITHDEBINFO`, `MINSIZEREL`

> -USE_CCACHE=ON (default: ON)

  to enable ccache utility support

> -DUSE_MXE=ON (default: OFF)

  Enables MXE (M cross environment) support.
  Disables USE_CCACHE. Script can automatically detect MXE.

> -DVERBOSE_PROGRAM_NAME=ON

  Verbose output program name. (default OFF)
  Experimental flag. Exmaple of output name: psi-plus-webkit

> -DPRODUCTION=ON

  to build a release version of Psi/Psi+

> -DUSE_KEYCHAIN=ON

  to enable Qt5Keychain library support

> -DBUILD_PSIMEDIA=ON

  build psimedia plugin if sources are found in the project folder

> -DONLY_BINARY=OFF

  When ON then only the binary file will be installed

> -DINSTALL_EXTRA_FILES=ON

  If OFF - sounds, certificates, iconsets, themes and cilent_icons.txt
  file will not be installed

> -DINSTALL_PLUGINS_SDK=ON

  If this flag is ON then with psi will be installed PluginsAPI that needed
  to build plugins separately of main program sources
  (default OFF)

> -DENABLE_PLUGINS=ON

  to build psi plugins (default OFF)

> -DONLY_PLUGINS=ON

  to build only psi plugins (default OFF). On enabling this flag
  ENABLE_PLUGINS flag turns on automatically

> -DDEV_MODE=ON

  In OS Windows enables prepare-bin-libs target. Allows copying the necessary libraries to run Psi/Psi+.
  In Linux sets PSI_DATA directory to the current binary directory (Usefully to debug plugins)

> -DUSE_XSS=ON

  In Linux OS adds XScreensaver support (default ON).

> -DUSE_DBUS=ON

  In Linux OS enables DBus support for client management, notifications, tunes (default ON).

> -DUSE_X11=ON

  Enable X11 features support (default ON)

> -DLIMIT_X11_USAGE=ON

  Disable usage of X11 features which may crash program (default OFF)

> -DUSE_TASKBARNOTIFIER=ON

  Shows the incoming events count on the program icon.
  For Linux systems, it uses the DBus service com.canonical.Unity if available.
  Windows it uses an icon overlay mechanism.
  Or simply changes the program icon for other cases. (default ON)

## Work with plugins:

### Next flags are working only if ENABLE_PLUGINS or ONLY_PLUGINS are enabled

> -DBUILD_PLUGINS=${plugins}

  set a list of plugins to build. To build all plugins: `-DBUILD_PLUGINS="ALL"` or do not set this flag

  possible values for the `${plugins}`:
   - historykeeperplugin
   - stopspamplugin
   - juickplugin
   - translateplugin
   - gomokugameplugin
   - attentionplugin
   - cleanerplugin
   - autoreplyplugin
   - contentdownloaderplugin
   - qipxstatusesplugin
   - skinsplugin
   - clientswitcherplugin
   - watcherplugin
   - videostatusplugin
   - screenshotplugin
   - jabberdiskplugin
   - storagenotesplugin
   - extendedoptionsplugin
   - imageplugin
   - extendedmenuplugin
   - birthdayreminderplugin
   - pepchangenotifyplugin
   - omemoplugin
   - openpgpplugin
   - otrplugin
   - chessplugin
   - conferenceloggerplugin
   - enummessagesplugin
   - httpuploadplugin
   - imagepreviewplugin

  Example:

  > -DBUILD_PLUGINS="chessplugin;otrplugin"

  The BUILD_PLUGINS variable can also be used as a blacklist.
  In this case, all plugins will be compiled, except those indicated.
  To do this, just specify the variable as

  > -DBUILD_PLUGINS="-chessplugin;-otrplugin"

  and plugins `chessplugin` and `otrplugin` will not be assembled

  ATTENTION! Mixing white and blacklists is not allowed.

> -DPLUGINS_ROOT_DIR=${path}

  Path to the include directory to build plugins outside of Psi/Psi+
  sources (path to the plugins.cmake file)

> -DPLUGINS_PATH=${path}

  to install plugins into `${path}`. To install into default suffix:

  -DPLUGINS_PATH=lib/psi-plus/plugins or do not set this flag

  For example, to install plugins into `~/.local/share/psi+/plugins`:

  > -DCMAKE_INSTALL_PREFIX=$HOME/.local -DPLUGINS_PATH=share/psi+/plugins

  For example, to install plugins into `/usr/share/psi-plus/plugins`:

  > -DCMAKE_INSTALL_PREFIX=/usr -DPLUGINS_PATH=share/psi-plus/plugins

## Win32 or MXE Section:

> -DQCA_DIR=DIRECTORY

  to set Qca library root directory

> -DZLIB_ROOT=DIRECTORY

  to set Zlib library root directory

> -DHUNSPELL_ROOT=DIRECTORY

  to set Hunspell library root directory

> -DENABLE_PORTABLE=ON

  to build a portable version (not need to rename binary).
  Enables prepare-bin-libs target.

> -DNO_DEBUG_OPTIMIZATION=OFF

  Disable optimization for debug builds. Windows only (default OFF)

### To build OTRPLUGIN in OS WINDOWS you need to set additional variables

> -DLIBGCRYPT_ROOT=%LIBGCRYPT_ROOT%

  path to `LIBGCRYPT` library root directory

> -DLIBGPGERROR_ROOT=%LIBGPGERROR_ROOT%

  path to `LIBGPG-ERROR` library root directory

> -DLIBOTR_ROOT=%LIBOTR_ROOT%

  path to LIBOTR library root directory

> -DLIBTIDY_ROOT=%LIBTIDY_ROOT%

  path to LIBTIDY library root directory

  For example:

  > -DLIBGCRYPT_ROOT=C:\libgcrypt -DLIBGPGERROR_ROOT=C:\libgpg-error -DLIBOTR_ROOT=C:\libotr -DLIBTIDY_ROOT=C:\libtidy

### If you are using Psi+ SDK you need to set SDK_PATH:

> -DSDK_PATH=path

### macOS specific flags

> -DUSE_SPARKLE=ON

  Use Sparkle for macOS builds (default ON)

> -DUSE_GROWL=OFF

  Use growl for macOS builds (default OFF)

> -DUSE_MAC_DOC=OFF

  Use macOS dock (default OFF)
