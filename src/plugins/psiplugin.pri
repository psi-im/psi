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
}

target.path = $$(HOME)/.local/share/Psi/plugins
INSTALLS += target

include(plugins.pri)
