#!/bin/sh
set -e

if [ $# != 3 ]; then
	echo "usage: $0 [package] [arch] [prefix]"
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

package_name=$1
target_arch=$2
base_prefix=$3

if [ "$platform" == "mac" ]; then
	target_platform=$target_arch-apple-darwin
	export MACOSX_DEPLOYMENT_TARGET=10.5
else
	if [ "$target_arch" == "x86_64" ]; then
		export PATH=/c/mingw64/bin:$PATH
	fi
fi

arch_prefix=$base_prefix/$target_arch
pkgdir=$PWD/packages
patchdir=$PWD/patches

build_base=$PWD
psi_base=$PWD/../..
deps_base=$PWD/deps

get_msys_path() {
	if [ `expr index $1 :` -gt 0 ]; then
		pdrive=`echo $1 | cut -f 1 --delimiter=:`
		prest=`echo $1 | cut -f 2 --delimiter=:`
		echo /$pdrive$prest
	else
		echo $1
	fi
}

build_package() {
	if [ ! -d "build/$2/$1" ]; then
		echo "$1/$2: building..."
		mkdir -p build/$2/$1
		old_pwd=$PWD
		cd build/$2/$1
		build_package_$1
		cd $old_pwd
	else
		echo "$1/$2: failed on previous run. remove the \"build/$2/$1\" directory to try again"
		exit 1
	fi
}

build_package_psi() {
	if [ "$platform" == "win" ]; then
		if [ "$target_arch" == "x86_64" ]; then
			qtdir=$QTDIR64
		else
			qtdir=$QTDIR32
		fi
		mqtdir=`get_msys_path $qtdir`

		cd $psi_base
		PATH=$mqtdir/bin:$PATH ./configure.exe --qtdir=$qtdir --release --with-qca-inc=$deps_base/$qca_win_dir/$target_arch/include --with-qca-lib=$deps_base/$qca_win_dir/$target_arch/lib --with-zlib-inc=$deps_base/$zlib_win_dir/$target_arch/include --with-zlib-lib=$deps_base/$zlib_win_dir/$target_arch/lib --with-aspell-inc=$deps_base/$aspell_win_dir/$target_arch/include --with-aspell-lib=$deps_base/$aspell_win_dir/$target_arch/lib
		mingw32-make

		mkdir -p $arch_prefix
		cp psi.exe $arch_prefix/Psi.exe
		cp $mqtdir/bin/QtCore4.dll $arch_prefix
		cp $mqtdir/bin/QtNetwork4.dll $arch_prefix
		cp $mqtdir/bin/QtXml4.dll $arch_prefix
		cp $mqtdir/bin/QtGui4.dll $arch_prefix
		mkdir -p $arch_prefix/imageformats
		cp $mqtdir/plugins/imageformats/qgif4.dll $arch_prefix/imageformats
		cp $mqtdir/plugins/imageformats/qjpeg4.dll $arch_prefix/imageformats
		cp $mqtdir/plugins/imageformats/qmng4.dll $arch_prefix/imageformats
		cp $deps_base/$qca_win_dir/$target_arch/bin/qca2.dll $arch_prefix
		mkdir -p $arch_prefix/crypto
		cp $deps_base/$qca_win_dir/$target_arch/plugins/crypto/qca-gnupg2.dll $arch_prefix/crypto
		cp $deps_base/$qca_win_dir/$target_arch/plugins/crypto/qca-ossl2.dll $arch_prefix/crypto
		cp $deps_base/$zlib_win_dir/$target_arch/bin/zlib1.dll $arch_prefix
		cp $deps_base/$aspell_win_dir/$target_arch/bin/libaspell-15.dll $arch_prefix
		cp -a $deps_base/$aspell_win_dir/$target_arch/lib/aspell-0.60 $arch_prefix/aspell
		cp $deps_base/$openssl_win_dir/$target_arch/bin/libeay32.dll $arch_prefix
		cp $deps_base/$openssl_win_dir/$target_arch/bin/ssleay32.dll $arch_prefix
		for n in `cat $build_base/gstbundle_libs_win`; do
			cp -a $deps_base/$gstbundle_win_dir/$target_arch/bin/$n $arch_prefix
		done
		mkdir -p $arch_prefix/gstreamer-0.10
		for n in `cat $build_base/gstbundle_gstplugins_win`; do
			cp -a $deps_base/$gstbundle_win_dir/$target_arch/lib/gstreamer-0.10/$n $arch_prefix/gstreamer-0.10
		done
		cp $deps_base/$psimedia_win_dir/$target_arch/plugins/gstprovider.dll $arch_prefix
		if [ "$target_arch" == "x86_64" ]; then
			cp /c/mingw64/bin/libgcc_s_sjlj-1.dll $arch_prefix
			cp /c/mingw64/bin/libstdc++-6.dll $arch_prefix
			cp /c/mingw64/bin/libwinpthread-1.dll $arch_prefix
		else
			cp /mingw/bin/libgcc_s_dw2-1.dll $arch_prefix
			cp /mingw/bin/libstdc++-6.dll $arch_prefix
			cp /mingw/bin/mingwm10.dll $arch_prefix
			cp /mingw/bin/pthreadGC2.dll $arch_prefix
		fi
		cp -a certs $arch_prefix
		cp -a iconsets $arch_prefix
		cp -a sound $arch_prefix
		cp COPYING $arch_prefix
		win32/tod README $arch_prefix/ReadMe.txt
		win32/tod INSTALL $arch_prefix/Install.txt

		mingw32-make distclean
	else
		if [ "$QT_LIB_PATH" == "" ]; then
			QT_LIB_PATH=$QTDIR/lib
		fi
		cd $psi_base
		export DYLD_FRAMEWORK_PATH=$QT_LIB_PATH:$deps_base/$qca_mac_dir/lib:$deps_base/$growl_dir/Framework
		./configure --with-qca-inc=$deps_base/$qca_mac_dir/include --with-qca-lib=$deps_base/$qca_mac_dir/lib --with-growl=$deps_base/$growl_dir/Framework --enable-universal
		make
	fi
}

if [ "$target_arch" != "" ]; then
	build_package $package_name $target_arch
else
	build_package $package_name uni
fi
