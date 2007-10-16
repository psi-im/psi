include(../../conf.pri)

INCLUDEPATH += $$PWD/libjingle 
INCLUDEPATH += /sw/include
LIBS += -L$$PWD/libjingle -ljingle_psi -ljingle_xmpp_psi 
LIBS += -L/sw/lib
