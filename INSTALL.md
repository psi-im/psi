## Installation

See "Packages and installers" section in [README.md](README.md) or [README.html](README.html).


## Build from sources

### Build dependencies

* compiler with C++17 support
* ccache (optional)
* cmake >= 3.2.0
* Qt >= 5.6.0
* QtWebKit (optional)
* QtWebEngine (optional)
* QCA (from [upstream](https://api.kde.org/qca/html/) or from our [fork](https://github.com/psi-im/qca))
* QtKeychain (optional)
* OpenSSL (required)
* zlib (required)
* Minizip (optional)
* Hunspell (optional)
* Aspell (optional)
* Enchant (optional)
* X11 (optional, Linux only)
* Xscreensaver (optional, Linux only)
* Sparkle (optional, macOS only)
* Growl (optional, macOS only)
* libhttp-parser (optional, for plugins only)
* libotr (optional, for plugins only)
* libtidy (optional, for plugins only)
* libsignal-protocol-c (optional, for plugins only)

### Common

```shell
mkdir builddir
cd builddir
cmake ..
# If necessary install all missed build dependencies until previous command is
# executed without errors.
make -j4
make install DESTDIR="../installdir"
# If necessary replace "../installdir" from command above to any path you need
# or copy them manually from "../installdir".
```

Available configuration options for cmake are described in [Readme-cmake.txt](Readme-cmake.txt).

### Linux

Installation of Psi build dependencies in Debian and Ubuntu:

```shell
sudo apt install -qq \
        libhunspell-dev \
        libminizip-dev \
        libqca-qt5-2-dev \
        libqt5svg5-dev \
        libqt5webkit5-dev \
        libqt5x11extras5-dev \
        libsm-dev \
        libssl-dev \
        libxss-dev \
        qt5keychain-dev \
        qtmultimedia5-dev \
        zlib1g-dev
```

Installation of additional build dependencies (for plugins) in Debian and Ubuntu:

```shell
sudo apt install -qq \
        libhttp-parser-dev \
        libotr5-dev \
        libsignal-protocol-c-dev \
        libtidy-dev
```

### macOS

There are different ways for building program in macOS. The easiest one is build using Homebrew. See instructions in [mac/build-using-homebrew.sh](mac/build-using-homebrew.sh).

### Windows

To be written...



## Useful links

* Build scripts for packages in Debian:
[psi](https://salsa.debian.org/xmpp-team/psi)
[psi-plus](https://salsa.debian.org/xmpp-team/psi-plus)
* Build scripts for package in Haiku:
[psi_plus](https://github.com/haikuports/haikuports/tree/master/net-im/psi_plus)
* Scripts for building of portable builds for Windows:
[cross-compilation-using-mxe](https://github.com/psi-plus/maintenance/tree/master/scripts/win32/cross-compilation-using-mxe)

To be continued...

