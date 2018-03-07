# Psi IM — Qt-based XMPP client

Stable version: 1.3 <br>
Release date: September 25th, 2017

Website: https://psi-im.org/ <br>
Sources: https://github.com/psi-im

## License

This program is licensed under the GNU General Public License. See the [COPYING](COPYING) file for more information.

## Description

Psi is a capable XMPP client aimed at experienced users. Its design goals are simplicity and stability. Psi is highly portable and runs on MS Windows, GNU/Linux and macOS.

The interface is very ICQ-like. Alerts appear in the contact list when events are received, and things like subscriptions requests appear as "system messages" (ICQ users know these well). Even "Chats" are queued in the contact list. Also, chats are "remembered" by default, so that you don't have to keep a bunch of windows open for each person. Just close the chat window. If you open it again it will all be there.

Psi is minimal but powerful. There are keybindings for just about everything, Unicode is supported throughout, and contacts are cached offline. Security is also a major consideration, and Psi provides it for both client-to-server (SSL) and client-to-client (GnuPG, OTR, OMEMO).

## Versions history

See [CHANGELOG](CHANGELOG) file.

## Installation

See [INSTALL](INSTALL) file.

## Development

In 2009 a Psi fork named [Psi+](https://psi-plus.com/) was started. Project purposes are: implementation of new features, writing of patches and plugins for transferring them to upstream. As of 2017 all active Psi+ developers have become official Psi developers, but Psi+ still has a number of unique features. From developers point of view Psi+ is just a development branch of Psi IM client which is hosted at separate git repositories and for which rolling release development model is used.

Currently the development model looks like this:

* Psi IM versions should look like: `X.Y` (where `X` and `Y` are not digits but numbers).
* Psi+ versions should look like: `X.Y.Z` (where `X`, `Y` and `Z` are not digits but numbers).
* All development of Psi (bug fixes, new features from Psi+ project) are done in `master` branch of [psi](https://github.com/psi-im/psi) repository and its submodules. Each commit to it increases `Z` part of Psi+ version to one.
* At some point of time new release branch is detouched from `master` branch (for example, `release-1.x`, `release-2.x`, etc.). All new releases (git tags) will be done in it. Necessary changes from `master` branch to stable branches should be cherry-picked or moved manually. Do not forget to [merge release branch](admin/merge_release_to_master.sh) into `master` after adding of new git tags.
* All new features are come into master branch of [main](https://github.com/psi-plus/main) Psi+ repo. Each commit increases `Z` part of Psi+ version to one. Sometime later they will be moved to Psi. It may require different amount of time.
* All patches in [main](https://github.com/psi-plus/main) Psi+ repo should be prepared against `master` branch of [psi](https://github.com/psi-im/psi) repo.
* After each new release of Psi new git tag with the same version should be created in [main](https://github.com/psi-plus/main) Psi+ repo.
* Psi [plugins](https://github.com/psi-im/plugins) are developed separately. Plugin API in `master` branches of [psi](https://github.com/psi-im/psi) and [plugin](https://github.com/psi-im/plugins) repos should be in sync.
* All [translations](https://github.com/psi-plus/psi-plus-l10n) are prepared for `master` branch of Psi+ and are (semi-automatically) [adapted](https://github.com/psi-im/psi-l10n) for Psi.

## Developers

### Lead developers

* 2017-now,  Sergey Ilinykh <<rion4ik@gmail.com>>, Psi IM and Psi+ projects
* 2010-2017, Sergey Ilinykh <<rion4ik@gmail.com>>, Psi+ project
* 2009-2010, [Justin Karneges](http://andbit.net/) <<justin@affinix.com>>, Psi IM project
* 2004-2009, [Kevin Smith](http://doomsong.co.uk/) <<kevin@kismith.co.uk>>, Psi IM project
* 2001-2004, [Justin Karneges](http://andbit.net/) <<justin@affinix.com>>, Psi IM project

### Other contributors

There are a lot of people who were involved into Psi IM and Psi+ development. Some of them are listed in license headers in source files, some of them might be found only in the history of commits in our git repositories. Also there are [translators](https://github.com/psi-plus/psi-plus-l10n/blob/master/AUTHORS), makers of graphics and just active users. We are thankful to all them.

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

Patches are welcome!  Contact to Psi+ team if you are working on them.

### Packaging

If you want to prepare personal builds of Psi IM and/or Psi+ for MS Windows and macOS systems, it is very welcome! We may distribute them via our official projects on SouceForge.net: [psi](https://sourceforge.net/projects/psi/), [psiplus](https://sourceforge.net/projects/psiplus/). Becoming an official maintaner for these systems is more complicated, but also possible.

For GNU/Linux and *BSD systems the situation is quite clear: just update packages (pkgbuilds, ebuild, etc.) in official repositories of your favorite distributions or make a Personal Package Archive (PPA) with them. We will add links to it into our documentation.

### Donations

If you want to donate some money for development of Psi IM and Psi+ project, it is possible. See related info at official websites. Thanks!

## Extra links

* [Psi IM builds for end users](https://sourceforge.net/projects/psi/files/) (executables)
* [Psi IM plugins](https://github.com/psi-im/plugins) (sources)
* [Psi IM plugin for VoIP and video-calls](https://github.com/psi-im/psimedia) (sources)
* [Psi IM translations](https://github.com/psi-im/psi-l10n) (sources)
* [Psi+ project](https://psi-plus.com/) (official website)
* [Psi+ builds for end users](https://sourceforge.net/projects/psiplus/files/) (executables)
* [Psi+ extra resources](https://github.com/psi-plus/resources) (iconsets, sounds, skins, themes, etc.)
* [Psi+ snapshots](https://github.com/psi-plus/psi-plus-snapshots) (sources)
* [Psi+ translations](https://github.com/psi-plus/psi-plus-l10n) (sources)
* [Qt configuration tool](https://github.com/psi-plus/qconf) (sources)

Have fun!

