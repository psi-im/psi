IRIS_BASE = $$PWD/..

include(../conf.pri)
windows:include(../conf_win.pri)

include(../common.pri)

QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.3
