#!/bin/bash

#Dependencies:
#git
#cmake
#xcode
#Qt 5.x


# Qt5 Settings
MAC_SDK_VER=10.9
QTDIR="${HOME}/Qt5.8.0/5.8/clang_64"
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
QCA_PREFIX="${DEPS_PREFIX}"
QCA_PATH="${PSI_DIR}/qca-build"
QCA_VER="2.2.0"


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

	MAKEOPT=${MAKEOPT:--j$((`sysctl -n hw.ncpu`+1)) -s }
	STAT_USER_ID='stat -f %u'
	STAT_USER_NAME='stat -f %Su'
	SED_INPLACE_ARG=".bak"

	v=`gmake --version 2>/dev/null`
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
	if [ ! -d "${PSI_DIR}" ]
	then
		mkdir "${PSI_DIR}" || die "can't create work directory ${PSI_DIR}"	
	fi

	fetch_sources
	fetch_deps

	rm -rf "${PSI_DIR}"/build

	[ -d "${PSI_DIR}"/build ] && die "can't delete old build directory ${PSI_DIR}/build"
	mkdir "${PSI_DIR}"/build || die "can't create build directory ${PSI_DIR}/build"
	log "\tCreated base directory structure"

	cd "${PSI_DIR}"
	get_framework $GROWL_URL $GROWL_FILE Growl.framework

	if [ ! -f "${QCA_PATH}/lib/qca-qt5.framework/qca-qt5" ]
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

fetch_deps() {
	cd ${PSI_DIR}

	if [ ! -d "deps" ]
	then
		if [ ! -f ${DEPS_FILE}.tar.bz2 ]
        	then
			log "Downloading ${DEPS_FILE}"
			curl -L -o ${DEPS_FILE}.tar.bz2 ${DEPS_URL} || die "can't download url ${DEPS_URL}"
		fi
		
		log "Extracting ${DEPS_FILE}"
		tar jxvf ${DEPS_FILE}.tar.bz2 2>/dev/null || die "can't extract file ${DEPS_FILE}"
	fi
}

build_qca() {
	mkdir -p "${QCA_PATH}" && cd "${QCA_PATH}" || die "Can't create QCA buld folder"
	
	export CC="/usr/bin/clang"
	export CXX="/usr/bin/clang++"
        
	log "Compiling QCA..."
	
	local opts="-DBUILD_TESTS=OFF -DOPENSSL_ROOT_DIR=${SSL_PATH} -DOPENSSL_LIBRARIES=${SSL_PATH}/lib -DLIBGCRYPT_LIBRARIES=${DEPS_PREFIX}/lib"
        cmake -DCMAKE_INSTALL_PREFIX="${QCA_PREFIX}" $opts  ../qca 2>/dev/null || die "QCA configuring error"
        make ${MAKEOPT} || die "QCA build error"
	
	install_name_tool -id @rpath/qca-qt5.framework/Versions/${QCA_VER}/qca-qt5 "${QCA_PATH}/lib/qca-qt5.framework/qca-qt5"
	QCA_PLUGINS=`ls ${QCA_PATH}/lib/qca-qt5/crypto | grep "dylib"`

	for p in $QCA_PLUGINS; do
               install_name_tool  -change "${QCA_PREFIX}/lib/qca-qt5.framework/Versions/${QCA_VER}/qca-qt5" "@rpath/qca-qt5.framework/Versions/${QCA_VER}/qca-qt5" "${QCA_PATH}/lib/qca-qt5/crypto/$p"
	done
	
	#make install || die "Can't install QCA"
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

        if [ -z $VERSION ];
        then
            VERSION=$(echo "1.0.${rev}$([ "$ENABLE_WEBKIT" = 1 ] && echo "-webkit") ($(echo ${rev_date}))")
        else
		if [ $ENABLE_WEBKIT != 0 ]; then
            		VERSION="${VERSION}-webkit"
        	fi
	fi

        echo "${VERSION}" > version
}

src_compile() {
	log "All ready. Now run make..."
	cd "${PSI_DIR}"/build

        CONF_OPTS="--disable-qdbus --enable-whiteboarding --disable-xss --release"

	if [ $ENABLE_WEBKIT != 0 ]; then
		CONF_OPTS=" --enable-webkit $CONF_OPTS"
	fi

        CONF_OPTS=" --with-idn-lib=${LIBS_PATH}/lib --with-idn-inc=${LIBS_PATH}/include --with-qca-lib=${QCA_PATH}/lib --with-zlib-lib=${LIBS_PATH}/lib --with-zlib-inc=${LIBS_PATH}/include --with-growl=${PSI_DIR} $CONF_OPTS"

	${QCONF} || die "QConf failed"

	export DYLD_FRAMEWORK_PATH=$DYLD_FRAMEWORK_PATH:${QCA_PATH}/lib:${PSI_DIR}

	./configure  ${CONF_OPTS} || die "configure failed"

	if [ ! -z "${MAC_SDK_VER}" ]; then
		echo "QMAKE_MAC_SDK = macosx${MAC_SDK_VER}" >> conf.pri
		echo "QMAKE_MACOSX_DEPLOYMENT_TARGET = ${MAC_SDK_VER}" >> conf.pri
	fi

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
        #QCA staff
        cp -a "${QCA_PATH}/lib/qca-qt5.framework" "$contentsdir/Frameworks"
        cleanup_framework "$contentsdir/Frameworks/qca-qt5.framework" qca-qt5 ${QCA_VER}
	mkdir -p "$contentsdir/PlugIns/crypto/"

	local QCA_PLUGINS=`ls ${QCA_PATH}/lib/qca-qt5/crypto | grep "dylib"`

	for p in $QCA_PLUGINS; do
		cp -f "${QCA_PATH}/lib/qca-qt5/crypto/$p" "$contentsdir/PlugIns/crypto/$p"

                install_name_tool -change "${SSL_PATH}/lib/libcrypto.1.0.0.dylib" "@executable_path/../Frameworks/libcrypto.dylib"    "$contentsdir/PlugIns/crypto/$p"
                install_name_tool -change "${SSL_PATH}/lib/libssl.1.0.0.dylib"    "@executable_path/../Frameworks/libssl.dylib"       "$contentsdir/PlugIns/crypto/$p"
		install_name_tool -change "${LIBS_PATH}/lib/libgcrypt.20.dylib"   "@executable_path/../Frameworks/libgcrypt.dylib"    "$contentsdir/PlugIns/crypto/$p"
                install_name_tool -change "${LIBS_PATH}/lib/libgpg-error.0.dylib" "@executable_path/../Frameworks/libgpg-error.dylib" "$contentsdir/PlugIns/crypto/$p"
	done
}

copy_qt() {
	for f in $QT_FRAMEWORKS; do
		cp -a ${QTDIR}/lib/$f.framework $contentsdir/Frameworks
		cleanup_framework $contentsdir/Frameworks/$f.framework $f ${QT_FRAMEWORK_VERSION}
	done

	for p in $QT_PLUGINS; do
		mkdir -p $contentsdir/PlugIns/$(dirname $p);
		cp -a ${QTDIR}/plugins/$p $contentsdir/PlugIns/$p
	done

	qt_conf_file="$contentsdir/Resources/qt.conf"
	touch ${qt_conf_file}
	echo "[Paths]" >> ${qt_conf_file}
	echo "Plugins = PlugIns" >> ${qt_conf_file}

	install_name_tool -add_rpath "@executable_path/../Frameworks"  "$contentsdir/MacOS/psi"
}

copy_main_libs() {
        cp -f "${LIBS_PATH}/lib/libz.dylib"     "$contentsdir/Frameworks/"
        cp -f "${LIBS_PATH}/lib/libidn.dylib"   "$contentsdir/Frameworks/"
        cp -f "${SSL_PATH}/lib/libssl.dylib"    "$contentsdir/Frameworks/"
        cp -f "${SSL_PATH}/lib/libcrypto.dylib" "$contentsdir/Frameworks/"
        chmod +w "$contentsdir/Frameworks/libssl.dylib"
        chmod +w "$contentsdir/Frameworks/libcrypto.dylib"

        install_name_tool -id "@executable_path/../Frameworks/libz.dylib" "$contentsdir/Frameworks/libz.dylib"
        install_name_tool -id "@executable_path/../Frameworks/libidn.dylib" "$contentsdir/Frameworks/libidn.dylib"
        install_name_tool -id "@executable_path/../Frameworks/libssl.dylib" "$contentsdir/Frameworks/libssl.dylib"
        install_name_tool -change "${SSL_PATH}/lib/libcrypto.1.0.0.dylib" "@executable_path/../Frameworks/libcrypto.dylib" "$contentsdir/Frameworks/libssl.dylib"
        install_name_tool -id "@executable_path/../Frameworks/libcrypto.dylib" "$contentsdir/Frameworks/libcrypto.dylib"

        install_name_tool -change "${LIBS_PATH}/lib/libz.1.dylib"    "@executable_path/../Frameworks/libz.dylib"   "$contentsdir/MacOS/psi"
        install_name_tool -change "${LIBS_PATH}/lib/libidn.11.dylib" "@executable_path/../Frameworks/libidn.dylib" "$contentsdir/MacOS/psi"
}

copy_otrplugins_libs() {
        cp -f "${LIBS_PATH}/lib/libgpg-error.dylib" "$contentsdir/Frameworks/"
        cp -f "${LIBS_PATH}/lib/libgcrypt.dylib"    "$contentsdir/Frameworks/"
        cp -f "${LIBS_PATH}/lib/libotr.dylib"       "$contentsdir/Frameworks/"
        cp -f "${LIBS_PATH}/lib/libtidy.dylib"      "$contentsdir/Frameworks/"

        install_name_tool -id "@executable_path/../Frameworks/libgpg-error.dylib" "$contentsdir/Frameworks/libgpg-error.dylib"
        
	install_name_tool -id "@executable_path/../Frameworks/libgcrypt.dylib" "$contentsdir/Frameworks/libgcrypt.dylib"
	install_name_tool -change "${LIBS_PATH}/lib/libgpg-error.0.dylib" "@executable_path/../Frameworks/libgpg-error.dylib" "$contentsdir/Frameworks/libgcrypt.dylib"
        
	install_name_tool -id "@executable_path/../Frameworks/libotr.dylib" "$contentsdir/Frameworks/libotr.dylib"
        install_name_tool -change "${LIBS_PATH}/lib/libgcrypt.20.dylib" "@executable_path/../Frameworks/libgcrypt.dylib" "$contentsdir/Frameworks/libotr.dylib"
	install_name_tool -change "${LIBS_PATH}/lib/libgpg-error.0.dylib" "@executable_path/../Frameworks/libgpg-error.dylib" "$contentsdir/Frameworks/libotr.dylib"
        
	install_name_tool -id "@executable_path/../Frameworks/libtidy.dylib" "$contentsdir/Frameworks/libtidy.dylib"

        install_name_tool -change "${LIBS_PATH}/lib/libotr.5.dylib"       "@executable_path/../Frameworks/libotr.dylib"       "$contentsdir/Resources/plugins/libotrplugin.dylib"
	install_name_tool -change "${LIBS_PATH}/lib/libgcrypt.20.dylib"   "@executable_path/../Frameworks/libgcrypt.dylib"    "$contentsdir/Resources/plugins/libotrplugin.dylib"
	install_name_tool -change "${LIBS_PATH}/lib/libgpg-error.0.dylib" "@executable_path/../Frameworks/libgpg-error.dylib" "$contentsdir/Resources/plugins/libotrplugin.dylib"
	install_name_tool -change "${LIBS_PATH}/lib/libtidy.5.dylib"      "@executable_path/../Frameworks/libtidy.dylib"      "$contentsdir/Resources/plugins/libotrplugin.dylib"
	install_name_tool -change "libtidy.5.dylib"      "@executable_path/../Frameworks/libtidy.dylib"      "$contentsdir/Resources/plugins/libotrplugin.dylib" #can be this path
}

copy_resources() {	
	cd "${PSI_DIR}"/build/

	contentsdir=${PSI_DIR}/build/${PSI_APP}/Contents
	mkdir "$contentsdir/Frameworks"	

	copy_qt
	copy_qca

	cp -a "${PSI_DIR}/Growl.framework" "$contentsdir/Frameworks"

	cleanup_framework "$contentsdir/Frameworks/Growl.framework" Growl A

	copy_main_libs

        log "Copying plugins..."
	if [ ! -d $contentsdir/Resources/plugins ]; then
    		mkdir -p "$contentsdir/Resources/plugins"
	fi

	for pl in ${PLUGINS}; do
		cd ${PSI_DIR}/build/src/plugins/generic/${pl}
                if [ -f "lib${pl}.dylib" ]; then
 			cp "lib${pl}.dylib" "$contentsdir/Resources/plugins"
		fi
	done

	copy_otrplugins_libs


        log "Copying langpack, web, skins..."
        cd "${PSI_DIR}/build"

	app_bundl_tr_dir="$contentsdir/Resources/translations"
	mkdir "$app_bundl_tr_dir"
	cd "$app_bundl_tr_dir"
	for l in $TRANSLATIONS; do

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

        cd "$contentsdir/Resources/"
	cp -r ${PSI_DIR}/build/sound .
	cp -r ${PSI_DIR}/build/themes .
        cp -r ${PSI_DIR}/build/iconsets .
	cp -r ${PSI_DIR}/resources/sound .
        cp -f ${PSI_DIR}/build/client_icons.txt .
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

copy_resources
make_bundle

finishtime=`date "+Finish time: %H:%M:%S"`
echo $starttime
echo $finishtime
