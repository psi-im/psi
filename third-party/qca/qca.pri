PSIDIR_TP = $$PWD/../../psi/third-party

INCLUDEPATH += $$PSIDIR_TP/qca/qca/include/QtCrypto
LIBS += -L$$PWD -lqca_psi
windows:LIBS += -lcrypt32 
mac:LIBS += -framework Security
