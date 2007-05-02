INCLUDEPATH += $$PWD/qca/include/QtCrypto
LIBS += -L$$PWD -lqca_psi
windows:LIBS += -lcrypt32 
mac:LIBS += -framework Security
