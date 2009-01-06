INCLUDEPATH += $$PWD/..
DEPENDPATH  += $$PWD/..

unix:{
  QMAKE_CFLAGS_WARN_ON -= -W
}
win32:{
  QMAKE_CFLAGS += -Zm400
}

SOURCES += \
  $$PWD/profiles.c \
  #$$PWD/toutf8.c \
  $$PWD/rfc3454.c \
  $$PWD/nfkc.c \
  $$PWD/stringprep.c
