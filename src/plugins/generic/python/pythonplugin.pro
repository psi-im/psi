include(../../plugins.pri)

PYTHON_PATH = /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/Python.framework/Versions/2.3/include/python2.3/

LIBS += -lpython

INCLUDEPATH += $$PYTHON_PATH

SOURCES += pythonplugin.cpp
