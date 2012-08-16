#!/bin/sh
set -e

if [ $# != 1 ]; then
	echo "usage: $0 [arch]"
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

build_base=$PWD/admin/build
. $build_base/package_info

target_arch=$1

if [ "$platform" == "mac" ]; then
	target_platform=$target_arch-apple-darwin
	export MACOSX_DEPLOYMENT_TARGET=10.5
else
	if [ "$target_arch" == "x86_64" ]; then
		export CC=/c/mingw64/bin/gcc
		export CXX=/c/mingw64/bin/g++
		export PATH=/c/mingw64/bin:$PATH
	fi
fi

pkgdir=$build_base/packages
patchdir=$build_base/patches

psi_base=$PWD
deps_base=$build_base/deps

get_msys_path() {
	if [ `expr index $1 :` -gt 0 ]; then
		pdrive=`echo $1 | cut -f 1 --delimiter=:`
		prest=`echo $1 | cut -f 2 --delimiter=:`
		echo /$pdrive$prest
	else
		echo $1
	fi
}

old_pwd=$PWD
cd $build_base && make deps
cd $old_pwd

if [ "$platform" == "win" ]; then
	if [ "$target_arch" == "x86_64" ]; then
		qtdir=$QTDIR64
	else
		qtdir=$QTDIR32
	fi
	mqtdir=`get_msys_path $qtdir`

	PATH=$mqtdir/bin:$PATH ./configure.exe --qtdir=$qtdir --release --with-qca-inc=$deps_base/$qca_win_dir/$target_arch/include --with-qca-lib=$deps_base/$qca_win_dir/$target_arch/lib --with-zlib-inc=$deps_base/$zlib_win_dir/$target_arch/include --with-zlib-lib=$deps_base/$zlib_win_dir/$target_arch/lib --with-aspell-inc=$deps_base/$aspell_win_dir/$target_arch/include --with-aspell-lib=$deps_base/$aspell_win_dir/$target_arch/lib

	rm -f $build_base/devenv
	touch $build_base/devenv
	if [ "$target_arch" == "x86_64" ]; then
		echo "export CC=/c/mingw64/bin/gcc" >> $build_base/devenv
		echo "export CXX=/c/mingw64/bin/g++" >> $build_base/devenv
		echo "export PATH=/c/mingw64/bin:\$PATH" >> $build_base/devenv
	fi
	echo "export PATH=$mqtdir/bin:$deps_base/$qca_win_dir/$target_arch/bin:$deps_base/$zlib_win_dir/$target_arch/bin:$deps_base/$aspell_win_dir/$target_arch/bin:$deps_base/$openssl_win_dir/$target_arch/bin:$deps_base/$gstbundle_win_dir/$target_arch/bin:\$PATH" >> $build_base/devenv
	echo "export QT_PLUGIN_PATH=$mqtdir/plugins:$deps_base/$qca_win_dir/$target_arch/plugins" >> $build_base/devenv
	echo "export PSI_MEDIA_PLUGIN=$deps_base/$psimedia_win_dir/$target_arch/plugins/gstprovider.dll" >> $build_base/devenv
else
	if [ "$QT_LIB_PATH" == "" ]; then
		QT_LIB_PATH=$QTDIR/lib
	fi
	if [ "$QT_PLUGIN_PATH" == "" ]; then
		QT_PLUGIN_PATH=$QTDIR/plugins
	fi
	export DYLD_FRAMEWORK_PATH=$QT_LIB_PATH:$deps_base/$qca_mac_dir/lib:$deps_base/$growl_dir/Framework
	./configure --with-qca-inc=$deps_base/$qca_mac_dir/include --with-qca-lib=$deps_base/$qca_mac_dir/lib --with-growl=$deps_base/$growl_dir/Framework --enable-universal

	# remove some gstbundle problem files
	rm -f $deps_base/$gstbundle_mac_dir/uni/lib/gstreamer-0.10/libgstximagesink.so
	rm -f $deps_base/$gstbundle_mac_dir/uni/lib/gstreamer-0.10/libgstxvimagesink.so
	rm -f $deps_base/$gstbundle_mac_dir/uni/lib/gstreamer-0.10/libgstximagesrc.so
	rm -f $deps_base/$gstbundle_mac_dir/uni/lib/gstreamer-0.10/libgstosxaudio.so

	rm -f $build_base/devenv
	touch $build_base/devenv
	echo "export DYLD_LIBRARY_PATH=$deps_base/$gstbundle_mac_dir/uni/lib:\$DYLD_LIBRARY_PATH" >> $build_base/devenv
	echo "export DYLD_FRAMEWORK_PATH=$QT_LIB_PATH:$deps_base/$qca_mac_dir/lib:$deps_base/$growl_dir/Framework:\$DYLD_FRAMEWORK_PATH" >> $build_base/devenv
	echo "export GST_PLUGIN_PATH=$deps_base/$gstbundle_mac_dir/uni/lib/gstreamer-0.10" >> $build_base/devenv
	echo "export GST_REGISTRY_FORK=no" >> $build_base/devenv
	echo "export QT_PLUGIN_PATH=$QT_PLUGIN_PATH:$deps_base/$qca_mac_dir/plugins" >> $build_base/devenv
	echo "export PSI_MEDIA_PLUGIN=$deps_base/$psimedia_mac_dir/plugins/libgstprovider.dylib" >> $build_base/devenv
fi
