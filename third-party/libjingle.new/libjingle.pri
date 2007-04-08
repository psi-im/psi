include(../../conf.pri)

INCLUDEPATH += $$PWD/libjingle
LIBS += -L$$PWD/libjingle -ljingle_psi -ljingle_xmpp_psi
