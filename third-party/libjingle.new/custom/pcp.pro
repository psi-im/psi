TEMPLATE = app
DEFINES += POSIX
CONFIG -= app_bundle

include(../../../../libjingle.pri)
include(../login/login.pri)

# Input
SOURCES += \
	pcp_main.cc
