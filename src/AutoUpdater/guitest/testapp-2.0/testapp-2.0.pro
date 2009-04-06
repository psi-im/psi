TEMPLATE = app
TARGET = testapp
SOURCES = main.cpp
QMAKE_POST_LINK = zip -r testapp-2.0.zip testapp.app
