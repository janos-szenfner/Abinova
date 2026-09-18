
# WPG plugin uses the bundled libwpg/libwpd/librevenge convenience
# libraries from thirdparty/, so no external dependencies are needed.

wpg_deps="yes"
WPG_CFLAGS=
WPG_LIBS=

if test "$enable_wpg" != ""; then

if test "$enable_wpg_builtin" = "yes"; then
AC_MSG_ERROR([wpg plugin: static linking not supported])
fi

WPG_CFLAGS=" \
	-I\$(top_srcdir)/thirdparty/libwpg-0.3.4/inc \
	-I\$(top_srcdir)/thirdparty/libwpd-0.10.3/inc \
	-I\$(top_srcdir)/thirdparty/librevenge-0.0.6/inc"
WPG_LIBS=" \
	\$(top_builddir)/thirdparty/libwpg.la \
	\$(top_builddir)/thirdparty/libwpd.la \
	\$(top_builddir)/thirdparty/librevenge.la \
	-lz"

test "$enable_wpg" = "auto" && PLUGINS="$PLUGINS wpg"

WPG_CFLAGS="$WPG_CFLAGS "'${PLUGIN_CFLAGS}'
WPG_LIBS="$WPG_LIBS "'${PLUGIN_LIBS}'

fi

AC_SUBST([WPG_CFLAGS])
AC_SUBST([WPG_LIBS])
