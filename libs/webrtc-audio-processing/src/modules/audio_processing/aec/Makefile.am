noinst_LTLIBRARIES = libaec.la

libaec_la_SOURCES = interface/echo_cancellation.h \
		    echo_cancellation.c \
		    aec_core.h \
		    aec_core.c \
		    aec_core_sse2.c \
		    aec_rdft.h \
		    aec_rdft.c \
		    aec_rdft_sse2.c \
		    resampler.h \
		    resampler.c
libaec_la_CFLAGS = $(AM_CFLAGS) $(COMMON_CFLAGS) \
		   -I$(top_srcdir)/src/common_audio/signal_processing_library/main/interface \
		   -I$(top_srcdir)/src/system_wrappers/interface \
		   -I$(top_srcdir)/src/modules/audio_processing/utility
