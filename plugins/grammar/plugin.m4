
# Grammar plugin uses the bundled hunspell built in
# thirdparty/hunspell-1.7.0, so no external dependencies are
# needed (beyond enchant for spell).

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
	-I\$(top_srcdir)/thirdparty/hunspell-1.7.0/src/hunspell \
	-DHUNSPELL_STATIC"
GRAMMAR_LIBS=" \
	\$(top_builddir)/thirdparty/libhunspell.la"

test "$enable_grammar" = "auto" && PLUGINS="$PLUGINS grammar"

GRAMMAR_CFLAGS="$GRAMMAR_CFLAGS "'${PLUGIN_CFLAGS}'
GRAMMAR_LIBS="$GRAMMAR_LIBS "'${PLUGIN_LIBS}'

fi

AC_SUBST([GRAMMAR_CFLAGS])
AC_SUBST([GRAMMAR_LIBS])
