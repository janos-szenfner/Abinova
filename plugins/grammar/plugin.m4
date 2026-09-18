
# Grammar plugin uses the bundled link-grammar built in
# thirdparty/link-grammar-5.12.5 via AC_CONFIG_SUBDIRS, so no
# external dependencies are needed (beyond enchant for spell).

grammar_deps="yes"
GRAMMAR_CFLAGS=
GRAMMAR_LIBS=

dnl make sure we enable grammar only if spell is enabled. At least in auto mode.
if test "$enable_grammar" != "" && test  "$abi_cv_spell" = "yes"; then

if test "$enable_grammar_builtin" = "yes"; then
AC_MSG_ERROR([grammar plugin: static linking not supported])
fi

if test "$abi_cv_spell" = "no"; then
AC_MSG_ERROR([grammar plugin: spell checking needs to be enabled])
fi

GRAMMAR_CFLAGS=" \
	-I\$(top_srcdir)/thirdparty/link-grammar-5.12.5"
GRAMMAR_LIBS=" \
	\$(top_builddir)/thirdparty/link-grammar-5.12.5/link-grammar/liblink-grammar.la"

dnl bundled link-grammar 5.12.5 is newer than 5.1.0
AC_DEFINE([HAVE_LINK_GRAMMAR_51],[1],["have link-grammar 5.1.0 or later"])

test "$enable_grammar" = "auto" && PLUGINS="$PLUGINS grammar"

GRAMMAR_CFLAGS="$GRAMMAR_CFLAGS "'${PLUGIN_CFLAGS}'
GRAMMAR_LIBS="$GRAMMAR_LIBS "'${PLUGIN_LIBS}'

fi

AC_SUBST([GRAMMAR_CFLAGS])
AC_SUBST([GRAMMAR_LIBS])
