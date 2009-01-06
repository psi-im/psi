TEMPLATE = subdirs

include(../libbase.pri)

!irisnetcore_bundle:SUBDIRS += corelib
appledns:!appledns_bundle:SUBDIRS += appledns
!iris_bundle:SUBDIRS += noncore
