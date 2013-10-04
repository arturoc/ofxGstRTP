# Revision changelog (version - date, svn rev. from upstream that was merged)
#    0.1 - 21 Oct 2011, r789
AC_INIT([webrtc-audio-processing], [0.1])
AM_INIT_AUTOMAKE([dist-xz tar-ustar])

AC_SUBST(LIBWEBRTC_AUDIO_PROCESSING_VERSION_INFO, [0:0:0])

AM_SILENT_RULES([yes])

AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL
AC_PROG_INSTALL

AC_LANG_C
AC_LANG_CPLUSPLUS

AC_ARG_WITH([ns-mode],
	    AS_HELP_STRING([--with-ns-mode=float|fixed], [Noise suppresion mode to use. Default is float]))

AS_CASE(["x${with_ns_mode}"],
	["fixed"], [NS_FIXED=1],
	["float"], [NS_FIXED=0],
	[NS_FIXED=0])
AM_CONDITIONAL(NS_FIXED, [test "x${NS_FIXED}" = "x1"])

COMMON_CFLAGS="-DNDEBUG -I\$(srcdir)/interface -I\$(srcdir)/main/interface -I\$(top_srcdir)/src -I\$(top_srcdir)/src/modules/interface"
COMMON_CXXFLAGS="-DNDEBUG -I\$(srcdir)/interface -I\$(srcdir)/main/interface -I\$(top_srcdir)/src -I\$(top_srcdir)/src/modules/interface"
AC_SUBST([COMMON_CFLAGS])
AC_SUBST([COMMON_CXXFLAGS])

AC_CONFIG_FILES([
webrtc-audio-processing.pc
Makefile
src/Makefile
src/common_audio/Makefile
src/common_audio/signal_processing_library/Makefile
src/common_audio/vad/Makefile
src/system_wrappers/Makefile
src/modules/Makefile
src/modules/audio_processing/Makefile
src/modules/audio_processing/utility/Makefile
src/modules/audio_processing/ns/Makefile
src/modules/audio_processing/aec/Makefile
src/modules/audio_processing/aecm/Makefile
src/modules/audio_processing/agc/Makefile
])

AC_OUTPUT
