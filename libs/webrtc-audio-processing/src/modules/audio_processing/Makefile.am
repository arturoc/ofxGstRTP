SUBDIRS = utility ns aec aecm agc
lib_LTLIBRARIES = libwebrtc_audio_processing.la

if NS_FIXED
COMMON_CXXFLAGS += -DWEBRTC_NS_FIXED=1
NS_LIB = libns_fix
else
COMMON_CXXFLAGS += -DWEBRTC_NS_FLOAT=1
NS_LIB = libns
endif

webrtcincludedir = $(includedir)/webrtc_audio_processing
webrtcinclude_HEADERS = $(top_srcdir)/src/typedefs.h \
			$(top_srcdir)/src/modules/interface/module.h \
			interface/audio_processing.h \
			$(top_srcdir)/src/common_types.h \
			$(top_srcdir)/src/modules/interface/module_common_types.h

libwebrtc_audio_processing_la_SOURCES = interface/audio_processing.h \
					audio_buffer.cc \
					audio_buffer.h \
					audio_processing_impl.cc \
					audio_processing_impl.h \
					echo_cancellation_impl.cc \
					echo_cancellation_impl.h \
					echo_control_mobile_impl.cc \
					echo_control_mobile_impl.h \
					gain_control_impl.cc \
					gain_control_impl.h \
					high_pass_filter_impl.cc \
					high_pass_filter_impl.h \
					level_estimator_impl.cc \
					level_estimator_impl.h \
					noise_suppression_impl.cc \
					noise_suppression_impl.h \
					splitting_filter.cc \
					splitting_filter.h \
					processing_component.cc \
					processing_component.h \
					voice_detection_impl.cc \
					voice_detection_impl.h
libwebrtc_audio_processing_la_CXXFLAGS = $(AM_CXXFLAGS) $(COMMON_CXXFLAGS) \
				       -I$(top_srcdir)/src/common_audio/signal_processing_library/main/interface \
				       -I$(top_srcdir)/src/common_audio/vad/main/interface \
				       -I$(top_srcdir)/src/system_wrappers/interface \
				       -I$(top_srcdir)/src/modules/audio_processing/utility \
				       -I$(top_srcdir)/src/modules/audio_processing/ns/interface \
				       -I$(top_srcdir)/src/modules/audio_processing/aec/interface \
				       -I$(top_srcdir)/src/modules/audio_processing/aecm/interface \
				       -I$(top_srcdir)/src/modules/audio_processing/agc/interface
libwebrtc_audio_processing_la_LIBADD = $(top_builddir)/src/system_wrappers/libsystem_wrappers.la \
				       $(top_builddir)/src/common_audio/signal_processing_library/libspl.la \
				       $(top_builddir)/src/common_audio/vad/libvad.la \
				       $(top_builddir)/src/modules/audio_processing/utility/libapm_util.la \
				       $(top_builddir)/src/modules/audio_processing/ns/$(NS_LIB).la \
				       $(top_builddir)/src/modules/audio_processing/aec/libaec.la \
				       $(top_builddir)/src/modules/audio_processing/aecm/libaecm.la \
				       $(top_builddir)/src/modules/audio_processing/agc/libagc.la
libwebrtc_audio_processing_la_LDFLAGS = $(AM_LDFLAGS) -version-info $(LIBWEBRTC_AUDIO_PROCESSING_VERSION_INFO)
