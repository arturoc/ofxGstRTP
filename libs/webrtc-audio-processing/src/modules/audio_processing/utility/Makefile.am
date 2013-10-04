noinst_LTLIBRARIES = libapm_util.la

libapm_util_la_SOURCES = delay_estimator_float.c \
			 delay_estimator_float.h \
			 delay_estimator.c \
			 delay_estimator.h \
			 fft4g.c \
			 fft4g.h \
			 ring_buffer.c \
			 ring_buffer.h
libapm_util_la_CFLAGS = $(AM_CFLAGS) $(COMMON_CFLAGS) \
			-I$(top_srcdir)/src/common_audio/signal_processing_library/main/interface
