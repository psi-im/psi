include($$PWD/pluginsconf.pri)
TEMPLATE = lib
CONFIG += plugin
QT += xml

MOC_DIR = .moc/
OBJECTS_DIR = .obj/
RCC_DIR = .rcc/
UI_DIR = .ui/

INCLUDEPATH += .ui/

greaterThan(QT_MAJOR_VERSION, 4) {
  QT += widgets
  DEFINES += HAVE_QT5
  CONFIG += c++11
}

qtwebengine:DEFINES+=HAVE_WEBENGINE
qtwebkit:DEFINES+=HAVE_WEBKIT

target.path = $$psi_plugins_dir
INSTALLS += target

include(plugins.pri)
