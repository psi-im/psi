PMD=$$PWD/../psimedia

HEADERS += $$PMD/psimedia.h $$PMD/psimediaprovider.h
SOURCES += $$PMD/psimedia.cpp

DEFINES += GSTPROVIDER_STATIC

# gstreamer stuff
DEFINES += QT_STATICPLUGIN
HEADERS += $$PMD/gstcustomelements.h
SOURCES += \
	$$PMD/gstcustomelements.c \
	$$PMD/gstcustomelements_appvideosink.c \
	$$PMD/gstcustomelements_apprtpsrc.c \
	$$PMD/gstcustomelements_apprtpsink.c \
	$$PMD/gstprovider.cpp

windows {
	INCLUDEPATH += \
		"C:\Program Files\Common Files\gtk+\include\glib-2.0" \
		"C:\Program Files\Common Files\gtk+\lib\glib-2.0\include" \
		"C:\Program Files\Common Files\libxml2\include" \
		"C:\Program Files\Common Files\GStreamer\0.10\include" \
		"C:\Program Files\Common Files\iconv\include" \
		"C:\Program Files\Common Files\zlib\include"
	LIBPATH += \
		"C:\Progra~1\Common~1\gtk+\lib" \
		"C:\Progra~1\Common~1\GStreamer\0.10\lib"
	LIBS += -llibgstreamer-0.10 -lgthread-2.0 -lglib-2.0 -lgobject-2.0 -llibgstvideo-0.10 -llibgstbase-0.10
}

unix:!mac {
	INCLUDEPATH += \
		/usr/include/glib-2.0 \
		/usr/lib/glib-2.0/include \
		/usr/include/libxml2 \
		/usr/include/gstreamer-0.10
	LIBS += -lgstreamer-0.10 -lgthread-2.0 -lglib-2.0 -lrt -lgstvideo-0.10
}

mac {
	INCLUDEPATH += \
		/opt/local/include/glib-2.0 \
		/opt/local/lib/glib-2.0/include \
		/opt/local/include/libxml2 \
		/opt/local/include/gstreamer-0.10
	LIBS += -L/opt/local/lib -lgstreamer-0.10 -lgthread-2.0 -lglib-2.0 -lgobject-2.0 -lgstvideo-0.10 -lgstbase-0.10
}

# device enum
windows: {
        SOURCES += $$PMD/deviceenum_win.cpp
}
unix:!mac: {
        SOURCES += $$PMD/deviceenum_unix.cpp
}
mac: {
        SOURCES += $$PMD/deviceenum_mac.cpp
        LIBS += -framework CoreAudio
}
