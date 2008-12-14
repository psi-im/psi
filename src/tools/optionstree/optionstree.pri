INCLUDEPATH += $$PWD
DEPENDPATH  += $$PWD

HEADERS += $$PWD/optionstree.h \
			$$PWD/varianttree.h \
			$$PWD/optionstreereader.h \
			$$PWD/optionstreewriter.h
SOURCES += $$PWD/optionstree.cpp \
			$$PWD/varianttree.cpp \
			$$PWD/optionstreereader.cpp \
			$$PWD/optionstreewriter.cpp

# Model/view classes
HEADERS += $$PWD/optionstreemodel.h 
SOURCES += $$PWD/optionstreemodel.cpp
			
QT += xml
