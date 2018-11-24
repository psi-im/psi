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

# FIXME next 2 lines will rise compatibility issues
# It's better avoid relying on how Psi was built and use runtime checks
contains(psi_features, qtwebengine):DEFINES+=HAVE_WEBENGINE
contains(psi_features, qtwebkit):DEFINES+=HAVE_WEBKIT

target.path = $$psi_plugins_dir
INSTALLS += target

include(plugins.pri)
