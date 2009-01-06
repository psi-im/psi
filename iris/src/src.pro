TEMPLATE = subdirs

include(libbase.pri)

SUBDIRS += irisnet
!iris_bundle:SUBDIRS += xmpp
