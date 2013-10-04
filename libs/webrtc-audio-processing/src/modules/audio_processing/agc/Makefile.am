noinst_LTLIBRARIES = libagc.la

libagc_la_SOURCES = interface/gain_control.h \
		    analog_agc.c \
		    analog_agc.h \
		    digital_agc.c \
		    digital_agc.h
libagc_la_CFLAGS = $(AM_CFLAGS) $(COMMON_CFLAGS) \
		   -I$(top_srcdir)/src/common_audio/signal_processing_library/main/interface \
		   -I$(top_srcdir)/src/modules/audio_processing/utility
