TEMPLATE = app
DEFINES += POSIX
CONFIG -= app_bundle

include(../../../../libjingle.pri)
include(login.pri)

# Input
SOURCES += \
	login_main.cc
