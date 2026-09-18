
# WordPerfect plugin uses the bundled libwpd/libwps/librevenge
# convenience libraries from thirdparty/, so no external
# dependencies are needed.

wordperfect_deps="yes"
WORDPERFECT_CFLAGS=
WORDPERFECT_LIBS=
WPS_DEFINE=" -DHAVE_LIBWPS"

if test "$enable_wordperfect" != ""; then

if test "$enable_wordperfect_builtin" = "yes"; then
AC_MSG_ERROR([wordperfect plugin: static linking not supported])
fi

WORDPERFECT_CFLAGS=" \
	-I\$(top_srcdir)/thirdparty/libwpd-0.10.3/inc \
	-I\$(top_srcdir)/thirdparty/libwps-0.4.11/inc \
	-I\$(top_srcdir)/thirdparty/librevenge-0.0.6/inc"
WORDPERFECT_LIBS=" \
	\$(top_builddir)/thirdparty/libwps.la \
	\$(top_builddir)/thirdparty/libwpd.la \
	\$(top_builddir)/thirdparty/librevenge.la \
	-lz"

test "$enable_wordperfect" = "auto" && PLUGINS="$PLUGINS wordperfect"

WORDPERFECT_CFLAGS="$WORDPERFECT_CFLAGS "'${PLUGIN_CFLAGS}'"$WPS_DEFINE"
WORDPERFECT_LIBS="$WORDPERFECT_LIBS "'${PLUGIN_LIBS}'

fi

AC_SUBST([WORDPERFECT_CFLAGS])
AC_SUBST([WORDPERFECT_LIBS])
