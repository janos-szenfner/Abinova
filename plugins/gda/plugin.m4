
gda_pkgs='libgda >= 1.2.0 libgnomedb >= 1.2.0'
gda_deps="no"

if test "$enable_gda" != ""; then

PKG_CHECK_EXISTS([ $gda_pkgs ], 
[
	gda_deps="yes"
], [
	test "$enable_gda" = "auto" && AC_MSG_WARN([gda plugin: dependencies not satisfied - $gda_pkgs])
])

fi

if test "$enable_gda" = "yes" || \
   test "$gda_deps" = "yes"; then

if test "$enable_gda_builtin" = "yes"; then
AC_MSG_ERROR([gda plugin: static linking not supported])
fi

PKG_CHECK_MODULES(GDA,[ $gda_pkgs ])

test "$enable_gda" = "auto" && PLUGINS="$PLUGINS gda"

GDA_CFLAGS="$GDA_CFLAGS "'${PLUGIN_CFLAGS}'
GDA_LIBS="$GDA_LIBS "'${PLUGIN_LIBS}'

fi

AC_SUBST([GDA_CFLAGS])
AC_SUBST([GDA_LIBS])

