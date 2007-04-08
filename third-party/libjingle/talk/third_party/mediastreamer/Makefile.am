noinst_LTLIBRARIES = libmediastreamer.la
libmediastreamer_la_SOURCES=msfilter.c msfilter.h msutils.h waveheader.h\
							mscodec.c mscodec.h \
							mssoundread.c mssoundread.h \
							mssoundwrite.c mssoundwrite.h \
							msbuffer.c msbuffer.h  \
							msqueue.c msqueue.h \
							msfifo.c msfifo.h \
							ms.c ms.h\
							mssync.c mssync.h \
							msnosync.c msnosync.h \
							msread.c msread.h \
							mswrite.c mswrite.h \
							mscopy.c mscopy.h \
							msosswrite.c msosswrite.h  \
							msossread.c msossread.h \
							msringplayer.c msringplayer.h \
							msrtprecv.c msrtprecv.h   \
							msrtpsend.c msrtpsend.h	\
							msAlawenc.c msAlawenc.h g711common.h \
							msAlawdec.c msAlawdec.h g711common.h \
							msMUlawenc.c msMUlawenc.h g711common.h \
							msMUlawdec.c msMUlawdec.h g711common.h \
							mstimer.c mstimer.h \
							msqdispatcher.c msqdispatcher.h \
							msfdispatcher.c msfdispatcher.h \
							sndcard.c sndcard.h \
							osscard.c osscard.h\
							hpuxsndcard.c \
							alsacard.c alsacard.h \
							jackcard.c jackcard.h \
							audiostream.c mediastream.h \
							msspeexenc.c msspeexenc.h msspeexdec.c msspeexdec.h \
							msilbcdec.c msilbcdec.h msilbcenc.c msilbcenc.h 

noinst_HEADERS = affine.h \
			      msAlawenc.h \
			      msfdispatcher.h \
			      msilbcdec.h \
			      msnosync.h \
			      msringplayer.h \
			      msspeexdec.h \
		    	      msutils.h \
		       	      waveheader.h \
			      alsacard.h \
			      msavdecoder.h \
			      msfifo.h \
		      	      msilbcenc.h \
			      msossread.h \
			      msrtprecv.h \
			      msspeexenc.h \
		    	      msv4l.h \
			      g711common.h \
			      msavencoder.h \
			      msfilter.h \
			      msLPC10decoder.h \
			      msosswrite.h \
			      msrtpsend.h \
			      mssync.h \
			      msvideosource.h \
                              jackcard.h \
			      msbuffer.h \
			      msGSMdecoder.h \
			      msLPC10encoder.h \
			      msqdispatcher.h \
			      mssdlout.h \
			      mstimer.h \
		 	      mswrite.h \
	                      mediastream.h \
			      mscodec.h \
			      msGSMencoder.h \
			      msMUlawdec.h \
			      msqueue.h \
		       	      mssoundread.h \
			      mstruespeechdecoder.h \
			      osscard.h \
                              msAlawdec.h \
			      mscopy.h \
			      ms.h \
		  	      msMUlawenc.h \
			      msread.h \
		      	      mssoundwrite.h \
			      mstruespeechencoder.h \
			      sndcard.h


libmediastreamer_la_LIBADD= $(GLIB_LIBS) $(ORTP_LIBS)

AM_CFLAGS=$(GLIB_CFLAGS) -DG_LOG_DOMAIN=\"MediaStreamer\" $(ORTP_CFLAGS) $(IPV6_CFLAGS) $(ILBC_CFLAGS) $(SPEEX_CFLAGS)

INCLUDES=  -I$(top_srcdir) $(ORTP_CFLAGS) 
