TEMPLATE = lib
CONFIG += plugin
QT += xml

target.path = $$(HOME)/.psi/plugins
INSTALLS += target

PSI_PATH = $$PWD/..


INCLUDEPATH += $$PSI_PATH

HEADERS += $$PSI_PATH/psiplugin.h
