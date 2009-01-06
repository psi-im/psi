IRIS_BASE = $$PWD
include(common.pri)

CONFIG *= depend_prl

INCLUDEPATH += $$IRIS_BASE/include $$IRIS_BASE/include/iris $$IRIS_BASE/src

iris_bundle:{
	include(src/xmpp/xmpp.pri)
}
else {
	LIBS += -L$$IRIS_BASE/lib -liris
}

# qt < 4.4 doesn't enable link_prl by default.  we could just enable it,
#   except that in 4.3 or earlier the link_prl feature is too aggressive and
#   pulls in unnecessary deps.  so, for 4.3 and earlier, we'll just explicitly
#   specify the stuff the prl should have given us.
# also, mingw seems to have broken prl support??
win32-g++|contains($$list($$[QT_VERSION]), 4.0.*|4.1.*|4.2.*|4.3.*) {
	DEFINES += IRISNET_STATIC             # from irisnet
	LIBS += -L$$IRIS_BASE/lib -lirisnet   # from iris
	windows:LIBS += -lWs2_32 -lAdvapi32   # from jdns
}
