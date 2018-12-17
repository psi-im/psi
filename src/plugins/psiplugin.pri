include($$PWD/pluginsconf.pri)
TEMPLATE = lib
CONFIG += plugin c++14
QT += xml widgets
DEFINES += HAVE_QT5

MOC_DIR = .moc/
OBJECTS_DIR = .obj/
RCC_DIR = .rcc/
UI_DIR = .ui/

INCLUDEPATH += .ui/

target.path = $$psi_plugins_dir
INSTALLS += target

include(plugins.pri)
