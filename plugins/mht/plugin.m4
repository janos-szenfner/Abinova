
#
# Optional packages
#

AC_ARG_WITH([libtidy], 
	[AS_HELP_STRING([--with-libtidy], [MHT plugin: clean up HTML before importing using libtidy])], 
[
	mht_cv_libtidy="$withval"
],[
	mht_cv_libtidy="auto"
])

# gsf pulls in libxml, so we are ok
mht_pkgs="$gsf_req"
mht_deps="no"

if test "$enable_mht" != ""; then

PKG_CHECK_EXISTS([ $mht_pkgs ], 
[
	mht_deps="yes"
], [
	test "$enable_mht" = "auto" && AC_MSG_WARN([mht plugin: dependencies not satisfied - $mht_pkgs])
])

fi

if test "$enable_mht" = "yes" || \
   test "$mht_deps" = "yes"; then

test "$enable_mht" = "auto" && PLUGINS="$PLUGINS mht"

if test "$enable_mht_builtin" = "yes"; then
AC_MSG_ERROR([mht plugin: static linking not supported])
fi

#
# Tests
#

AC_CHECK_HEADERS([tidy/tidy.h],
[
	libtidy_found="yes"
], [
	libtidy_found="no"
])

#
# Settings
#

if test "$mht_cv_libtidy" = "yes" &&
   test "$libtidy_found" = "no"; then
	AC_MSG_ERROR([MHT plugin: error - libtidy headers not found])
elif test "$mht_cv_libtidy" = "auto"; then
	mht_cv_libtidy="$libtidy_found"
fi
if test "$mht_cv_libtidy" = "yes"; then
	MHT_OPT_LIBS="$MHT_OPT_LIBS -ltidy"
	MHT_OPT_CFLAGS="$MHT_OPT_CFLAGS -DXHTML_HTML_TIDY_SUPPORTED"
else
	MHT_OPT_CFLAGS="$MHT_OPT_CFLAGS -DXHTML_HTML_XML2_SUPPORTED"
fi

# the MHTML importer is self-contained, no external MIME library needed
MHT_OPT_CFLAGS="$MHT_OPT_CFLAGS -DXHTML_MULTIPART_SUPPORTED"

PKG_CHECK_MODULES(MHT,[ $mht_pkgs ])

MHT_CFLAGS="$MHT_CFLAGS $MHT_OPT_CFLAGS "'${PLUGIN_CFLAGS}'
MHT_LIBS="$MHT_LIBS $MHT_OPT_LIBS "'${PLUGIN_LIBS}'

fi

AC_SUBST([MHT_CFLAGS])
AC_SUBST([MHT_LIBS])

AM_CONDITIONAL([ABI_XHTML_XML2], test /bin/true)
AM_CONDITIONAL([ABI_XHTML_MHT], test /bin/true)
AM_CONDITIONAL([ABI_XHTML_TIDY], test "$mht_cv_libtidy" = "yes")
