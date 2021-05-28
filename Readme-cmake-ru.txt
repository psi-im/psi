# Как собрать Psi/Psi+ используя cmake

## Подготовка каталога сборки:

> $ mkdir build && cd build
> $ cmake FLAGS ..

  вместо флага FLAGS нужно ставить флаги из секции "Полезные CMAKE флаги"
  можно вообще не ставить никаких флагов

## Сборка:

> $ cmake --build . --target all --

или

> $ make

## Установка:

> $ cmake --build . --target install --

или

> $ make install

## Если в установке нет необходимости, для запуска Psi/Psi+:

> $ make prepare-bin
> $ cd psi && ./psi && cd .. #для Psi
> $ cd psi && ./psi-plus && cd .. #для Psi+

## Полезные CMAKE флаги:

> -PSI_PLUS=OFF

  Компилировать Psi+ вместо Psi (по-умолчанию OFF)

> -DPSI_LIBDIR=${path}

  Путь к каталогу библиотек Psi/Psi+. Путь по которому
  Psi/Psi+ будет искать плагины

> -DPSI_DATADIR=${path}

  Путь к каталогу данных программы Psi/Psi+. Путь по
  которому Psi/Psi+ будет искать данные (иконпаки, темы и.т.д.)

> -DCMAKE_INSTALL_PREFIX=prefix

  задать префикс (каталог установки)

> -DBUNDLED_IRIS=ON

  использовать встроенную библиотеку iris (по-умолчанию - ON)
  функционал внешней библиотеки пока-что не реализован

> -DBUNDLED_QCA=ON

  компилировать библиотеку qca из репозитория https://github.com/psi-im/qca
  вместе с Psi. Добавляет: DTLS, Blake2b и другие полезные механизны шифрования XMPP
  (по-умолчанию -OFF)

> -DJINGLE_SCTP=ON

  включает SCTP поверх транспорта ICE Jingle / каналы данных
  если ON - добавляет опцию BUNDLED_USRSCTP (по-умолчанию -ON)

> -DBUNDLED_USRSCTP=ON

  компилировать библиотеку usrsctp из официального репозитория github
  Полезно, если в системе нет такой библиотеки или есть, но несовместимая с Psi
  (необходима для p2p отправки файлов и доступна только при включенной опции
  JINGLE_SCTP)
  (по-умолчанию -OFF)

> -DUSE_ASPELL=OFF

  использовать механизм проверки орфографии Aspell (по-умолчанию -OFF)

> -DUSE_ENCHANT=ON

  использовать механизм проверки орфографии Enchant (по-умолчанию -OFF)

> -DUSE_HUNSPELL=ON

  использовать механизм проверки орфографии Hunspell (по-умолчанию - ON)

> -DCHAT_TYPE=BASIC

  выбрать тип движка чатлогов. Возможные значения: WEBKIT, WEBENGINE, BASIC
  значение по-умолчанию - BASIC.

> -DPSI_VERSION=${version}

  задать версию Psi/Psi+ вручную
  ( Пример для Psi+: 1.0.40 (2017-06-05, Psi:a7d2d7b8, Psi+:055e945, webkit) ).
  Данный флаг ставить не обязательно, т.к. скрипт автоматически
  определяет версию по содержимому файла "version"

> -DCMAKE_BUILD_TYPE=Release

  задать тип сборки. Возможные значения: DEBUG, RELEASE, RELWITHDEBINFO,
  MINSIZEREL  (по-умолчанию - Release)

> -USE_CCACHE=ON (по-умолчанию - ON)

  использовать утилиту ccache при сборке

>  -DUSE_MXE=ON (по-умолчанию - OFF)

  Включает поддержку MXE (M cross environment). Выключает флаг
  USE_CCACHE и добавляет новые зависимости, если флаг PRODUCTION включен.
  Данный флаг ставить не обязательно, т.к. скрипт умеет определять MXE
  автоматически

> -DVERBOSE_PROGRAM_NAME=ON

  использовать развернутое имя для бинарного файла (Экспериментальный флаг)
  (по-умолчанию -OFF). После компиляции будет создан бинарный файл не с
  именем psi или psi-plus, а например psi-webkit или psi-plus-webengine

> -DPRODUCTION=ON (по-умолчанию для Psi - ON, для Psi+ - OFF)

  собрать релизную версию Psi/Psi+

> -DUSE_KEYCHAIN=ON

  собрать с поддержкой Qt5Keychain

> -DBUILD_PSIMEDIA=ON

  собрать вместе с плагином psimedia если подготовлены исходники

> -DONLY_BINARY=OFF

  Если ON - устанавливается только бинарный файл

> -DINSTALL_EXTRA_FILES=ON

  Если OFF, то звуки, сертификаты, иконки, темы и файл cilent_icons.txt
  установлены не будут

> -DINSTALL_PLUGINS_SDK=ON

  Если флаг включен, то вместе с пси ставятся файлы необходимые для
  сборки плагинов (возможно будет полезно для сопровождающих пакеты)
  по-умолчанию - OFF

> -DENABLE_PLUGINS=ON

  включить сборку плагинов (по-умолчанию -OFF)
  если плагинов в каталоге plugins нет, скрипт упадёт с ошибкой

> -DONLY_PLUGINS=ON

  собирать только плагины не собирая саму Psi/Psi+ (по-умолчанию -OFF).
  Включив этот флаг, флаг ENABLE_PLUGINS включается автоматически

> -DDEV_MODE=ON

  В OS Windows включает цель сборки prepare-bin-libs.
  Этот флаг удобен для запуска Psi/Psi+ сразу после сборки при разработке.
  При включении этого флага скрипт ищет библиотеки зависимостей и по команде:
  > $ make prepare-bin-libs
  копирует их в каталог сборки.
  В OS Linux включает режим разработчика и вместе с флагом ENABLE_PLUGINS
  при использовании psi-plus-snapshots позовяет отлаживать плагины без
  установки Psi

> -DUSE_XSS=ON

  В OS Linux добавляет поддержку XScreensaver (по-умолчанию ON).

> -DUSE_DBUS=ON

  В OS Linux включает поддержку DBus для управления клиентом, уведомлений, тюнов (по-умолчанию ON).

> -DUSE_X11=ON

  включить поддержку функций X11 (по-умолчанию ON)

> -DLIMIT_X11_USAGE=ON

  отключает поддержку функций X11 которые могур приводить к падению программы
  (по-умолчанию OFF)

## Работа с плагинами:

### Следующие флаги работают только если включены флаги ENABLE_PLUGINS или ONLY_PLUGINS

> -DBUILD_PLUGINS=${plugins}

  задать список плагинов для сборки. Чтобы собрать все плагины можно задать -DBUILD_PLUGINS="ALL" или вообще не ставить этот флаг

  - возможные значения для ${plugins} (можно определить по содержимому каталога plugins/generic):

    historykeeperplugin    stopspamplugin juickplugin translateplugin gomokugameplugin attentionplugin
    cleanerplugin autoreplyplugin contentdownloaderplugin qipxstatusesplugin skinsplugin
    clientswitcherplugin watcherplugin videostatusplugin screenshotplugin jabberdiskplugin
    storagenotesplugin    extendedoptionsplugin imageplugin extendedmenuplugin birthdayreminderplugin
    pepchangenotifyplugin omemoplugin openpgpplugin otrplugin chessplugin conferenceloggerplugin
    enummessagesplugin httpuploadplugin imagepreviewplugin

  Пример:

  > -DBUILD_PLUGINS="chessplugin;otrplugin"

    Переменная BUILD_PLUGINS может также быть использована как черный список.
    В этом случае будут собраны все плагины, кроме указаных.
    Для этого достаточно указать переменную как

    > -DBUILD_PLUGINS="-chessplugin;-otrplugin"

    и плагины chessplugin и otrplugin собраны не будут

    ВНИМАНИЕ! Смешивание белого и черного списков не допускается.


> -DPLUGINS_ROOT_DIR=${path}

  Путь к каталогу include для сборки плагинов в отрыве от исходников
  Psi/Psi+ (каталог где лежит файл plugins.cmake)

> -DPLUGINS_PATH=${path}

  установка плагинов в каталог с суфииксом ${path}. Для установки по-умолчанию:

  -DPLUGINS_PATH=lib/psi-plus/plugins или не задавать этот флаг

  Например для установки плагинов в ~/.local/share/psi+/plugins:

  > -DCMAKE_INSTALL_PREFIX=$HOME/.local -DPLUGINS_PATH=share/psi+/plugins

  Напирмер для установки плагинов в /usr/share/psi-plus/plugins:

  > -DCMAKE_INSTALL_PREFIX=/usr -DPLUGINS_PATH=share/psi-plus/plugins

## Секция Win32 или MXE:

Для сборки под ОС Windows установка программы не требуется!!!

> -DENABLE_PORTABLE=ON

  для сборки портативной версии (сделано для удобства, чтобы не
  переименовывать бинарник вручную). (по-умолчанию - OFF). На выходе
  получим "имя-бинарника-portable.exe", а если VERBOSE_PROGRAM_NAME=ON,
  то "расширенное-имя-бинарника-portable.exe". При включении этого флага
  автоматически становится доступна цель сборки prepare-bin-libs.

> -DQCA_DIR=DIRECTORY

  задать корневой каталог с библиотекой Qca

> -DZLIB_ROOT=DIRECTORY

  задать корневой каталог с библиотекой Zlib

> -DHUNSPELL_ROOT=DIRECTORY

  задать корневой каталог с библиотекой Hunspell

### Для сборки плагина OTR в OS WINDOWS возможно понадобятся дополнительные флаги:

> -DLIBGCRYPT_ROOT=%LIBGCRYPT_ROOT%

  задать корневой каталог с библиотекой LIBGCRYPT

> -DLIBGPGERROR_ROOT=%LIBGPGERROR_ROOT%

  задать корневой каталог с библиотекой LIBGPG-ERROR

> -DLIBOTR_ROOT=%LIBOTR_ROOT%

  задать корневой каталог с библиотекой LIBOTR

> -DLIBTIDY_ROOT=%LIBTIDY_ROOT%

  задать корневой каталог с библиотекой LIBTIDY

  Например:

  > -DLIBGCRYPT_ROOT=C:\libgcrypt -DLIBGPGERROR_ROOT=C:\libgpg-error -DLIBOTR_ROOT=C:\libotr -DLIBTIDY_ROOT=C:\libtidy

> -DNO_DEBUG_OPTIMIZATION=OFF

  Отключить оптимизации компилятора при сборке дебаг-версии (по-умолчанию OFF)


### Если при сборке Psi/Psi+ используется SDK, нужно задать путь SDK_PATH:

> -DSDK_PATH=path

  Если задать этот флаг, то флаги к корневым каталогам библиотек
  зависимостей можно не задавать.

### macOS specific flags

> -DUSE_SPARKLE=ON

  использовать Sparkle для сборок macOS (по-умолчанию ON)

> -DUSE_GROWL=OFF

  использовать growl для сборок macOS (по-умолчанию OFF)

> -DUSE_MAC_DOC=OFF

  использовать док macOS (по-умолчанию OFF)
