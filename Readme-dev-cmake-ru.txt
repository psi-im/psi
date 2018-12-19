Описание cmake файлов
(памятка для разработчиков)

./CMakeLists.txt - корневой файл (главный скрипт):
    содержит список основных опций;
    определяет тип программы Psi/Psi+
    определяет Webkit/Webengine
    содержит основные определения (definitions)
    обработка переменных SDK
    определение версии программы
    подключает ccache, mxe
    содержит функцию copy
    подключает каталоги iris, 3rdparty, src
    в случае сборки только плагинов - подключает плагины
    если в корне есть каталог псимедии - собирает псимедию

./3rdparty/CMakeLists.txt - собирает статическую библиотеку qhttp
./3rdparty/qite/libqite/libqite.cmake - содержит список файлов проекта qite
 
./cmake/modules - каталог модулей для поиска библиотек:
./cmake/modulesCOPYING-CMAKE-SCRIPTS - файл лицензии (для решения проблем
    с сопровождением)
./cmake/modules/FindLibGpgError.cmake - ищет libgpg-error
./cmake/modules/FindQJDns.cmake - ищет qjdns
./cmake/modules/FindEnchant.cmake - ищет enchant
./cmake/modules/FindLibOtr.cmake - ищет otr
./cmake/modules/FindQJSON.cmake - ищет qjson (устарело и отключено)
./cmake/modules/FindHunspell.cmake - ищет hunspell
./cmake/modules/FindLibTidy.cmake - ищет tidy или html-tidy
./cmake/modules/FindSparkle.cmake - ищет sparkle (не проверялось)
./cmake/modules/FindIDN.cmake - ищет idn
./cmake/modules/FindMINIZIP.cmake - ищет minizip
./cmake/modules/FindXCB.cmake - ищет xcb
./cmake/modules/FindLibGcrypt.cmake - ищет libgctypt или libgcrypt2
./cmake/modules/FindQca.cmake - ищет qca-qt5
./cmake/modules/FindZLIB.cmake - ищет zlib
./cmake/modules/FindPsiPluginsApi.cmake - модуль поиска файлов, необходимых для сборки плагинов

/***
модули поиска изначально сделаны так, чтобы можно было указать где искать
для этого введены переменные ИМЯ_ROOT где можно указать путь поиска
перед подключением модуля
***/

./iris/CMakeLists.txt - собирает библиотеку iris:
    содержит опции для iris (продублированы в главном скрипте)
    подключает qjdns (если включена опция)
    подключает каталоги src/irisnet и src/xmpp

/***
./iris - каталог содержит дополнительные файлы для сборки библиоетки
    отдельно от проекта Psi. Функционал реализован частично.
    Требуется доработка.
***/

./iris/cmake/modules - модули поиска библиотек для iris (копии модулей 
    из корня)

./iris/src/irisnet/CMakeLists.txt - собирает статическую библиотеку irisnet

./iris/src/xmpp/CMakeLists.txt - собирает статическую библиотеку iris

./src/CMakeLists.txt - собирает и устанавливает проект Psi/Psi+:
    подключает каталог ../translations (если он существует) и собирает
        файлы переводов
    ищет библиотеки и подключает их к проекту
    задает версию программы на основе данных главного скрипта
    генерирует файл config.h на основе файла config.h.in
    генерирует файл psi_win.rc на основе файла ../win32/psi_win.rc.in и
        компилирует psi_win.o
    подключает файл src.cmake
    подключает файл ../3rdparty/qite/libqite/libqite.cmake 
    подключает файл irisprotocol/irisprotocol.cmake
    подключает файл protocol/protocol.cmake
    подключает файл plugins/plugins.cmake
    подключает каталоги AutoUpdater, options, tabs, privacy, Certificates,
        avcall, psimedia, contactmanager, tools, libpsi/dialogs, libpsi/tools,
        widgets, sxe, whiteboarding
    подменяет выходное имя программы на расширенное (если включено)
    если имя расширено, генерирует .desktop файл при помощи модуля generate_desktopfile.cmake
    создает правила для установки
    если включена опция DEV_MODE для win32 генерирует правила установки
        библиотек зависимостей в выходной каталог сборки при помощи модуля win32-prepare-deps.cmake
    подключает каталог plugins, если включена опция и скопированы плагины

./src/generate_desktopfile.cmake - генерирует .desktop файл

./src/config.h.in - файл-шаблон для создания файла config.h

./src/src.cmake:
    содержит определения (definitions) определяющие функционал программы по-умолчанию
    содержит списки основных файлов исходников, необходимых для сборки:
        список FORMS - содержит ui файлы
        список HEADERS - содержит заголовки файлов для которых будут генерироваться .moc файлы
        список SOURCES - содержит исходные файлы для которых будут генерироваться .moc файлы
        список PLAIN_HEADERS - содержит заголовки файлов для которых не будут генерироваться .moc файлы
        список PLAIN_SOURCES - содержит исходные файлы для которых не будут генерироваться .moc файлы
  
./src/win32-prepare-deps.cmake - генерирует список файлов для установки
    командой make prepare-bin-libs, которая установит библиотеки зависимостей
    в выходной каталог сборки. Если доступно использует windeployqt

./src/get-version.cmake - определяет версию клиента по содержимому файла ../version

./src/AutoUpdater/CMakeLists.txt - собирает статическую библиотеку AutoUpdater

./src/avcall/CMakeLists.txt - собирает статическую библиотеку avcall

./src/Certificates/CMakeLists.txt - собирает статическую библиотеку Certificates

./src/contactmanager/CMakeLists.txt - собирает статическую библиотеку contactmanager

./src/irisprotocol/irisprotocol.cmake - список файлов исходников которые подключаются в основные списки сборки

./src/libpsi/dialogs/CMakeLists.txt - собирает статическую библиотеку libpsi_dialogs

./src/libpsi/tools/CMakeLists.txt - собирает статическую библиотеку libpsi_tools:
    подключает каталог zip

./src/libpsi/tools/zip/CMakeLists.txt - собирает статическую библиотеку zip

./src/options/CMakeLists.txt - собирает статическую библиотеку options

./src/privacy/CMakeLists.txt - собирает статическую библиотеку privacy

./src/protocol/protocol.cmake - список файлов исходников которые подключаются в основные списки сборки

./src/psimedia/CMakeLists.txt - собирает статическую библиотеку psimedia

./src/sxe/CMakeLists.txt - собирает статическую библиотеку sxe

./src/tabs/CMakeLists.txt - собирает статическую библиотеку tabs

./src/tools/CMakeLists.txt - собирает статическую библиотеку tools

./src/whiteboarding/CMakeLists.txt - собирает статическую библиотеку whiteboarding

./src/widgets/CMakeLists.txt - собирает статическую библиотеку widgets

./src/plugins/plugins.cmake - список файлов заголовков оторые подключаются в основные списки сборки

./src/plugins/variables.cmake.in - шаблон файла, который содержит основные переменные для сборки плагинов, общие для всех плагинов

./src/plugins/pluginsconf.pri.cmake.in - шаблон файла pluginsconf.pri, генерируемого при сборке

./src/plugins/CMakeLists.txt - основной скрипт управляющий плагинами
    содержит основные правила сборки для всех плагинов
    подключает каталоги generic, unix, dev

./src/plugins/generic/CMakeLists.txt:
    подключает каталоги плагинов, если задана переменная BUILD_PLUGINS, то
    подключает только заданные каталоги плагинов

./src/plugins/dev/CMakeLists.txt:
    подключает каталоги плагинов, если задана переменная BUILD_PLUGINS, то
    подключает только заданные каталоги плагинов

./src/plugins/unix/CMakeLists.txt:
    подключает каталоги плагинов, если задана переменная BUILD_PLUGINS, то
    подключает только заданные каталоги плагинов

./src/plugins/тип/плагин/CMakeLists.txt - собирает и устанавливает плагин
    типа "тип" с именем "плагин". Структура у всех этих скриптов практически
    не отличается

./win32/psi_win.rc.in - файл-шаблон для создания файла psi_win.rc

/***
Плагины могут быть собраны отдельно от основных исходников, но в этом
случае необходимо указать переменную PLUGINS_ROOT_DIR где указать
путь к каталогу src/plugins/include в основных исходниках
***/
