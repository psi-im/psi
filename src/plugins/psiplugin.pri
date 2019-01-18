include($$PWD/pluginsconf.pri)
TEMPLATE = lib
CONFIG += plugin c++14
QT += xml widgets

MOC_DIR = .moc/
OBJECTS_DIR = .obj/
RCC_DIR = .rcc/
UI_DIR = .ui/

INCLUDEPATH += .ui/

target.path = $$psi_plugins_dir
INSTALLS += target

include(plugins.pri)
