# Psi &ndash; Qt-based XMPP client

Website: https://psi-im.org/ <br>
Sources: https://github.com/psi-im

## License

This program is licensed under the GNU General Public License. See the [COPYING](https://github.com/psi-im/psi/blob/master/COPYING) file for more information.

## Description

Psi is a powerful XMPP client designed for experienced users. It is highly portable and runs on GNU/Linux, MS Windows, macOS, FreeBSD and Haiku.

User interface of program is very flexible in customization. For example, there are "multi windows" and "all in one" modes, support of different iconsets and themes. Psi supports file sharing and audio/video calls. Security is also a major consideration, and Psi provides it for both client-to-server (TLS) and client-to-client (OpenPGP, OTR, OMEMO) via appropriate plugins.

WebKit version of Psi has few additional features (in comparing with basic version of Psi): support of animated emoticons, support of (adium) themes in private chats and group chats, support of previewing of images and videos in private chats and group chats, etc. But if you prefer old fashioned plain text chats from era of IRC heyday, then basic version of Psi is your obvious choice.

WebEngine version of Psi is identical to WebKit version of Psi in supporting of extra features, but it has some pros and cons:

* Better work with embedded video (in chats) in MS Windows.
* Worse integration to system. (It does not use system theme and fonts in GNU/Linux.)
* Supports less amount of compilers, systems and CPU architectures.
* Stability issues in macOS.

## Versions history

See [CHANGELOG](https://github.com/psi-im/psi/blob/master/CHANGELOG) file.

## Installation

For build from sources see [INSTALL](https://github.com/psi-im/psi/blob/master/INSTALL.md) file.

GNU/Linux and FreeBSD users may install [packages](https://github.com/psi-im/psi#packages-and-installers) from official and unofficial repositories, ports, etc.

macOS users may install and update official builds using [Homebrew](https://brew.sh/) cask:

```
brew cask install psi-plus
```

or download app bundles from [SourceForge](https://github.com/psi-im/psi#packages-and-installers) and install them manually. Program doesn't have embedded mechanism of updates, so in this case users should monitor updates themselves.

Haiku users may install official package from HaikuPorts:

```
pkgman install psi_plus
```

MS Windows users may download official installers and portable builds from [SourceForge](https://github.com/psi-im/psi#packages-and-installers). Program doesn't have embedded mechanism of updates, so users should monitor updates themselves.

## Development

In 2009 a Psi fork named [Psi+](https://psi-plus.com/) was started. Project purposes were: implementation of new features, writing of patches and plugins for transferring them to upstream. As of 2017 all active Psi+ developers have become official Psi developers and now Psi+ is just a development branch of Psi with rolling release development model.

Users who wants to receive new features and bug fixes very quickly may use Psi+ on daily basis. Users who do not care about new trends and prefer constancy may choose Psi as it uses classical development model and its releases are quite rare.

Currently the development model looks like this:

* Psi versions should look like: `X.Y` (where `X` and `Y` are not digits but numbers).
* Psi+ versions should look like: `X.Y.Z` (where `X`, `Y` and `Z` are not digits but numbers).
* All development of Psi is done in `master` branch of [psi](https://github.com/psi-im/psi) repository and its submodules. Each commit to it increases `Z` part of Psi+ version to one.
* At some point of time new release branch is detouched from `master` branch (for example, `release-1.x`, `release-2.x`, etc.). All new releases (git tags) will be done in it. Necessary changes from `master` branch to stable branches should be cherry-picked or moved manually. Do not forget to [merge release branch](admin/merge_release_to_master.sh) into `master` after adding of new git tags.
* Psi [plugins](https://github.com/psi-im/plugins) are developed separately. Plugins API in guaranteed to be compatible only for git tags. In `master` branches of [psi](https://github.com/psi-im/psi), [plugins](https://github.com/psi-im/plugins) and [psimedia](https://github.com/psi-im/psimedia) repos they may be not in sync during development. Psi+ solves this problem by providing all-in-one tarballs (Psi core + all officially supported plugins and resources), see special repository [psi-plus-snapshots](https://github.com/psi-plus/psi-plus-snapshots).
* All [translations](https://github.com/psi-plus/psi-plus-l10n) are prepared for Psi+ project and are semi-automatically [adapted](https://github.com/psi-im/psi-l10n) for Psi (using special script).

All changes are tested on Continuous Integration services:

* Travis CI: [psi](https://travis-ci.org/psi-im/psi), [plugins](https://travis-ci.org/psi-im/plugins), [psimedia](https://travis-ci.org/psi-im/psimedia), [psi-plus](https://travis-ci.org/psi-plus/psi-plus-snapshots).
* Sibuserv CI: [psi](https://sibuserv-ci.org/projects/psi), [plugins](https://sibuserv-ci.org/projects/psi-plugins), [psi-plus](https://sibuserv-ci.org/projects/psi-plus-snapshots), [psimedia](https://sibuserv-ci.org/projects/psimedia).

## Developers

### Lead developers

* 2017-now,  Sergey Ilinykh <<rion4ik@gmail.com>>, Psi and Psi+ projects
* 2010-2017, Sergey Ilinykh <<rion4ik@gmail.com>>, Psi+ project
* 2009-2010, [Justin Karneges](https://jblog.andbit.net/) <<justin@karneges.com>>, Psi project
* 2004-2009, [Kevin Smith](https://doomsong.co.uk/) <<kevin@kismith.co.uk>>, Psi project
* 2001-2004, [Justin Karneges](https://jblog.andbit.net/) <<justin@karneges.com>>, Psi project

### Other contributors

There are a lot of people who were involved into Psi and Psi+ development. Some of them are listed in license headers in source files, some of them might be found only in the history of commits in our git repositories. Also there are [translators](https://github.com/psi-plus/psi-plus-l10n/blob/master/AUTHORS), makers of graphics and just active users. We are thankful to all them!

## How you can help

### Bug reports

If you found a bug please report about it in our [Bug Tracker](https://github.com/psi-im/psi/issues). If you have doubts contact with us in [XMPP Conference](https://chatlogs.jabber.ru/psi-dev@conference.jabber.ru) &lt;psi-dev@conference.jabber.ru&gt; (preferable) or in a [Mailing List](https://groups.google.com/forum/#!forum/psi-users).

### Beta testing

As we (intentionally) do not have nor beta versions of Psi, nor daily build builds for it, you are invited to use [Psi+ program](https://psi-plus.com/) for suggesting and testing of new features, and for reporting about new bugs (if they happen).

### Comments and wishes

We like constructive comments and wishes to functions of program. You may contact with us in [XMPP Conference](https://chatlogs.jabber.ru/psi-dev@conference.jabber.ru) &lt;psi-dev@conference.jabber.ru&gt; for discussing of your ideas. Some of them will be drawn up as feature requests in our [Bug Tracker](https://github.com/psi-im/psi/issues).

### Translations

The work of translators is quite routine and boring. People who do it usually lose interests and their translations become incomplete. If you see such situation for translation to your native language, please join to our [translations team](https://www.transifex.com/tehnick/psi-plus/). It is extremely welcome!

### Graphics

There are many ways to contribute to the Psi project, if you think you can do a better job with any of the Psi graphics, then go right ahead!

### Programming

Patches are welcome! Contact to Psi+ team if you are working on them.

### Packaging

If you want to prepare personal builds of Psi and/or Psi+ for MS Windows and macOS systems, it is very welcome! We may distribute them via our official projects on SouceForge.net: [psi](https://sourceforge.net/projects/psi/), [psiplus](https://sourceforge.net/projects/psiplus/). Becoming an official maintaner for these systems is more complicated, but also possible.

For GNU/Linux and *BSD systems the situation is quite clear: just update packages (pkgbuilds, ebuild, etc.) in official repositories of your favorite distributions or make a Personal Package Archive (PPA) with them. We will add links to it into our documentation.

### Donations

If you want to donate some money for development of Psi and Psi+ project, it is possible. See related info at official websites. Thanks!

## Main links

* [Psi](https://github.com/psi-im/psi) (sources)
* [Psi translations](https://github.com/psi-im/psi-l10n) (sources)
* [Officially supported plugins](https://github.com/psi-im/plugins) (sources)
* [Multimedia plugin for audio and video calls](https://github.com/psi-im/psimedia) (sources)
* [Extra resources](https://github.com/psi-im/resources) (iconsets, sounds, skins, themes, etc.)
* [Qt configuration tool](https://github.com/psi-plus/qconf) (sources)
* [Psi+ project](https://psi-plus.com/) (official website)
* [Psi+ snapshots](https://github.com/psi-plus/psi-plus-snapshots) (sources)
* [Psi+ translations](https://github.com/psi-plus/psi-plus-l10n) (sources)

## Packages and installers

* [Official builds for Psi releases](https://sourceforge.net/projects/psi/files/) (executables for Windows and macOS)
* [Psi+ installers for Windows](https://sourceforge.net/projects/psiplus/files/Windows/Personal-Builds/KukuRuzo/)
* [Psi+ portable builds for Windows](https://sourceforge.net/projects/psiplus/files/Windows/Personal-Builds/tehnick/)
* [Psi+ builds for Linux](https://sourceforge.net/projects/psiplus/files/Linux/tehnick/) (AppImage)
* [Psi+ builds for macOS](https://sourceforge.net/projects/psiplus/files/macOS/tehnick/)
* [Psi+ package in Haiku](https://depot.haiku-os.org/psi_plus)
* [Official PPA for Ubuntu and distros based on it](https://launchpad.net/~psi-plus/+archive/ubuntu/ppa) (daily builds)
* [Unofficial PPA for Debian and Ubuntu](http://notesalexp.org/index-old.html) (see [notes](https://psi-plus.com/wiki/en:debian#nightly_builds) about using it)
* [Unofficial PPA for openSUSE](https://software.opensuse.org/package/psi-plus) (daily builds)
* [Unofficial PPA for Fedora](https://copr.fedorainfracloud.org/coprs/valdikss/psi-plus-snapshots/) (outdated)
* [Packages for different Linux distros](https://repology.org/metapackage/psi-plus/versions)

## Extra links

* [Statistics of Psi project on OpenHub](https://www.openhub.net/p/psi)
* [Statistics of Psi+ project on OpenHub](https://www.openhub.net/p/psi-plus)
* [Page on Wikipedia](https://en.wikipedia.org/wiki/Psi_\(instant_messaging_client\))

Have fun!
