#!/bin/sh
set -e

if [ $# != 2 ]; then
	echo "usage: $0 [prefix] [distdir]"
	exit 1
fi

platform=`uname -s`
if [ "$platform" == "Darwin" ]; then
	platform=mac
elif [ "$platform" == "MINGW32_NT-6.1" ]; then
	platform=win
else
	echo "error: unsupported platform $platform"
	exit 1
fi

. ./package_info

destdir=
base_prefix=$1
dist_base=$2

build_base=$PWD
psi_base=$PWD/../..
deps_base=$PWD/deps

mkdir -p $dist_base

# args: path to framework, framework name, framework version
cleanup_framework() {
	# remove dev stuff
	rm -rf $1/Headers
	rm -f $1/${2}_debug
	rm -f $1/${2}_debug.prl
	rm -rf $1/Versions/$3/Headers
	rm -f $1/Versions/$3/${2}_debug
	rm -f $1/Versions/$3/${2}_debug.prl
}

if [ "$platform" == "mac" ]; then
	target_base=$destdir$base_prefix
	target_dist_base=$dist_base

	mkdir -p $target_dist_base

	QT_FRAMEWORKS="QtCore QtNetwork QtXml QtGui"
	QT_PLUGINS="imageformats/libqjpeg.dylib imageformats/libqgif.dylib imageformats/libqmng.dylib"
	QCA_PLUGINS="crypto/libqca-ossl.dylib crypto/libqca-gnupg.dylib"

	cp -a $psi_base/psi.app $target_dist_base/Psi.app
	contentsdir=$target_dist_base/Psi.app/Contents

	for g in $QT_FRAMEWORKS; do
		install_name_tool -change $QTDIR/lib/$g.framework/Versions/4/$g @executable_path/../Frameworks/$g.framework/Versions/4/$g $contentsdir/MacOS/psi
	done

	install_name_tool -change qca.framework/Versions/2/qca @executable_path/../Frameworks/qca.framework/Versions/2/qca $contentsdir/MacOS/psi

	mkdir -p $contentsdir/Frameworks
	for f in $QT_FRAMEWORKS; do
		cp -a $QTDIR/lib/$f.framework $contentsdir/Frameworks
		cleanup_framework $contentsdir/Frameworks/$f.framework $f 4

		install_name_tool -id @executable_path/../Frameworks/$f.framework/Versions/4/$f $contentsdir/Frameworks/$f.framework/$f
		for g in $QT_FRAMEWORKS; do
			install_name_tool -change $QTDIR/lib/$g.framework/Versions/4/$g @executable_path/../Frameworks/$g.framework/Versions/4/$g $contentsdir/Frameworks/$f.framework/$f
		done
	done

	for p in $QT_PLUGINS; do
		mkdir -p $contentsdir/Plugins/$(dirname $p);
		cp -a $QTDIR/plugins/$p $contentsdir/Plugins/$p
		for g in $QT_FRAMEWORKS; do
			install_name_tool -change $QTDIR/lib/$g.framework/Versions/4/$g @executable_path/../Frameworks/$g.framework/Versions/4/$g $contentsdir/Plugins/$p
		done
	done

	cp -a $deps_base/$qca_mac_dir/lib/qca.framework $contentsdir/Frameworks
	cleanup_framework $contentsdir/Frameworks/qca.framework qca 2
	install_name_tool -id @executable_path/../Frameworks/qca.framework/Versions/2/qca $contentsdir/Frameworks/qca.framework/qca
	for g in $QT_FRAMEWORKS; do
		install_name_tool -change $g.framework/Versions/4/$g @executable_path/../Frameworks/$g.framework/Versions/4/$g $contentsdir/Frameworks/qca.framework/qca
	done

	mkdir -p $contentsdir/Plugins/crypto
	for p in $QCA_PLUGINS; do
		cp -a $deps_base/$qca_mac_dir/plugins/$p $contentsdir/Plugins/$p
		for g in $QT_FRAMEWORKS; do
			install_name_tool -change $g.framework/Versions/4/$g @executable_path/../Frameworks/$g.framework/Versions/4/$g $contentsdir/Plugins/$p
		done
		install_name_tool -change qca.framework/Versions/2/qca @executable_path/../Frameworks/qca.framework/Versions/2/qca $contentsdir/Plugins/$p
	done

	cp -a $deps_base/$growl_dir/Framework/Growl.framework $contentsdir/Frameworks
	cleanup_framework $contentsdir/Frameworks/Growl.framework Growl A

	GSTBUNDLE_LIB_FILES=
	for n in `cat $build_base/gstbundle_libs_mac`; do
		for l in `find $deps_base/$gstbundle_mac_dir/uni/lib -maxdepth 1 -type f -name $n`; do
			base_l=`basename $l`
			GSTBUNDLE_LIB_FILES="$GSTBUNDLE_LIB_FILES $base_l"
		done
	done

	GSTBUNDLE_LIB_SUBS=
	for n in `cat $build_base/gstbundle_libs_mac`; do
		for l in `find $deps_base/$gstbundle_mac_dir/uni/lib -maxdepth 1 -name $n`; do
			base_l=`basename $l`
			GSTBUNDLE_LIB_SUBS="$GSTBUNDLE_LIB_SUBS $base_l"
		done
	done

	GSTBUNDLE_LIB_GST_FILES=
	for n in `cat $build_base/gstbundle_gstplugins_mac`; do
		for l in `find $deps_base/$gstbundle_mac_dir/uni/lib/gstreamer-0.10 -type f -name $n`; do
			base_l=`basename $l`
			if [ "$base_l" != "libgstosxaudio.so" ]; then
				GSTBUNDLE_LIB_GST_FILES="$GSTBUNDLE_LIB_GST_FILES $base_l"
			fi
		done
	done

	# subs are files we need to copy, in addition to being what we attempt to substitute via install_name_tool
	for l in $GSTBUNDLE_LIB_SUBS; do
		cp -a $deps_base/$gstbundle_mac_dir/uni/lib/$l $contentsdir/Frameworks
	done

	# now to substitutions on the regular files only
	for l in $GSTBUNDLE_LIB_FILES; do
		for g in $GSTBUNDLE_LIB_SUBS; do
			install_name_tool -change $g @executable_path/../Frameworks/$g $contentsdir/Frameworks/$l
		done
	done

	mkdir -p $contentsdir/Frameworks/gstreamer-0.10
	for p in $GSTBUNDLE_LIB_GST_FILES; do
		cp $deps_base/$gstbundle_mac_dir/uni/lib/gstreamer-0.10/$p $contentsdir/Frameworks/gstreamer-0.10
		for g in $GSTBUNDLE_LIB_SUBS; do
			install_name_tool -change $g @executable_path/../Frameworks/$g $contentsdir/Frameworks/gstreamer-0.10/$p
		done
	done

	cp $deps_base/$psimedia_mac_dir/plugins/libgstprovider.dylib $contentsdir/Plugins
	for g in $GSTBUNDLE_LIB_SUBS; do
		install_name_tool -change $g @executable_path/../Frameworks/$g $contentsdir/Plugins/libgstprovider.dylib
	done
	for g in $QT_FRAMEWORKS; do
		install_name_tool -change $g.framework/Versions/4/$g @executable_path/../Frameworks/$g.framework/Versions/4/$g $contentsdir/Plugins/libgstprovider.dylib
	done
else
	target_base=$destdir$base_prefix
	target_dist_base=$dist_base

	mkdir -p $target_dist_base
	cp -a $target_base/* $target_dist_base
fi
