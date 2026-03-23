# Howto build Psi using cmake utility

## Prepare sources:

```sh
mkdir buildir && cd buildir
cmake FLAGS ..
```

  Here instead of the `FLAGS` can be the flags from the "Useful CMAKE FLAGS" section.
  You may omit the flags.

## Build sources:

```sh
cmake --build . --target all --
```

or

```sh
make
```

## Install Psi:

```sh
cmake --build . --target install --
```

or

```sh
make install
```

## To run Psi without installation:

```sh
cmake --build . --target prepare-bin
```

or

```sh
make prepare-bin
cd psi && ./psi && cd .. #For Psi
cd psi && ./psi-plus && cd .. #For Psi+
```

## Useful CMAKE FLAGS:

> -DPSI_PLUS=ON

  Build Psi+ client instead of Psi.
  (default `OFF`)

> -DPSI_LIBDIR=${path}

  Path to Psi libraries directory. Path to the directory
  where Psi will search plugins.

> -DPSI_DATADIR=${path}

  Path to Psi data directory. Path to the directory
  where Psi will search datafiles (iconpacks, themes, etc).

> -DCMAKE_INSTALL_PREFIX=prefix

  To set installation prefix (installation folder).

> -DBUNDLED_IRIS=ON

  To build the `Iris` library bundled.
  (default `ON`)

> -DBUNDLED_IRIS_ALL=ON

  To build `iris` library bundled with `QCA` bundled and `USRSCTP` bundled.
  (default `OFF`)

> -DIRIS_BUNDLED_QCA=ON

  To build `QCA` library from https://github.com/psi-im/qca with Psi.
  Adds: `DTLS`, `Blake2b` and other useful for XMPP crypto-stuff.
  (default `OFF`)

> -DJINGLE_SCTP=ON

  Enable `SCTP` over `ICE Jingle` transport / data channels.
  Adds the `BUNDLED_USRSCTP` option if `ON`.
  (default `ON`)

> -DIRIS_BUNDLED_USRSCTP=ON

  To build `usrsctp` library (required for p2p file transfer) from the official GitHub repository.
  Compile compatible `usrsctp` lib when system one is not available or incompatible with Psi.
  Available only if `JINGLE_SCTP` flag is set to `ON`).
  (default `OFF`)

> -DBUNDLED_KEYCHAIN=ON

  To build `QtKeychain` library bundled  from the official GitHub repository.
  Useful for build on macOS.
  (default `OFF`)

> -DUSE_ASPELL=OFF

  To use `Aspell` spellchecker.
  (default `OFF`)

> -DUSE_ENCHANT=ON

  To use `Enchant` spellchecker.
  (default `OFF`)

> -DUSE_HUNSPELL=ON

  To use `Hunspell` spellchecker.
  (default `ON`)

> -DCHAT_TYPE=BASIC

   To set a type of chatlog engine. Possible values: `WEBKIT`, `WEBENGINE`, `BASIC`.
   Default value: `BASIC`.

> -DPSI_VERSION=${version}

  To set the Psi version manually.
  Example for Psi+: `1.0.40 (2017-06-05, Psi:a7d2d7b8, Psi+:055e945, webkit)`.
  Script sets this flag automatically from `version` file if it exists in sources directory

> -DCMAKE_BUILD_TYPE=Release

  To set a build type. Possible values: `DEBUG`, `RELEASE`, `RELWITHDEBINFO`, `MINSIZEREL`.
  (default: `Release`)

> -USE_CCACHE=ON

  To enable `ccache` utility support.
  (default: `ON`)

> -DUSE_MXE=ON

  Enables `MXE` ("M cross environment") support.
  Disables `USE_CCACHE` and adds new dependencies, if the flag `PRODUCTION` is on.
  Script can automatically detect `MXE`.
  (default: `OFF`)

> -DVERBOSE_PROGRAM_NAME=ON

  Verbose output program name.
  Experimental flag. After compilation will be created a binary file not with
  the name `psi` or `psi-plus`, but, for example, `psi-webkit` or `psi-plus-webengine`.
  (default `OFF`)

> -DPRODUCTION=ON

  To build a release version of Psi/Psi+.
  (Default for the Psi is `ON`, for the Psi+ is `OFF`)

> -DUSE_KEYCHAIN=ON

  To enable `Qt5Keychain` library support.

> -DBUILD_PSIMEDIA=ON

  To build `psimedia` plugin if sources are found in the project folder.

> -DONLY_BINARY=OFF

  When `ON` then only the binary file will be installed.

> -DINSTALL_EXTRA_FILES=ON

  If `OFF` - sounds, certificates, iconsets, themes and `client_icons.txt`
  file will not be installed.

> -DINSTALL_PLUGINS_SDK=ON

  If this flag is `ON` then with Psi will be installed PluginsAPI
  that needed to build plugins separately of main program sources.
  This might be useful for package maintainers.
  (default `OFF`)

> -DENABLE_PLUGINS=ON

  To build plugins.
  If there are no plugins in the `plugins` directory, the script will fail with an error.
  (default `OFF`)

> -DONLY_PLUGINS=ON

  To build only plugins without build of the Psi.
  On enabling this flag `ENABLE_PLUGINS` flag turns on automatically.
  (default `OFF`)

> -DDEV_MODE=ON

  Useful to debug plugins.
  On Windows, enables the `prepare-bin-libs` build target.
  This flag is useful for running Psi immediately after building during development.
  When this flag is enabled, the script searches for dependency libraries 
  and copies them to the build directory with the command: `make prepare-bin-libs`.
  
  On Linux, it enables developer mode and, together with the `ENABLE_PLUGINS` flag.
  When using `psi-plus-snapshots` allows debugging plugins without installing the Psi.
  Sets the `PSI_DATA` directory to the current executable directory.

> -DUSE_XSS=ON

  In Linux adds `XScreensaver` support.
  (default `ON`).

> -DUSE_DBUS=ON

  In Linux enables `DBus` support for client management, notifications, tunes.
  (default `ON`).

> -DUSE_X11=ON

  Enable `X11` features support.
  (default `ON`)

> -DLIMIT_X11_USAGE=ON

  Disable usage of `X11` features which may crash program.
  (default `OFF`)

> -DUSE_TASKBARNOTIFIER=ON

  Shows the incoming events count on the program icon.
  For Linux systems, it uses the `DBus` service `com.canonical.Unity` if available.
  On Windows is used an icon overlay mechanism.
  Or simply changes the program icon for other cases.
  (default `ON`)

## Work with plugins:

### Next flags are working only if ENABLE_PLUGINS or ONLY_PLUGINS are enabled

> -DBUILD_PLUGINS=${plugins}

  To set a list of plugins to build. To build all plugins: `-DBUILD_PLUGINS="ALL"` or do not set this flag.

  Possible values for the `${plugins}` (you may determine by a contents of the [plugins/generic](https://github.com/psi-im/plugins/tree/master/generic) directory):
   - `attentionplugin`
   - `autoreplyplugin`
   - `battleshipgameplugin`
   - `birthdayreminderplugin`
   - `chessplugin`
   - `cleanerplugin`
   - `clientswitcherplugin`
   - `conferenceloggerplugin`
   - `contentdownloaderplugin`
   - `enummessagesplugin`
   - `extendedmenuplugin`
   - `extendedoptionsplugin`
   - `gomokugameplugin`
   - `historykeeperplugin`
   - `imageplugin`
   - `imagepreviewplugin`
   - `jabberdiskplugin`
   - `juickplugin`
   - `messagefilterplugin`
   - `omemoplugin`
   - `openpgpplugin`
   - `otrplugin`
   - `pepchangenotifyplugin`
   - `qipxstatusesplugin`
   - `skinsplugin`
   - `stopspamplugin`
   - `storagenotesplugin`
   - `translateplugin`
   - `videostatusplugin`
   - `watcherplugin`

  Example:

  > -DBUILD_PLUGINS="chessplugin;otrplugin"

  The `BUILD_PLUGINS` variable can also be used as a blacklist.
  In this case, all plugins will be compiled, except those indicated.
  To do this, just specify the variable asЖ

  > -DBUILD_PLUGINS="-chessplugin;-otrplugin"

  and plugins `chessplugin` and `otrplugin` will not be assembled.

  **ATTENTION!** Mixing white and blacklists is not allowed.

> -DPLUGINS_ROOT_DIR=${path}

  Path to the include directory to build plugins outside of Psi
  sources (path to the `plugins.cmake` file)

> -DPLUGINS_PATH=${path}

  To install plugins into `${path}`. To install into default suffix:

  `-DPLUGINS_PATH=lib/psi-plus/plugins` or do not set this flag.

  For example, to install plugins into `~/.local/share/psi+/plugins`:

  > -DCMAKE_INSTALL_PREFIX=$HOME/.local -DPLUGINS_PATH=share/psi+/plugins

  For example, to install plugins into `/usr/share/psi-plus/plugins`:

  > -DCMAKE_INSTALL_PREFIX=/usr -DPLUGINS_PATH=share/psi-plus/plugins

## Win32 or MXE Section:

No program installation is required for assembly under Windows OS!!!

> -DENABLE_PORTABLE=ON

  To build a portable version (this is done for convenience, so you don't have to rename the binary manually).
  The output will be "binary-name-portable.exe", and if `VERBOSE_PROGRAM_NAME=ON`,
  then "extended-binary-name-portable.exe".
  Enabling this flag automatically makes the `prepare-bin-libs` build target available.
  (The default is `OFF`).

> -DQCA_DIR=DIRECTORY

  To set `Qca` library root directory.

> -DZLIB_ROOT=DIRECTORY

  To set `Zlib` library root directory.

> -DHUNSPELL_ROOT=DIRECTORY

  To set `Hunspell` library root directory.

> -DNO_DEBUG_OPTIMIZATION=OFF

  Disable optimization for debug builds. Windows only.
  (default `OFF`)

### To build OTR plugin in OS WINDOWS you need to set additional variables

> -DLIBGCRYPT_ROOT=%LIBGCRYPT_ROOT%

  Set path to `LIBGCRYPT` library root directory.

> -DLIBGPGERROR_ROOT=%LIBGPGERROR_ROOT%

  Set path to `LIBGPG-ERROR` library root directory.

> -DLIBOTR_ROOT=%LIBOTR_ROOT%

  Set path to the `LIBOTR` library root directory.

> -DLIBTIDY_ROOT=%LIBTIDY_ROOT%

  Set path to the `LIBTIDY` library root directory.

  For example:

  `-DLIBGCRYPT_ROOT=C:\libgcrypt -DLIBGPGERROR_ROOT=C:\libgpg-error -DLIBOTR_ROOT=C:\libotr -DLIBTIDY_ROOT=C:\libtidy`

> -DNO_DEBUG_OPTIMIZATION=OFF

  Disable compiler optimizations when building the debug version.
  (default is `OFF`)


### If you are using Psi SDK you need to set SDK_PATH:

> -DSDK_PATH=path

  If you set this flag, you don't need to set the flags for the root directories 
  of the libraries dependencies.

### macOS specific flags

> -DUSE_SPARKLE=ON

  Use Sparkle for macOS builds.
  (default `ON`)

> -DUSE_GROWL=OFF

  Use growl for macOS builds.
  (default `OFF`)

> -DUSE_MAC_DOC=OFF

  Use macOS dock.
  (default `OFF`)
