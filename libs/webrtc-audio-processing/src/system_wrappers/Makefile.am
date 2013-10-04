noinst_LTLIBRARIES = libsystem_wrappers.la

libsystem_wrappers_la_SOURCES = interface/cpu_features_wrapper.h \
				interface/critical_section_wrapper.h \
				source/cpu_features.cc \
				source/critical_section.cc \
				source/critical_section_posix.h \
				source/critical_section_windows.h
# This assumes that we want the POSIX implementation -- should eventually be
# converted to a conditional to include Windows support
libsystem_wrappers_la_SOURCES += source/critical_section_posix.cc
libsystem_wrappers_la_CXXFLAGS = $(AM_CXXFLAGS) $(COMMON_CXXFLAGS) \
				 -I$(srcdir)/interface
