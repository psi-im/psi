# Как собрать Psi используя cmake

## Подготовка каталога сборки:

```sh
mkdir buildir && cd buildir
cmake FLAGS ..
```

  Тут вместо флага `FLAGS` нужно ставить флаги из секции "Полезные флаги CMAKE".
  Можно вообще не ставить никаких флагов.

## Сборка:

```sh
cmake --build . --target all --
```

или

```sh
make
```

## Установка Psi:

```sh
cmake --build . --target install --
```

или

```sh
make install
```

## Если в установке нет необходимости, для запуска Psi:

```sh
cmake --build . --target prepare-bin
```

или

```sh
make prepare-bin
cd psi && ./psi && cd .. #для Psi
cd psi && ./psi-plus && cd .. #для Psi+
```

## Полезные флаги CMAKE:

> -DPSI_PLUS=ON

  Компилировать Psi+ вместо Psi.
  (по-умолчанию `OFF`)

> -DPSI_LIBDIR=${path}

  Путь к каталогу библиотек Psi. Путь по которому
  Psi будет искать плагины.

> -DPSI_DATADIR=${path}

  Путь к каталогу данных программы Psi. Путь по
  которому Psi будет искать данные (иконпаки, темы и т.д.).

> -DCMAKE_INSTALL_PREFIX=prefix

  Задать префикс (каталог установки).

> -DBUNDLED_IRIS=ON

  Использовать встроенную библиотеку `Iris`.
  (по-умолчанию `ON`)

> -DBUNDLED_IRIS_ALL=ON

  Использовать встроенную библиотеку `iris` со встроенной `QCA` и `USRSCTP`.
  (по-умолчанию `OFF`)

> -DIRIS_BUNDLED_QCA=ON

  Компилировать библиотеку `QCA` из репозитория https://github.com/psi-im/qca вместе с Psi.
  Добавляет: `DTLS`, `Blake2b` и другие полезные механизмы шифрования XMPP.
  (по-умолчанию `OFF`)

> -DJINGLE_SCTP=ON

  Включает `SCTP` поверх транспорта `ICE Jingle` / каналы данных.
  Если `ON` - добавляет опцию `BUNDLED_USRSCTP`.
  (по-умолчанию `ON`)

> -DIRIS_BUNDLED_USRSCTP=ON

  Компилировать библиотеку `usrsctp` (необходима для p2p отправки файлов) из официального репозитория GitHub.
  Полезно, если в системе нет такой библиотеки или есть, но несовместимая с Psi.
  Доступна только при включенной опции `JINGLE_SCTP`.
  (по-умолчанию `OFF`)

> -DBUNDLED_KEYCHAIN=ON

  Компилировать библиотеку `QtKeychain` из официального репозитория GitHub.
  Полезно для сборки под macOS.
  (по-умолчанию `OFF`)

> -DUSE_ASPELL=OFF

  Использовать проверку орфографии `Aspell`.
  (по-умолчанию `OFF`)

> -DUSE_ENCHANT=ON

  Использовать проверку орфографии `Enchant`.
  (по-умолчанию `OFF`)

> -DUSE_HUNSPELL=ON

  Использовать проверку орфографии `Hunspell`.
  (по-умолчанию `ON`)

> -DCHAT_TYPE=BASIC

  Выбрать тип движка истории бесед. Возможные значения: `WEBKIT`, `WEBENGINE`, `BASIC`.
  Значение по-умолчанию: `BASIC`.

> -DPSI_VERSION=${version}

  Задать версию Psi вручную.
  Пример для Psi+: `1.0.40 (2017-06-05, Psi:a7d2d7b8, Psi+:055e945, webkit)`.
  Данный флаг ставить не обязательно, т.к. скрипт автоматически определяет версию по содержимому файла `version`

> -DCMAKE_BUILD_TYPE=Release

  Задать тип сборки. Возможные значения: `DEBUG`, `RELEASE`, `RELWITHDEBINFO`, `MINSIZEREL`.
  (по-умолчанию `Release`)

> -USE_CCACHE=ON

  Использовать утилиту `ccache` при сборке.
  (по-умолчанию `ON`)

> -DUSE_MXE=ON

  Включает поддержку `MXE` ("M cross environment").
  Выключает флаг `USE_CCACHE` и добавляет новые зависимости, если флаг `PRODUCTION` включен.
  Данный флаг ставить не обязательно, т.к. скрипт умеет определять `MXE` автоматически.
  (по-умолчанию `OFF`)

> -DVERBOSE_PROGRAM_NAME=ON

  Использовать развернутое имя для бинарного файла.
  Экспериментальный флаг. После компиляции будет создан бинарный файл не с
  именем `psi` или `psi-plus`, а например `psi-webkit` или `psi-plus-webengine`.
  (по-умолчанию `OFF`)

> -DPRODUCTION=ON

  Собрать релизную версию Psi/Psi+.
  (по-умолчанию для Psi - `ON`, для Psi+ - `OFF`)

> -DUSE_KEYCHAIN=ON

  Собрать с поддержкой `Qt5Keychain`.

> -DBUILD_PSIMEDIA=ON

Собрать вместе с плагином `psimedia` если исходники найдены в папке проекта.

> -DONLY_BINARY=OFF

  Если `ON` - устанавливается только бинарный файл.

> -DINSTALL_EXTRA_FILES=ON

  Если `OFF`, то звуки, сертификаты, иконки, темы и файл `client_icons.txt`
  установлены не будут.

> -DINSTALL_PLUGINS_SDK=ON

  Если флаг `ON`, то вместе с Psi ставятся файлы PluginsAPI
  необходимые для сборки плагинов отдельно от исходников главной программы.
  Возможно будет полезно для сопровождающих пакетов.
  (по-умолчанию `OFF`)

> -DENABLE_PLUGINS=ON

  Включить сборку плагинов.
  Если плагинов в каталоге `plugins` нет, скрипт упадёт с ошибкой.
  (по-умолчанию `OFF`)

> -DONLY_PLUGINS=ON

  Собирать только плагины не собирая саму Psi.
  Включив этот флаг, флаг `ENABLE_PLUGINS` включается автоматически.
  (по-умолчанию `OFF`)

> -DDEV_MODE=ON

  Полезно для отладки плагинов.
  В Windows включает цель сборки `prepare-bin-libs`.
  Этот флаг удобен для запуска Psi сразу после сборки при разработке.
  При включении этого флага скрипт ищет библиотеки зависимостей и по команде:
  `make prepare-bin-libs` копирует их в каталог сборки.
  
  В Linux включает режим разработчика и вместе с флагом `ENABLE_PLUGINS`.
  При использовании `psi-plus-snapshots` позволяет отлаживать плагины без установки Psi.
  Устанавливает каталог `PSI_DATA` в текущий каталог исполняемых файлов.

> -DUSE_XSS=ON

  В Linux добавляет поддержку `XScreensaver`.
  (по-умолчанию ON).

> -DUSE_DBUS=ON

  В Linux включает поддержку `DBus` для управления клиентом, уведомлений, тюнов.
  (по-умолчанию `ON`).

> -DUSE_X11=ON

  Включить поддержку функций `X11`.
  (по-умолчанию `ON`)

> -DLIMIT_X11_USAGE=ON

  Отключает поддержку функций `X11` которые могут приводить к падению программы.
  (по-умолчанию `OFF`)

> -DUSE_TASKBARNOTIFIER=ON

  Показывает количество входящих событий на значке программы.
  Для систем Linux используется `DBus` служба `com.canonical.Unity`, если доступна.
  На Windows используется механизм оверлея значков.
  Или просто меняет иконку программы в других случаях.
  (по-умолчанию `ON`)

## Работа с плагинами:

### Следующие флаги работают только если включены флаги ENABLE_PLUGINS или ONLY_PLUGINS

> -DBUILD_PLUGINS=${plugins}

  Задать список плагинов для сборки. Чтобы собрать все плагины можно задать `-DBUILD_PLUGINS="ALL"` или вообще не ставить этот флаг.

  Возможные значения для `${plugins}` (можно определить по содержимому каталога [plugins/generic](https://github.com/psi-im/plugins/tree/master/generic)):
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

  Пример:

  > -DBUILD_PLUGINS="chessplugin;otrplugin"

  Переменная `BUILD_PLUGINS` может также быть использована как чёрный список.
  В этом случае будут собраны все плагины, кроме указанных.
  Для этого достаточно указать переменную как:

  > -DBUILD_PLUGINS="-chessplugin;-otrplugin"

  и плагины `chessplugin` и `otrplugin` собраны не будут.

  **ВНИМАНИЕ!** Смешивание белого и черного списков не допускается.

> -DPLUGINS_ROOT_DIR=${path}

  Путь к каталогу include для сборки плагинов в отрыве от исходников
  Psi (каталог где лежит файл `plugins.cmake`)

> -DPLUGINS_PATH=${path}

  Установка плагинов в каталог с суффиксом `${path}`. Для установки по-умолчанию:

  `-DPLUGINS_PATH=lib/psi-plus/plugins` или не задавать этот флаг.

  Например, для установки плагинов в `~/.local/share/psi+/plugins`:

  > -DCMAKE_INSTALL_PREFIX=$HOME/.local -DPLUGINS_PATH=share/psi+/plugins

  Например, для установки плагинов в `/usr/share/psi-plus/plugins`:

  > -DCMAKE_INSTALL_PREFIX=/usr -DPLUGINS_PATH=share/psi-plus/plugins

## Секция Win32 или MXE:

Для сборки под ОС Windows установка программы не требуется!!!

> -DENABLE_PORTABLE=ON

  Для сборки портативной версии (сделано для удобства, чтобы не переименовывать бинарник вручную).
  На выходе получим "имя-бинарника-portable.exe", а если `VERBOSE_PROGRAM_NAME=ON`,
  то "расширенное-имя-бинарника-portable.exe".
  При включении этого флага автоматически становится доступна цель сборки `prepare-bin-libs`.
  (по-умолчанию - `OFF`).

> -DQCA_DIR=DIRECTORY

  Задать корневой каталог с библиотекой `Qca`.

> -DZLIB_ROOT=DIRECTORY

  Задать корневой каталог с библиотекой `Zlib`.

> -DHUNSPELL_ROOT=DIRECTORY

  Задать корневой каталог с библиотекой `Hunspell`.

> -DNO_DEBUG_OPTIMIZATION=OFF

  Отключить оптимизации компилятора при сборке. Только для Windows.
  (по-умолчанию `OFF`)

### Для сборки плагина OTR в OS WINDOWS возможно понадобятся дополнительные флаги:

> -DLIBGCRYPT_ROOT=%LIBGCRYPT_ROOT%

  Задать корневой каталог с библиотекой `LIBGCRYPT`.

> -DLIBGPGERROR_ROOT=%LIBGPGERROR_ROOT%

  Задать корневой каталог с библиотекой `LIBGPG-ERROR`.

> -DLIBOTR_ROOT=%LIBOTR_ROOT%

  Задать корневой каталог с библиотекой `LIBOTR`.

> -DLIBTIDY_ROOT=%LIBTIDY_ROOT%

  Задать корневой каталог с библиотекой `LIBTIDY`.

  Например:

  `-DLIBGCRYPT_ROOT=C:\libgcrypt -DLIBGPGERROR_ROOT=C:\libgpg-error -DLIBOTR_ROOT=C:\libotr -DLIBTIDY_ROOT=C:\libtidy`

> -DNO_DEBUG_OPTIMIZATION=OFF

  Отключить оптимизации компилятора при сборке дебаг-версии.
  (по-умолчанию `OFF`)


### Если при сборке Psi используется SDK, нужно задать путь SDK_PATH:

> -DSDK_PATH=path

  Если задать этот флаг, то флаги к корневым каталогам библиотек
  зависимостей можно не задавать.

### macOS specific flags

> -DUSE_SPARKLE=ON

  Использовать Sparkle для сборок macOS.
  (по-умолчанию `ON`)

> -DUSE_GROWL=OFF

  Использовать growl для сборок macOS.
  (по-умолчанию `OFF`)

> -DUSE_MAC_DOC=OFF

  Использовать док macOS.
  (по-умолчанию `OFF`)
