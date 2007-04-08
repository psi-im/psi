AC_INIT([libjingle], [0.3.0], [google-talk-open@googlegroups.com])
AC_CANONICAL_SYSTEM
AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE([dist-zip])
AC_PROG_CC
AC_PROG_CXX
AM_PROG_LIBTOOL
LIBTOOL="$LIBTOOL --silent"
AC_PROG_INSTALL
AC_DEFINE(PRODUCTION_BUILD, 1, [Build as a production build])
AC_DEFINE(PRODUCTION, 1, [Build as a production build])
AC_DEFINE(POSIX, 1, [If we're using configure, we're on POSIX])
AC_DEFINE(FEATURE_ENABLE_SSL, 1, [Enable SSL])
AC_DEFINE(FEATURE_ENABLE_CHAT_ARCHIVING, 1, [Chat archving])

HAVE_EXPAT=no
AC_CHECK_LIB(expat, XML_ParserCreate, HAVE_EXPAT="yes")
if test "x$HAVE_EXPAT" = xyes ; then
  EXPAT_LIBS="-lexpat"
  AC_SUBST(EXPAT_LIBS)
else
  AC_ERROR([Expat is required to build libjingle. You can get it from http://expat.sourceforge.net/])
fi

if test `uname -s` = Linux ; then
  AC_DEFINE(LINUX, 1, [Building on Linux])
  if test x`ls talk/third_party/gips/VoiceEngine_Linux_gcc.a` != x ; then
    enable_gips=yes
    MEDIA_LIBS=$PWD/talk/third_party/gips/VoiceEngine_Linux_gcc.a
  fi 
elif test `uname -s` = Darwin ; then
  AC_DEFINE(OSX, 1, [Building on OSX])
  if test x`ls talk/third_party/gips/VoiceEngine_mac_gcc.a` != x ; then
    enable_gips=yes
    MEDIA_LIBS="$PWD/talk/third_party/gips/VoiceEngine_mac_gcc.a -framework CoreAudio -framework AudioToolbox"
  fi
fi

if test x$enable_gips = xyes ; then
  AC_DEFINE(HAVE_GIPS, 1, [Have GIPS Voice Engine])
elif test `uname -s` = Linux ; then

AC_CHECK_HEADERS(alsa/asoundlib.h,
	[AC_CHECK_LIB(asound, snd_pcm_open,
		[ALSA_LIBS="-lasound" ; AC_DEFINE(__ALSA_ENABLED__,1,[Defined when alsa support is enabled]) ])
	]
)
AC_SUBST(ALSA_LIBS)

PKG_CHECK_MODULES(GLIB, glib-2.0 gmodule-2.0 gthread-2.0, enable_glib=yes, enable_glib=no)
if test x$enable_glib = xno; then
	AC_ERROR([GLib 2.0 is required to build libjingle. You can get it from http://www.gtk.org/])
fi
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)
AC_DEFINE(HAVE_GLIB, 1, [Glib is required for oRTP code])

PKG_CHECK_MODULES(ORTP, ortp = 0.7.1, enable_ortp=yes, enable_ortp=no)
if test x$enable_ortp = xno ; then
      AC_ERROR([oRTP is required to build libjingle. You can get it from http://www.linphone.org/ortp/])
fi
AC_SUBST(ORTP_CFLAGS)
AC_SUBST(ORTP_LIBS)

MEDIA_LIBS="$PWD/talk/third_party/mediastreamer/libmediastreamer.la -lasound"

dnl only accept speex>=1.1.6 or 1.0.5 (the versions that have speex_encode_int )
AC_ARG_WITH( speex,
      [  --with-speex      Set prefix where speex lib can be found (ex:/usr, /usr/local) [default=/usr] ],
      [ speex_prefix=${withval}],[ speex_prefix="/usr" ])

AC_CHECK_HEADERS(speex.h,[AC_CHECK_LIB(speex,speex_encode_int,speex_found=yes,speex_found=no)
],speex_found=no)

if test "$speex_found" = "no" ; then
AC_MSG_WARN([Could not find a libspeex version that have the speex_encode_int() function. Please install libspeex=1.0.5 or libspeex>=1.1.6 from http://www.speex.org/])
else
SPEEX_CFLAGS=" -I${speex_prefix}/include -I${speex_prefix}/include/speex"
SPEEX_LIBS="-L${speex_prefix}/lib -lspeex -lm"
CPPFLAGS_save=$CPPFLAGS
CPPFLAGS=$SPEEX_CFLAGS
LDFLAGS_save=$LDFLAGS
LDFLAGS=$SPEEX_LIBS
AC_DEFINE(HAVE_SPEEX,1,[has speex])
fi

AC_SUBST(SPEEX_CFLAGS)
AC_SUBST(SPEEX_LIBS)
CPPFLAGS=$CPPFLAGS_save
LDFLAGS=$LDFLAGS_save

AC_ARG_WITH( ilbc,
      [  --with-ilbc      Set prefix where ilbc headers and libs can be found (ex:/usr, /usr/local, none to disable ilbc support) [default=/usr] ],
      [ ilbc_prefix=${withval}],[ ilbc_prefix="/usr" ])

if test "$ilbc_prefix" = "none" ; then
        AC_MSG_NOTICE([iLBC codec support disabled. ])
else
        ILBC_CFLAGS=" -I${ilbc_prefix}/include/ilbc"
        ILBC_LIBS="-L${ilbc_prefix}/lib -lilbc -lm"
        CPPFLAGS_save=$CPPFLAGS
        CPPFLAGS=$ILBC_CFLAGS
        LDFLAGS_save=$LDFLAGS
        LDFLAGS=$ILBC_LIBS
        AC_CHECK_HEADERS(iLBC_decode.h,[AC_CHECK_LIB(ilbc,iLBC_decode,ilbc_found=yes,ilbc_found=no)
        ],ilbc_found=no)

        CPPFLAGS=$CPPFLAGS_save
        LDFLAGS=$LDFLAGS_save

        if test "$ilbc_found" = "no" ; then
                AC_MSG_WARN([Could not find ilbc headers or libs. Please install ilbc package from http://www.linphone.org if you want iLBC codec support in libjingle.])
                ILBC_CFLAGS=
                ILBC_LIBS=
        else
                AC_DEFINE(HAVE_ILBC,1,[Defined when we have ilbc codec lib])
                AC_SUBST(ILBC_CFLAGS)
                AC_SUBST(ILBC_LIBS)
        fi
fi

else
  enable_sound=no
fi  #  Linux check

AM_CONDITIONAL(GIPS, test x$enable_gips = xyes)
if test x$enable_sound != xno ; then
  # don't build examples for cygwin
  EXAMPLES_DIR=examples
  AC_SUBST([EXAMPLES_DIR])
fi
AC_SUBST(MEDIA_LIBS)

AC_OUTPUT([Makefile
	  talk/Makefile
          talk/base/Makefile
	  talk/third_party/Makefile
          talk/third_party/gips/Makefile
	  talk/third_party/mediastreamer/Makefile
	  talk/examples/Makefile
	  talk/examples/login/Makefile
	  talk/examples/call/Makefile
          talk/examples/pcp/Makefile
	  talk/p2p/Makefile
	  talk/p2p/base/Makefile
    	  talk/p2p/client/Makefile
          talk/session/Makefile
          talk/session/tunnel/Makefile
          talk/session/phone/Makefile
          talk/xmllite/Makefile
	  talk/xmpp/Makefile
	])

echo
echo $PACKAGE $VERSION
echo
echo Supported Codecs:
if test x$enable_gips = xyes ; then
echo GIPS: yes
else
echo Speex: $speex_found
echo iLBC:  $ilbc_found
echo MULAW: yes
fi
echo
