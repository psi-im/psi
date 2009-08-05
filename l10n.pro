TEMPLATE = subdirs
CONFIG += ordered

include(conf.pri)
windows:include(conf_windows.pri)

# configure iris
unix:system("echo \"include(../src/conf_iris.pri)\" > iris/conf.pri")
windows:system("echo include(../src/conf_iris.pri) > iris\\conf.pri")

SUBDIRS += \
        third-party/qca \
        iris \
        src
