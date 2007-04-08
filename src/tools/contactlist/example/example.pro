TEMPLATE = app
TARGET = contactlist

HEADERS += \
	mycontact.h \
	mycontactlist.h \
	mygroup.h \

SOURCES += \
	main.cpp

include(../contactlist.pri)

RESOURCES += resources.qrc
