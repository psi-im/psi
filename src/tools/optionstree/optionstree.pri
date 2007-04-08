INCLUDEPATH += $$PWD
DEPENDPATH  += $$PWD

HEADERS += $$PWD/optionstree.h \
			$$PWD/varianttree.h
SOURCES += $$PWD/optionstree.cpp \
			$$PWD/varianttree.cpp

# Model/view classes
HEADERS += $$PWD/optionstreemodel.h 
SOURCES += $$PWD/optionstreemodel.cpp
			
QT += xml
