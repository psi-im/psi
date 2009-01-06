load(winlocal.prf)

# qca
CONFIG += crypto

# zlib
INCLUDEPATH += $$WINLOCAL_PREFIX/include
LIBS += -L$$WINLOCAL_PREFIX/lib

# zlib may have a different lib name depending on how it was compiled
win32-g++ {
	LIBS += -lz
}
else {
	LIBS += -lzlib   # static
	#LIBS += -lzdll  # dll
}
