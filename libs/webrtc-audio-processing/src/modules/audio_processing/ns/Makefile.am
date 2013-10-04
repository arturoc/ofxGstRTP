noinst_LTLIBRARIES = libns.la libns_fix.la

libns_la_SOURCES = interface/noise_suppression.h \
		   noise_suppression.c \
		   windows_private.h \
		   defines.h \
		   ns_core.c \
		   ns_core.h
libns_la_CFLAGS = $(AM_CFLAGS) $(COMMON_CFLAGS) \
		  -I$(top_srcdir)/src/common_audio/signal_processing_library/main/interface \
		  -I$(top_srcdir)/src/modules/audio_processing/utility

libns_fix_la_SOURCES = interface/noise_suppression_x.h \
		       noise_suppression_x.c \
		       nsx_defines.h \
		       nsx_core.c \
		       nsx_core.h
libns_fix_la_CFLAGS = $(AM_CFLAGS) $(COMMON_CFLAGS) \
		      -I$(top_srcdir)/src/common_audio/signal_processing_library/main/interface \
		      -I$(top_srcdir)/src/modules/audio_processing/utility
