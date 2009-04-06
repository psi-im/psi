TEMPLATE = lib
CONFIG += plugin
QT += xml

target.path = $$(HOME)/.psi/plugins
INSTALLS += target

include(plugins.pri)
