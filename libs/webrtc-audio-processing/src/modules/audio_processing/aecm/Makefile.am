noinst_LTLIBRARIES = libaecm.la

libaecm_la_SOURCES = interface/echo_control_mobile.h \
		     echo_control_mobile.c \
		     aecm_core.c \
		     aecm_core.h
libaecm_la_CFLAGS = $(AM_CFLAGS) $(COMMON_CFLAGS) \
		    -I$(top_srcdir)/src/common_audio/signal_processing_library/main/interface \
		    -I$(top_srcdir)/src/modules/audio_processing/utility
