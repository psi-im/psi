# common stuff for iris.pro and iris.pri

# FIXME: Remove this
DEFINES += IRIS_XMPP_JID_DEPRECATED

# default build configuration
!iris_build_pri {
	# build appledns on mac
	mac:CONFIG += appledns

	# bundle appledns inside of irisnetcore on mac
	mac:CONFIG += appledns_bundle

	# bundle irisnetcore inside of iris
	CONFIG += irisnetcore_bundle

	# don't build iris, app will include iris.pri
	#CONFIG += iris_bundle
}
