TEMPLATE = lib
CONFIG += plugin
QT += xml

target.path = $$(HOME)/.local/share/Psi/plugins
INSTALLS += target

include(plugins.pri)
