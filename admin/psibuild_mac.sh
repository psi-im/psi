#!/bin/bash

#Dependencies:
#git
#cmake
#xcode
#Qt 5.x


# Qt5 Settings
MAC_SDK_VER=10.12
MAC_DEPLOYMENT_TARGET=10.9
QTDIR="${HOME}/Qt5.9.0/5.9/clang_64"
QT_FRAMEWORK_VERSION=5

QT_FRAMEWORKS="QtCore QtNetwork QtXml QtGui QtMultimedia QtMultimediaWidgets QtWidgets QtConcurrent QtPrintSupport QtOpenGL QtSvg QtWebEngineWidgets QtWebEngineCore QtQuick QtQml QtWebChannel QtPositioning QtQuickWidgets"  #QtDBus QtWebEngine 

QT_PLUGINS="audio/libqtaudio_coreaudio.dylib bearer/libqcorewlanbearer.dylib bearer/libqgenericbearer.dylib platforms/libqcocoa.dylib printsupport/libcocoaprintersupport.dylib iconengines/libqsvgicon.dylib"
QT_PLUGINS="${QT_PLUGINS} mediaservice/libqtmedia_audioengine.dylib mediaservice/libqavfmediaplayer.dylib imageformats/libqgif.dylib imageformats/libqjpeg.dylib imageformats/libqsvg.dylib  imageformats/libqwbmp.dylib imageformats/libqtiff.dylib imageformats/libqwebp.dylib  imageformats/libqtga.dylib imageformats/libqico.dylib imageformats/libqicns.dylib imageformats/libqmacjp2.dylib"

export QMAKESPEC="macx-clang"

# OPTIONS / НАСТРОЙКИ

# build and store directory / каталог для сорсов и сборки
PSI_DIR="${HOME}/psi"

QCONFDIR="${PSI_DIR}/qconf"
DEPS_PREFIX="${PSI_DIR}/deps"

PSI_APP="psi.app"

GROWL_URL="https://drive.google.com/uc?export=download&id=0B9THQ10qg_RSNWhuOW1rbV9WYmc"
GROWL_FILE="Growl-2.0.1"

DEPS_URL="https://drive.google.com/uc?export=download&id=0B9THQ10qg_RSaXplUDlXNEZ3ZDQ"
DEPS_FILE="deps_170602"
DEPS_DIR="deps"

GST_URL="https://drive.google.com/uc?export=download&id=0B9THQ10qg_RSa0hTWWJydDdoN1k"
GST_FILE="gstbundle-0.10.36-mac"
GST_DIR=$GST_FILE

PSIMEDIA_URL="https://drive.google.com/uc?export=download&id=0B9THQ10qg_RSYmtWbk1zN2JJems"
PSIMEDIA_FILE="psimedia-1.0.5-mac"
PSIMEDIA_DIR=$PSIMEDIA_FILE

# official repository
GIT_REPO="https://github.com/psi-im/psi.git"
GIT_REPO_PLUS=https://github.com/psi-plus/main.git
GIT_REPO_PLUGINS=https://github.com/psi-im/plugins.git
GIT_REPO_MAINTENANCE=https://github.com/psi-plus/maintenance.git
GIT_REPO_RESOURCES=https://github.com/psi-plus/resources.git
QCONF_REPO_URI="https://github.com/psi-plus/qconf.git"
LANGS_REPO_URI="git://github.com/psi-plus/psi-plus-l10n.git"
QCA_REPO_URI="https://github.com/KDE/qca.git"

ALL_TRANS="1"
ENABLE_WEBKIT="0"
WORK_OFFLINE="0"


SSL_PATH="${DEPS_PREFIX}"
LIBS_PATH="${DEPS_PREFIX}"
QCA_PREFIX="${QTDIR}" #"${DEPS_PREFIX}"
QCA_PATH="${PSI_DIR}/qca-build"
QCA_VER="2.2.0"
QCA_PLUGINS_PATH=${QCA_PREFIX}/plugins/crypto #${QCA_PATH}/lib/qca-qt5/crypto


export PATH="$QTDIR/bin:$PATH"


#######################
# FUNCTIONS / ФУНКЦИИ #
#######################
# Exit with error message
die() { echo; echo " !!!ERROR: $@"; exit 1; }
warning() { echo; echo " !!!WARNING: $@"; }
log() { echo -e "\033[1m*** $@ \033[0m"; }


get_qconf() {
	cd "${PSI_DIR}"
	if [ ! -f "$QCONFDIR/qconf" ]; then
		git_fetch "${QCONF_REPO_URI}" qconf "QConf"
		cd qconf && ./configure && $MAKE $MAKEOPT || die "Can't build qconf!"
	fi

	if [ -f "$QCONFDIR/qconf" ]; then
		QCONF="$QCONFDIR/qconf"
	else
		die "Can'find qconf!"
	fi
}

check_env() {
	log "Testing environment..."

	if [ ! -d "${PSI_DIR}" ]
	then
		mkdir "${PSI_DIR}" || die "can't create work directory ${PSI_DIR}"	
	fi

	MAKEOPT=${MAKEOPT:--j$((`sysctl -n hw.ncpu`+1)) -s }
	STAT_USER_ID='stat -f %u'
	STAT_USER_NAME='stat -f %Su'
	SED_INPLACE_ARG=".bak"

	v=`cmake --version 2>/dev/null` || \
		die "You should install CMake first. / Сначала установите CMake"

	v=`git --version 2>/dev/null` || \
		die "You should install Git first. / Сначала установите Git (http://git-scm.com/download)"

	# Make
	if [ ! -f "${MAKE}" ]; then
		MAKE=""
		for gn in gmake make; do
			[ -n "`$gn --version 2>/dev/null`" ] && { MAKE="$gn"; break; }
		done
		[ -z "${MAKE}" ] && die "You should install GNU Make first / "\
			"Сначала установите GNU Make"
	fi
	log "\tFound make tool: ${MAKE}"

	# QConf
	QCONF=`which qconf`
	if [ -z "${QCONF}" ]
	then
		for qc in qt-qconf qconf qconf-qt4; do
			v=`$qc --version 2>/dev/null |grep affinix` && QCONF=$qc
		done
    	fi
	if [ -z "${QCONF}" ]
	then
		get_qconf
	fi

	[ ! -z "${QCONF}" ] && log "\tFound qconf tool: " $QCONF

	find_qt_util() {
		local name=$1
		result=""
		for un in $QTDIR/bin/$name $QTDIR/bin/$name-qt4 $QTDIR/bin/qt4-${name} $QTDIR/bin/${name}4; do
			[ -n "`$un -v 2>&1 |grep Qt`" ] && { result="$un"; break; }
		done
		if [ -z "${result}" ]; then
			[ "$nonfatal" = 1 ] || die "You should install $name util as part of"\
				"Qt framework / Сначала установите утилиту $name из Qt framework"
			log "${name} Qt tool is not found. ignoring.."
		else
			log "\tFound ${name} Qt tool: ${result}"
		fi
	}

	local result
	# qmake
	find_qt_util qmake; QMAKE="${result}"
	find_qt_util macdeployqt; MACDEPLOYQT="${result}"
	nonfatal=1 find_qt_util lrelease; LRELEASE="${result}"
	log "Environment is OK"
}

git_fetch() {
  local remote="$1"
  local target="$2"
  local comment="$3"
  local curd=`pwd`
  local forcesubmodule=0
  [ -d "${target}/.git" ] && [ "$(cd "${target}" && git config --get remote.origin.url)" = "${remote}" ] && {
    [ $WORK_OFFLINE = 0 ] && {
      cd "${target}"
      [ -n "${comment}" ] && log "Update ${comment} .."
      git pull 2>/dev/null || die "git update failed"
      cd "${curd}"
    } || true
  } || {
    forcesubmodule=1
    log "Checkout ${comment} .."
    [ -d "${target}" ] && rm -rf "$target"
    git clone "${remote}" "$target" 2>/dev/null || die "git clone failed"
  }
  [ $WORK_OFFLINE = 0 ] && {
    cd "${target}"
    git submodule update --init 2>/dev/null || die "git submodule update failed"
  }
  cd "${curd}"
}

cleanup_framework() {
	# remove dev stuff
	rm -rf $1/Headers
	rm -f $1/${2}_debug
	rm -f $1/${2}_debug.prl
	rm -rf $1/Versions/$3/Headers
	rm -f $1/Versions/$3/${2}_debug
	rm -f $1/Versions/$3/${2}_debug.prl
}

prepare_workspace() {
	log "Init directories..."

	fetch_sources
	fetch_deps

	rm -rf "${PSI_DIR}"/build

	[ -d "${PSI_DIR}"/build ] && die "can't delete old build directory ${PSI_DIR}/build"
	mkdir "${PSI_DIR}"/build || die "can't create build directory ${PSI_DIR}/build"
	log "\tCreated base directory structure"

	cd "${PSI_DIR}"
	get_framework $GROWL_URL $GROWL_FILE Growl.framework

	if [ ! -f "${QCA_PREFIX}/lib/qca-qt5.framework/qca-qt5" ]
	then
		build_qca
	fi
}

get_framework() {
	get_url=$1
	file_name=$2	

	if [ ! -f $file_name.tar.bz2 ]; then
		log "Downloading $file_name"
		curl -L -o $file_name.tar.bz2 $get_url || die "can't download url $get_url"
		tar jxvf $file_name.tar.bz2 2>/dev/null || die "can't extract file $file_name"
	fi
}

fetch_sources() {
	log "Fetch sources..."
	cd "${PSI_DIR}"
	git_fetch "${GIT_REPO}" psi "Psi"
	git_fetch "${GIT_REPO_PLUS}" git-plus "Psi+ additionals"
	git_fetch "${GIT_REPO_MAINTENANCE}" maintenance "Psi+ maintenance"
	git_fetch "${GIT_REPO_RESOURCES}" resources "Psi+ resources"
	git_fetch "${GIT_REPO_PLUGINS}" plugins "Psi plugins"
	git_fetch "${LANGS_REPO_URI}" translations "Psi+ translations"
	git_fetch "${QCA_REPO_URI}" qca "QCA"
}

get_deps() {
	local deps_file=$1
	local deps_url=$2
	local deps_dir=$3

	if [ ! -d ${deps_dir} ]
	then
		if [ ! -f ${deps_file}.tar.bz2 ]
        	then
			log "Downloading ${deps_file}"
			curl -L -o ${deps_file}.tar.bz2 ${deps_url} || die "can't download url ${deps_url}"
		fi
		
		log "Extracting ${deps_file}"
		tar jxvf ${deps_file}.tar.bz2 2>/dev/null || die "can't extract file ${deps_file}"
	fi

}

fetch_deps() {
	cd ${PSI_DIR}

	get_deps $DEPS_FILE $DEPS_URL $DEPS_DIR
	get_deps $GST_FILE $GST_URL $GST_DIR
	get_deps $PSIMEDIA_FILE $PSIMEDIA_URL $PSIMEDIA_DIR
}

build_qca() {
	mkdir -p "${QCA_PATH}" && cd "${QCA_PATH}" || die "Can't create QCA build folder"
	
	export CC="/usr/bin/clang"
	export CXX="/usr/bin/clang++"
        
	log "Compiling QCA..."

	sed -ie "s/target_link_libraries(qca-ossl crypto)/target_link_libraries(qca-ossl)/" "${PSI_DIR}/qca/plugins/qca-ossl/CMakeLists.txt"
	
	local opts="-DBUILD_TESTS=OFF -DOPENSSL_ROOT_DIR=${SSL_PATH} -DOPENSSL_LIBRARIES=${SSL_PATH}/lib -DLIBGCRYPT_LIBRARIES=${DEPS_PREFIX}/lib"
	#opts=$opts -DCMAKE_VERBOSE_MAKEFILE=ON -DCMAKE_CXX_FLAGS="-stdlib=libc++ -std=gnu++11 -arch x86_64"
	cmake -DCMAKE_INSTALL_PREFIX="${QCA_PREFIX}" -DQCA_PLUGINS_INSTALL_DIR="${QCA_PLUGINS_PATH}/.." $opts ${PSI_DIR}/qca 2>/dev/null || die "QCA configuring error"
	make ${MAKEOPT} || die "QCA build error"
	
	make install || die "Can't install QCA"

	install_name_tool -id @rpath/qca-qt5.framework/Versions/${QCA_VER}/qca-qt5 "${QCA_PREFIX}/lib/qca-qt5.framework/qca-qt5"
	QCA_PLUGINS=`ls ${QCA_PLUGINS_PATH} | grep "dylib"`

	for p in $QCA_PLUGINS; do
               install_name_tool  -change "${QCA_PREFIX}/lib/qca-qt5.framework/Versions/${QCA_VER}/qca-qt5" "@rpath/qca-qt5.framework/Versions/${QCA_VER}/qca-qt5" "${QCA_PLUGINS_PATH}/$p"
	done
}

prepare_sources() {
	log "Exporting sources..."

	cd "${PSI_DIR}"
	cp -a "psi/" "build/"

	TRANSLATIONS=`ls ${PSI_DIR}/translations/translations | grep -v en | sed s/psi_// | sed s/.ts//`

	local actual_translations=""
	LANGS_DIR="${PSI_DIR}/build/langs"
	[ -n "$TRANSLATIONS" ] && {
		mkdir -p $LANGS_DIR
		for l in $TRANSLATIONS; do
			mkdir -p "$LANGS_DIR/$l"
			cp -f "translations/translations/psi_$l.ts"  "$LANGS_DIR/$l/psi_$l.ts"
			[ -n "${LRELEASE}" -o -f "$LANGS_DIR/$l/psi_$l.qm" ] && actual_translations="${actual_translations} $l"
		done
		actual_translations="$(echo $actual_translations)"
		[ -z "${actual_translations}" ] && warning "Translations not found"
	}

	log "Copying plugins..."
	cd "${PSI_DIR}/plugins/generic"
	PLUGINS=`find . -maxdepth 1 -name '*plugin' | cut -d"/" -f2 | grep -v "videostatusplugin"`
	cp -a "${PSI_DIR}/plugins/generic" "${PSI_DIR}/build/src/plugins" || \
		die "preparing plugins requires prepared psi sources"

	cd "${PSI_DIR}/psi"
	local rev=$(./admin/git_revnumber.sh)
	local rev_date=$(git log -n1 --date=short --pretty=format:'%ad')

	cd ${PSI_DIR}/build
	local cur_ver=$(cat ./version | grep -Eoe "^[^\-]+")

        if [ -z $VERSION ];
        then
            VERSION=$(echo "${cur_ver}.${rev}$([ "$ENABLE_WEBKIT" = 1 ] && echo "-webkit") ($(echo ${rev_date}))")
        else
		if [ $ENABLE_WEBKIT != 0 ]; then
            		VERSION="${VERSION}-webkit"
        	fi
	fi

	log "Psi version is ${VERSION}"

	echo "${VERSION}" > version
}

src_compile() {
	log "All ready. Now run make..."
	cd "${PSI_DIR}"/build

	CONF_OPTS="--disable-qdbus --enable-whiteboarding --disable-xss --release"

	if [ $ENABLE_WEBKIT != 0 ]; then
		CONF_OPTS=" --enable-webkit $CONF_OPTS"
	fi

	CONF_OPTS=" --with-idn-lib=${LIBS_PATH}/lib --with-idn-inc=${LIBS_PATH}/include --with-qca-lib=${QCA_PREFIX}/lib --with-zlib-lib=${LIBS_PATH}/lib --with-zlib-inc=${LIBS_PATH}/include --with-growl=${PSI_DIR} $CONF_OPTS"

	${QCONF} || die "QConf failed"

	export DYLD_FRAMEWORK_PATH=${QCA_PREFIX}/lib:${PSI_DIR}:$DYLD_FRAMEWORK_PATH

	./configure  ${CONF_OPTS} || die "configure failed"

	if [ ! -z "${MAC_SDK_VER}" ]; then
		echo "QMAKE_MAC_SDK = macosx${MAC_SDK_VER}" >> conf.pri
		echo "QMAKE_MACOSX_DEPLOYMENT_TARGET = ${MAC_DEPLOYMENT_TARGET}" >> conf.pri
	fi

	echo "QMAKE_CXXFLAGS += -Wno-inconsistent-missing-override" >> conf.pri

	$MAKE $MAKEOPT || die "make failed"

	plugins_compile
}

prep_otr_plugin() {
	sed -ie "s/buffio.h/tidybuffio.h/" src/htmltidy.*
	echo "INCLUDEPATH += ${DEPS_PREFIX}/include/ ${DEPS_PREFIX}/include/libotr" >> otrplugin.pro
	echo "LIBS += -L${DEPS_PREFIX}/lib" >> otrplugin.pro
}

plugins_compile() {
	cd "${PSI_DIR}/build/src/plugins"
	echo "QMAKE_MAC_SDK = macosx${MAC_SDK_VER}" >> psiplugin.pri
	echo "QMAKE_MACOSX_DEPLOYMENT_TARGET = ${MAC_DEPLOYMENT_TARGET}" >> psiplugin.pri
	
	log "List plugins for compiling..."
	echo ${PLUGINS}
	log "Compiling plugins..."
	for pl in ${PLUGINS}; do
		cd ${PSI_DIR}/build/src/plugins/generic/${pl} && log "Compiling ${pl} plugin."

		if [ $pl = "otrplugin" ]; then
			prep_otr_plugin
		fi
		
		$QMAKE && $MAKE $MAKEOPT || log "make ${pl} plugin failed"
	done
}

copy_qca() {
	log "\tCopying qca..."

	cp -a "${QCA_PREFIX}/lib/qca-qt5.framework" "$CONTENTSDIR/Frameworks"
	cleanup_framework "$CONTENTSDIR/Frameworks/qca-qt5.framework" qca-qt5 ${QCA_VER}
	mkdir -p "$BUNDLE_PLUGINS_DIR/crypto/"

	local QCA_PLUGINS=`ls ${QCA_PLUGINS_PATH} | grep "dylib"`

	for p in $QCA_PLUGINS; do
		cp -f "${QCA_PLUGINS_PATH}/$p" "$BUNDLE_PLUGINS_DIR/crypto/$p"

		install_name_tool -change "${SSL_PATH}/lib/libcrypto.1.0.0.dylib" "@executable_path/../Frameworks/libcrypto.dylib"    "$BUNDLE_PLUGINS_DIR/crypto/$p"
		install_name_tool -change "${SSL_PATH}/lib/libssl.1.0.0.dylib"    "@executable_path/../Frameworks/libssl.dylib"       "$BUNDLE_PLUGINS_DIR/crypto/$p"
		install_name_tool -change "${LIBS_PATH}/lib/libgcrypt.20.dylib"   "@executable_path/../Frameworks/libgcrypt.dylib"    "$BUNDLE_PLUGINS_DIR/crypto/$p"
		install_name_tool -change "${LIBS_PATH}/lib/libgpg-error.0.dylib" "@executable_path/../Frameworks/libgpg-error.dylib" "$BUNDLE_PLUGINS_DIR/crypto/$p"
	done
}

copy_gst_bundle() {
	log "\tCopying psimedia..."

	local build_base=${PSI_DIR}/build/admin/build
	local gst_base=${PSI_DIR}/${GST_FILE}/x86_64
	local psimedia_base=${PSI_DIR}/${PSIMEDIA_FILE}

	GSTBUNDLE_LIB_FILES=
	for n in `cat $build_base/gstbundle_libs_mac`; do
		for l in `find $gst_base/lib -maxdepth 1 -type f -name $n`; do
			base_l=`basename $l`
			GSTBUNDLE_LIB_FILES="$GSTBUNDLE_LIB_FILES $base_l"
		done
	done

	GSTBUNDLE_LIB_SUBS=
	for n in `cat $build_base/gstbundle_libs_mac`; do
		for l in `find $gst_base/lib -maxdepth 1 -name $n`; do
			base_l=`basename $l`
			GSTBUNDLE_LIB_SUBS="$GSTBUNDLE_LIB_SUBS $base_l"
		done
	done

	GSTBUNDLE_LIB_GST_FILES=
	for n in `cat $build_base/gstbundle_gstplugins_mac`; do
		for l in `find $gst_base/lib/gstreamer-0.10 -type f -name $n`; do
			base_l=`basename $l`
			if [ "$base_l" != "libgstosxaudio.so" ]; then
				GSTBUNDLE_LIB_GST_FILES="$GSTBUNDLE_LIB_GST_FILES $base_l"
			fi
		done
	done

	# subs are files we need to copy, in addition to being what we attempt to substitute via install_name_tool
	for l in $GSTBUNDLE_LIB_SUBS; do
		cp -a $gst_base/lib/$l $CONTENTSDIR/Frameworks
	done

	# now to substitutions on the regular files only
	for l in $GSTBUNDLE_LIB_FILES; do
		for g in $GSTBUNDLE_LIB_SUBS; do
			install_name_tool -change $g @executable_path/../Frameworks/$g $CONTENTSDIR/Frameworks/$l
		done
	done

	mkdir -p $CONTENTSDIR/Frameworks/gstreamer-0.10
	for p in $GSTBUNDLE_LIB_GST_FILES; do
		cp $gst_base/lib/gstreamer-0.10/$p $CONTENTSDIR/Frameworks/gstreamer-0.10
		for g in $GSTBUNDLE_LIB_SUBS; do
			install_name_tool -change $g @executable_path/../Frameworks/$g $CONTENTSDIR/Frameworks/gstreamer-0.10/$p
		done
	done

	cp $psimedia_base/plugins/libgstprovider.dylib $BUNDLE_PLUGINS_DIR
	for g in $GSTBUNDLE_LIB_SUBS; do
		install_name_tool -change $g @executable_path/../Frameworks/$g $BUNDLE_PLUGINS_DIR/libgstprovider.dylib
	done
}

copy_qt() {
	log "\tCopying Qt libs..."

	local plugins_dir_name=Plugins
	BUNDLE_PLUGINS_DIR=$CONTENTSDIR/$plugins_dir_name

	for f in $QT_FRAMEWORKS; do
		cp -a ${QTDIR}/lib/$f.framework $CONTENTSDIR/Frameworks
		cleanup_framework $CONTENTSDIR/Frameworks/$f.framework $f ${QT_FRAMEWORK_VERSION}
	done

	for p in $QT_PLUGINS; do
		mkdir -p $BUNDLE_PLUGINS_DIR/$(dirname $p);
		cp -a ${QTDIR}/plugins/$p $BUNDLE_PLUGINS_DIR/$p
	done

	qt_conf_file="$CONTENTSDIR/Resources/qt.conf"
	touch ${qt_conf_file}
	echo "[Paths]" >> ${qt_conf_file}
	echo "Plugins = ${plugins_dir_name}" >> ${qt_conf_file}

	install_name_tool -add_rpath "@executable_path/../Frameworks"  "$CONTENTSDIR/MacOS/psi"
}

copy_main_libs() {
	log "\tCopying libs..."

        cp -f "${LIBS_PATH}/lib/libz.dylib"     "$CONTENTSDIR/Frameworks/"
        cp -f "${LIBS_PATH}/lib/libidn.dylib"   "$CONTENTSDIR/Frameworks/"
        cp -f "${SSL_PATH}/lib/libssl.dylib"    "$CONTENTSDIR/Frameworks/"
        cp -f "${SSL_PATH}/lib/libcrypto.dylib" "$CONTENTSDIR/Frameworks/"
        chmod +w "$CONTENTSDIR/Frameworks/libssl.dylib"
        chmod +w "$CONTENTSDIR/Frameworks/libcrypto.dylib"

        install_name_tool -id "@executable_path/../Frameworks/libz.dylib" "$CONTENTSDIR/Frameworks/libz.dylib"
        install_name_tool -id "@executable_path/../Frameworks/libidn.dylib" "$CONTENTSDIR/Frameworks/libidn.dylib"
        install_name_tool -id "@executable_path/../Frameworks/libssl.dylib" "$CONTENTSDIR/Frameworks/libssl.dylib"
        install_name_tool -change "${SSL_PATH}/lib/libcrypto.1.0.0.dylib" "@executable_path/../Frameworks/libcrypto.dylib" "$CONTENTSDIR/Frameworks/libssl.dylib"
        install_name_tool -id "@executable_path/../Frameworks/libcrypto.dylib" "$CONTENTSDIR/Frameworks/libcrypto.dylib"

        install_name_tool -change "${LIBS_PATH}/lib/libz.1.dylib"    "@executable_path/../Frameworks/libz.dylib"   "$CONTENTSDIR/MacOS/psi"
        install_name_tool -change "${LIBS_PATH}/lib/libidn.11.dylib" "@executable_path/../Frameworks/libidn.dylib" "$CONTENTSDIR/MacOS/psi"
}

copy_otrplugins_libs() {
	log "\tCopying otr libs..."

        cp -f "${LIBS_PATH}/lib/libgpg-error.dylib" "$CONTENTSDIR/Frameworks/"
        cp -f "${LIBS_PATH}/lib/libgcrypt.dylib"    "$CONTENTSDIR/Frameworks/"
        cp -f "${LIBS_PATH}/lib/libotr.dylib"       "$CONTENTSDIR/Frameworks/"
        cp -f "${LIBS_PATH}/lib/libtidy.dylib"      "$CONTENTSDIR/Frameworks/"

        install_name_tool -id "@executable_path/../Frameworks/libgpg-error.dylib" "$CONTENTSDIR/Frameworks/libgpg-error.dylib"
        
	install_name_tool -id "@executable_path/../Frameworks/libgcrypt.dylib" "$CONTENTSDIR/Frameworks/libgcrypt.dylib"
	install_name_tool -change "${LIBS_PATH}/lib/libgpg-error.0.dylib" "@executable_path/../Frameworks/libgpg-error.dylib" "$CONTENTSDIR/Frameworks/libgcrypt.dylib"
        
	install_name_tool -id "@executable_path/../Frameworks/libotr.dylib" "$CONTENTSDIR/Frameworks/libotr.dylib"
        install_name_tool -change "${LIBS_PATH}/lib/libgcrypt.20.dylib" "@executable_path/../Frameworks/libgcrypt.dylib" "$CONTENTSDIR/Frameworks/libotr.dylib"
	install_name_tool -change "${LIBS_PATH}/lib/libgpg-error.0.dylib" "@executable_path/../Frameworks/libgpg-error.dylib" "$CONTENTSDIR/Frameworks/libotr.dylib"
        
	install_name_tool -id "@executable_path/../Frameworks/libtidy.dylib" "$CONTENTSDIR/Frameworks/libtidy.dylib"

        install_name_tool -change "${LIBS_PATH}/lib/libotr.5.dylib"       "@executable_path/../Frameworks/libotr.dylib"       "$CONTENTSDIR/Resources/plugins/libotrplugin.dylib"
	install_name_tool -change "${LIBS_PATH}/lib/libgcrypt.20.dylib"   "@executable_path/../Frameworks/libgcrypt.dylib"    "$CONTENTSDIR/Resources/plugins/libotrplugin.dylib"
	install_name_tool -change "${LIBS_PATH}/lib/libgpg-error.0.dylib" "@executable_path/../Frameworks/libgpg-error.dylib" "$CONTENTSDIR/Resources/plugins/libotrplugin.dylib"
	install_name_tool -change "${LIBS_PATH}/lib/libtidy.5.dylib"      "@executable_path/../Frameworks/libtidy.dylib"      "$CONTENTSDIR/Resources/plugins/libotrplugin.dylib"
	install_name_tool -change "libtidy.5.dylib"      "@executable_path/../Frameworks/libtidy.dylib"      "$CONTENTSDIR/Resources/plugins/libotrplugin.dylib" #can be this path
}

copy_plugins() {
	log "\tCopying plugins..."

	if [ ! -d $CONTENTSDIR/Resources/plugins ]; then
    		mkdir -p "$CONTENTSDIR/Resources/plugins"
	fi

	for pl in ${PLUGINS}; do
		cd ${PSI_DIR}/build/src/plugins/generic/${pl}
		if [ -f "lib${pl}.dylib" ]; then
 			cp "lib${pl}.dylib" "$CONTENTSDIR/Resources/plugins"
		fi
	done
}

copy_resources() {
	log "\tCopying langpack, web, skins..."

	cd "${PSI_DIR}/build"

	app_bundl_tr_dir="$CONTENTSDIR/Resources/translations"
	mkdir "$app_bundl_tr_dir"
	cd "$app_bundl_tr_dir"
	for l in $TRANSLATIONS
	do
		f="$LANGS_DIR/$l/psi_$l"
		[ -n "${LRELEASE}" -a -f "${f}.ts" ] && "${LRELEASE}" -silent "${f}.ts" 2>/dev/null
		[ -f "${f}.qm" ] && cp "${f}.qm" . && {
			log "Copy translations files for ${l}"

                	qt_tr=`ls ${QTDIR}/translations | grep "^qt.*_${l}"`
			for qtrl in $qt_tr; do
				cp -f "${QTDIR}/translations/${qtrl}" .
			done
		}
	done

	cd "$CONTENTSDIR/Resources/"
	cp -r ${PSI_DIR}/build/sound .
	cp -r ${PSI_DIR}/build/themes .
	cp -r ${PSI_DIR}/build/iconsets .
	cp -r ${PSI_DIR}/resources/sound .
	cp -f ${PSI_DIR}/build/client_icons.txt .
}

copy_growl() {
	log "\tCopying Growl..."

	cp -a "${PSI_DIR}/Growl.framework" "$CONTENTSDIR/Frameworks"
	cleanup_framework "$CONTENTSDIR/Frameworks/Growl.framework" Growl A
}

prepeare_bundle() {	
	log "Copying dependencies..."

	cd "${PSI_DIR}/build"

	CONTENTSDIR=${PSI_DIR}/build/${PSI_APP}/Contents
	mkdir "$CONTENTSDIR/Frameworks"	

	copy_qt
	copy_qca
	copy_plugins
	copy_growl
	copy_main_libs
	copy_otrplugins_libs
	copy_gst_bundle
	copy_resources
}

make_bundle() {
	log "Making standalone bundle..."

	VOLUME_NAME="Psi"
	WC_DMG=wc.dmg
	WC_DIR=wc
	TEMPLATE_DMG=template.dmg
	MASTER_DMG="psi.dmg"

	cd ${PSI_DIR}/build

	# generate empty template
	mkdir template
	hdiutil create -size 260m "$TEMPLATE_DMG" -srcfolder template -format UDRW -volname "$VOLUME_NAME" -quiet
	rmdir template

	cp $TEMPLATE_DMG $WC_DMG

	mkdir -p $WC_DIR
	hdiutil attach "$WC_DMG" -noautoopen -quiet -mountpoint "$WC_DIR"
	cp -a $PSI_APP $WC_DIR
	diskutil eject `diskutil list | grep "$VOLUME_NAME" | grep "Apple_HFS" | awk '{print $6}'`

	rm -f $MASTER_DMG
	hdiutil convert "$WC_DMG" -quiet -format UDZO -imagekey zlib-level=9 -o $MASTER_DMG
	rm -rf $WC_DIR $WC_DMG
	hdiutil internet-enable -yes -quiet $MASTER_DMG || true

	newf="${PSI_DIR}/psi-${VERSION}-mac.dmg"
	mv -f $MASTER_DMG "${newf}"
}

make_bundle2() {
	log "Making standalone bundle..."

	cd ${PSI_DIR}/build/admin/build
	cp -f "${PSI_DIR}/maintenance/scripts/macosx/template.dmg.bz2" "template.dmg.bz2"
	sh pack_dmg.sh "psi-${VERSION}-mac.dmg" "Psi" "dist/psi-${VERSION}-mac"

	newf="${PSI_DIR}/psi-${VERSION}-mac.dmg"
	mv -f "psi-${VERSION}-mac.dmg" "${newf}"
}


#############
# Go Go Go! #
#############
while [ "$1" != "" ]; do
	case $1 in
		-w | --webkit )		ENABLE_WEBKIT=1
							;;
		-h | --help )		echo "usage: $0 [-w | --webkit] [-v | --verbose] VERSION"
							exit
							;;
		* )					VERSION=$1
	esac
	shift
done

starttime=`date "+Start time: %H:%M:%S"`
check_env
prepare_workspace
prepare_sources
src_compile

prepeare_bundle
make_bundle

finishtime=`date "+Finish time: %H:%M:%S"`
echo $starttime
echo $finishtime
