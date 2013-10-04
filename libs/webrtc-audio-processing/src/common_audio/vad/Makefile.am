noinst_LTLIBRARIES = libvad.la

libvad_la_SOURCES = main/interface/webrtc_vad.h \
		    main/source/webrtc_vad.c \
		    main/source/vad_core.c \
		    main/source/vad_core.h \
		    main/source/vad_defines.h \
		    main/source/vad_filterbank.c \
		    main/source/vad_filterbank.h \
		    main/source/vad_gmm.c \
		    main/source/vad_gmm.h \
		    main/source/vad_sp.c \
		    main/source/vad_sp.h
libvad_la_CFLAGS = $(AM_CFLAGS) $(COMMON_CFLAGS) \
		   -I$(top_srcdir)/src/common_audio/signal_processing_library/main/interface
