
m4_define([abi_version_major], [4])
m4_define([abi_version_minor], [0])
m4_define([abi_version_micro], [0])
m4_define([abi_series], [abi_version_major.abi_version_minor])
m4_define([abi_version], [abi_version_major.abi_version_minor.abi_version_micro])

AC_INIT([abinova],[abi_version],[https://github.com/janos-szenfner/Exp-Abi])
AC_CANONICAL_HOST
AC_CONFIG_HEADERS(config.h)
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.9 tar-ustar dist-bzip2 subdir-objects foreign])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

dnl we use AM_MAINTAINER_MODE and enable it to allow to disable it.
dnl very usefull to build in scratchbox where the maintainer mode is used for the PC
dnl build but the embedded build just use pregenerated one (with a different version)
AM_MAINTAINER_MODE([enable])

#
# Dependency requirements
#

#
# versions requested.
#
GTK_VERSION_REQ=4.6.0
GTK_ENCODED_VERSION="GDK_VERSION_4_6"

# This check MUST be done early.
#
# We define EMBEDDED_TARGET to a numerical value so that we can do like
# #if EMBEDDED_TARGET = EMBEDDED_TARGET_POKY
#
# The following have to be defined unconditionally, because we want 
# EMBEDDED_TARGET to be undefined if not building for embedded so that we 
# can use #ifdef and #ifndef on it (two undefined symbols will return 
# true if tested for equality in #if construct)
AC_DEFINE([EMBEDDED_TARGET_GENERIC], [1], [Generic embedded platform])
AC_DEFINE([EMBEDDED_TARGET_POKY],    [3], [Poky embedded platform])

AC_ARG_ENABLE([embedded],
	[AS_HELP_STRING([--enable-embedded], [=generic|poky, (Gtk+ only), build for embedded platform])],
[
	abi_cv_embedded_platform="$enableval"
	case "$enableval" in
	poky)
		abi_cv_embedded_target=EMBEDDED_TARGET_POKY ;;
	generic)
		abi_cv_embedded_target=EMBEDDED_TARGET_GENERIC ;;
	yes)
		abi_cv_embedded_target=EMBEDDED_TARGET_GENERIC 
		abi_cv_embedded_platform="generic" ;;
	no)
		;;
	*)
		AC_MSG_ERROR([bad value $enableval for --enable-embedded]) ;;
	esac
],[
	abi_cv_embedded_platform="no"
	abi_cv_embedded_target="no"
])

#
# setting the deps requirement is done further down
#

# cross platform deps
fribidi_req='fribidi >= 0.10.4'
glib_req='glib-2.0 >= 2.74.0 gthread-2.0 >= 2.74.0 gobject-2.0 >= 2.74.0'
gsf_req='libgsf-1 >= 1.14.26'
zlib_req='zlib'

xslt_req='libxslt'
xp_pkgs="
  ${fribidi_req}
  ${glib_req}
  ${gsf_req}
  ${zlib_req}
  ${xslt_req}
"

# optional deps
enchant_req='enchant-2'
gio_req='gio-2.0'

dnl libosso-gsf-1'
# placeholder for accumulated optional deps
opt_pkgs=''

# gtk deps
cairo_req='cairo-pdf cairo-ps pangocairo'
#
gtk_req="gtk4 >= $GTK_VERSION_REQ gtk4-unix-print librsvg-2.0 >= 2.46.0 pango >= 1.44"
dnl cairo-fc is needed but only available on cairo > 1.10
dnl http://bugzilla.abisource.com/show_bug.cgi?id=13265
PKG_CHECK_EXISTS(cairo >= 1.10, [gtk_req="$gtk_req cairo-fc"],[])
gtk_pkgs="
  ${cairo_req}
  ${gtk_req}
"

#
# System tests
#

AC_PROG_CC
AC_PROG_CXX
AX_CXX_COMPILE_STDCXX_17(noext,mandatory)
AC_PROG_INSTALL
LT_INIT([disable-static])
AC_PROG_LN_S

AC_CHECK_PROG(HAVE_PERL, perl, yes, no)
if test "x$HAVE_PERL" = "xno"; then
	AC_MSG_ERROR([*** perl program not found])
fi


PLATFORM="unix"
TOOLKIT="gtk"
AC_SUBST([PLATFORM])
AC_DEFINE_UNQUOTED(PLATFORM, "$PLATFORM", The platform that is compiled for)
AC_SUBST([TOOLKIT])
AC_DEFINE_UNQUOTED(TOOLKIT, "$TOOLKIT", The toolkit that is used)

# Platform-specific tests

function_err='Your operating system or setup seems to be missing one or more required functions'
header_err='Your operating system or setup seems to be missing one or more required header files'

dnl "sqrt" test fails, hmm
AC_CHECK_FUNCS([localeconv strcspn strncasecmp strtoul],
[], [
	AC_MSG_ERROR([$function_err])
])

AC_CHECK_HEADERS([sys/time.h], [], [$header_err])
AC_FUNC_STRTOD
AC_TYPE_UINT32_T
AC_CHECK_FUNCS([alarm gettimeofday],
[], [
	AC_MSG_ERROR([$function_err])
])

#
# Features
#


AC_ARG_ENABLE([print], 
	[AS_HELP_STRING([--disable-print], [(Gtk+ only) do not include printing support])], 
[
	abi_cv_print="$enableval"
],[
	abi_cv_print="yes"
])

if test "$abi_cv_print" = "yes"; then
	opt_pkgs="$opt_pkgs"
fi

AC_ARG_ENABLE([spell], 
	[AS_HELP_STRING([--disable-spell], [(Gtk+ only) do not include spell checking support])], 
[
	abi_cv_spell="$enableval"
],[
	# autodetect
	PKG_CHECK_EXISTS([ $enchant_req ],
	[
		abi_cv_spell="yes"
	], [
		abi_cv_spell="no"
	])
])

if test "$abi_cv_spell" = "yes"; then
	opt_pkgs="$opt_pkgs $enchant_req"
fi

AC_ARG_ENABLE([statusbar], 
	[AS_HELP_STRING([--disable-statusbar], [(Gtk+ only) do not include status bar])], 
[
	abi_cv_statusbar="$enableval"
],[
	# on by default
	abi_cv_statusbar="yes"
])

AC_ARG_ENABLE([emacs-keybinding], 
	[AS_HELP_STRING([--enable-emacs-keybinding], [Enable the use of emacs-compatible keyboard commands])], 
[
	abi_cv_emacs_keybinding="$enableval"
], [
	abi_cv_emacs_keybinding="yes"
])

AC_ARG_ENABLE([vi-keybinding], 
	[AS_HELP_STRING([--enable-vi-keybinding], [Enable the use of vi-compatible keyboard commands])], 
[
	abi_cv_vi_keybinding="$enableval"
], [
	abi_cv_vi_keybinding="yes"
])

AC_ARG_ENABLE([clipart], 
	[AS_HELP_STRING([--enable-clipart], [Install clipart])], 
[
	abi_cv_clipart="$enableval"
], [
	abi_cv_clipart="no"
])

AC_ARG_ENABLE([templates], 
	[AS_HELP_STRING([--enable-templates], [Install additional templates])], 
[
	abi_cv_templates="$enableval"
], [
	abi_cv_templates="no"
])

AC_ARG_ENABLE([debug], 
	[AS_HELP_STRING([--enable-debug], [Enable debugging functionality, verbose terminal output])], 
[
	if test "$enableval" = "yes"; then
		abi_cv_debug="yes"
	fi
], [
	abi_cv_debug="no"
])

#
# Optional packages
#

AC_ARG_WITH([gio], 
	[AS_HELP_STRING([--with-gio], [use GIO library])], 
[
	abi_cv_gio="$withval"
],[
	# use gio if detected
	PKG_CHECK_EXISTS([ $gio_req ],
	[
		abi_cv_gio="yes"
	], [
		abi_cv_gio="no"
	])
])

if test "$abi_cv_gio" = "yes"; then
	opt_pkgs="$opt_pkgs $gio_req"
fi

AC_ARG_WITH([darwinports],
	[AS_HELP_STRING([--with-darwinports], [(Mac OSX only) add `/opt/local' prefix to CPP/LDFLAGS])], 
[
        if test "x$withval" != "xno"; then
            CPPFLAGS="$CPPFLAGS -I/opt/local/include"
            LDFLAGS="$LDFLAGS -headerpad_max_install_names -L/opt/local/lib"
        fi
])

AC_ARG_WITH([fink],
	[AS_HELP_STRING([--with-fink], [(Mac OSX only) add `/sw' prefix to CPP/LDFLAGS])], 
[
        if test "x$withval" != "xno"; then
                CPPFLAGS="$CPPFLAGS -I/sw/include"
                LDFLAGS="$LDFLAGS -L/sw/lib"
        fi
])

AC_ARG_WITH([abisdk],
	[AS_HELP_STRING([--with-abisdk], [(Mac OSX only) use the AbiSDK])], 
[
	if test "x$withval" = "x"; then
		AC_MSG_ERROR([The AbiSDK path must be specified.])
	fi
	if test "x$withval" != "xno"; then
		PKG_CONFIG=/opt/abi/bin/pkg-config
		PKG_CONFIG_PATH=/usr/lib/pkgconfig ; export PKG_CONFIG_PATH
		CPPFLAGS="$CPPFLAGS -mmacosx-version-min=10.4 -isysroot $withval -I/opt/abi/include"
		LDFLAGS="$LDFLAGS -mmacosx-version-min=10.4 -isysroot $withval -L/opt/abi/lib"
	fi
])


redland_req='redland >= 1.0.10 rasqal >= 0.9.17'
AC_ARG_WITH([redland], 
	[AS_HELP_STRING([--with-redland], [use redland and raptor libraries])], 
[
	abi_cv_redland="$withval"
],[
	# use redland if detected
	PKG_CHECK_EXISTS([ $redland_req ],
	[
		abi_cv_redland="yes"
	], [
		abi_cv_redland="no"
	])
])

if test "$abi_cv_redland" = "yes"; then
	opt_pkgs="$opt_pkgs $redland_req"
	AC_DEFINE([WITH_REDLAND], [1], [Using redland])
fi

AM_CONDITIONAL([WITH_REDLAND], test "$abi_cv_redland" = "yes")
AM_CONDITIONAL([HAVE_REDLAND], test "$abi_cv_redland" = "yes")


evolution_data_server_req='libebook-1.2 >= 3.6'
AC_ARG_WITH([evolution_data_server], 
	[AS_HELP_STRING([--with-evolution-data-server], [Use Evolution Data Server to get at contact information])], 
[
	abi_cv_evolution_data_server="$withval"
],[
	# use evolution_data_server if detected
	PKG_CHECK_EXISTS([ $evolution_data_server_req ],
	[
		abi_cv_evolution_data_server="yes"
	], [
		abi_cv_evolution_data_server="no"
	])
])

if test "$abi_cv_evolution_data_server" = "yes"; then
	opt_pkgs="$opt_pkgs $evolution_data_server_req"
	AC_DEFINE([WITH_EVOLUTION_DATA_SERVER], [1], [Using evolution_data_server])
fi

AM_CONDITIONAL([WITH_EVOLUTION_DATA_SERVER], test "$abi_cv_evolution_data_server" = "yes")
AM_CONDITIONAL([HAVE_EVOLUTION_DATA_SERVER], test "$abi_cv_evolution_data_server" = "yes")


libical_req=' libical >= 0.46 '
AC_ARG_WITH([libical], 
	[AS_HELP_STRING([--with-libical], [use libical and raptor libraries])], 
[
	abi_cv_libical="$withval"
],[
	# use libical if detected
	PKG_CHECK_EXISTS([ $libical_req ],
	[
		abi_cv_libical="yes"
	], [
		abi_cv_libical="no"
	])
])

if test "$abi_cv_libical" = "yes"; then
	opt_pkgs="$opt_pkgs $libical_req"
	AC_DEFINE([WITH_LIBICAL], [1], [Using libical])
fi

AM_CONDITIONAL([WITH_LIBICAL], test "$abi_cv_libical" = "yes")
AM_CONDITIONAL([HAVE_LIBICAL], test "$abi_cv_libical" = "yes")


champlain_req=' champlain-gtk-0.12 '
AC_ARG_WITH([champlain], 
	[AS_HELP_STRING([--with-champlain], [use champlain to display maps])],
	,[with_champlain="check"])

if test "x$with_champlain" != "xno"; then
    PKG_CHECK_MODULES(CHAMPLAIN, [ $champlain_req ],,
    [if test "x$with_champlain" = "xyes"; then
    	AC_MSG_FAILURE([--with-champlain was given, but test for champlain failed])
    fi])
    abi_cv_champlain=yes

	if test "$pkg_failed" = "no"; then
	#
	# try to make sure that champlain-gtk through pkg-config and
 	# the GTK+ version that the user wants abiword to directly
 	# link with match on major number.
	# 
	# The test is simple, make sure that a little client using the
	# GTK+ that champlain natively uses is using the GTK+ version
	# that we want to link with.
 	#
	AC_LANG_PUSH(C++)
	CXXFLAGS_cache=$CXXFLAGS
	CXXFLAGS=" $CHAMPLAIN_CFLAGS "
	LDFLAGS_cache=$LDFLAGS
	LDFLAGS=" $CHAMPLAIN_LIBS "

 	  echo -n "checking whether CHAMPLAIN uses GTK+3... "
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([#include <champlain-gtk/champlain-gtk.h>],[
 #if GTK_MAJOR_VERSION < 3
 #error bad
 #endif
                               ])], gtk_matches=yes, gtk_matches=no)
        echo "$gtk_matches"        

	LDFLAGS=$LDFLAGS_cache
	CXXFLAGS=$CXXFLAGS_cache
        AC_LANG_POP

 	# Don't do mapping if their libchamplain is not using the right GTK+
	if test "$gtk_matches" = "no"; then
		if test "x$with_champlain" = "xyes"; then
			AC_MSG_FAILURE([--with-champlain was given, but champlain does not use the correct gtk version])
		fi
	   abi_cv_champlain=no
           CHAMPLAIN_CFLAGS=
           CHAMPLAIN_LIBS=
	fi
	else
		abi_cv_champlain=no
	fi
else
	abi_cv_champlain=no
fi

if test "$abi_cv_champlain" = "yes"; then
dnl	opt_pkgs="$opt_pkgs $champlain_req"
	AC_DEFINE([WITH_CHAMPLAIN], [1], [Using champlain])
fi

AC_SUBST([CHAMPLAIN_CFLAGS])
AC_SUBST([CHAMPLAIN_LIBS])
AM_CONDITIONAL([WITH_CHAMPLAIN], test "$abi_cv_champlain" = "yes")
AM_CONDITIONAL([HAVE_CHAMPLAIN], test "$abi_cv_champlain" = "yes")


AC_ARG_WITH([icondir],
	[AS_HELP_STRING([--with-icondir=DIR], [install icon in DIR instead of PREFIX/share/icons/THEME/SIZE/apps])],
[
	abi_cv_icondir="$withval"
])
if test "$abi_cv_icondir" = "" -o \
        "$abi_cv_icondir" = "yes" -o \
        "$abi_cv_icondir" = "no"; then
	abi_cv_prefix="$prefix"
	test "$abi_cv_prefix" = "NONE" && abi_cv_prefix="$ac_default_prefix"  
	abi_cv_icondir="${abi_cv_prefix}/share/icons"
fi
AC_SUBST(ABIWORD_ICONDIR, "$abi_cv_icondir")

dnl ***********************************************************************
dnl GCC/Clang sanitizer support
dnl ***********************************************************************
AC_ARG_WITH([sanitizer],
            [AS_HELP_STRING([--with-sanitizer=@<:@address/undefined/no@:>@],
                            [Use specific GCC/Clang analyzer])],
            [with_sanitizer=$withval],
            [with_sanitizer=no])
AS_IF([test "x$with_sanitizer" != "xno"],[
	AX_CHECK_COMPILE_FLAG([-fsanitize=$withval],
	                      [CFLAGS="$CFLAGS -fsanitize=$withval"
			       CXXFLAGS="$CXXFLAGS -fsanitize=$withval"],
	                      [AC_MSG_ERROR([-fsanitize=$withval is not supported])])
        AX_APPEND_COMPILE_FLAGS([-fno-omit-frame-pointer])
        AC_LANG_PUSH(C++)
        AX_APPEND_COMPILE_FLAGS([-fno-omit-frame-pointer])
        AC_LANG_POP
])

dnl ***************************************************************
dnl Excessive warnings
dnl ***************************************************************
AC_LANG_PUSH(C++)
AX_CHECK_COMPILE_FLAG([-Werror=unknown-warning-option], [
	ax_compiler_flags_test="-Werror=unknown-warning-option"
], [
	ax_compiler_flags_test=""
])
dnl XXX enable these
dnl -Wformat-nonliteral
AX_APPEND_COMPILE_FLAGS([ \
   -Wcast-align \
   -Wchar-subscripts \
   -Wconst-qual \
   -Wextra \
   -Wformat \
   -Wformat-security \
   -Wformat-overflow=2 \
   -Wformat-truncation=2 \
   -Wheader-guard \
   -Wimplicit-fallthrough=2 \
   -Wlogical-not-parentheses \
   -Wlogical-op \
   -Wmisleading-indentation \
   -Wmissing-noreturn \
   -Wno-overloaded-virtual \
   -Wparentheses-equality \
   -Wpointer-arith \
   -Wpointer-bool-conversion \
   -Wredundant-decls \
   -Wshadow \
   -Wsign-compare \
   -Wsuggest-override \
   -Wunreachable-code \
   -Wunreachable-code-loop-increment \
   -Wunused \
   -Wunused-const-variable \
   -Wunused-label \
   -Wunused-local-typedef \
   -Wunused-private-field \
   -Wunused-value \
   -Wunused-variable \
   -Wwrite-strings \
   -Wzero-as-null-pointer-constant \
], [], [$ax_compiler_flags_test])
AC_LANG_POP

#
# Dependency tests
#

# We need libpng
for l in libpng libpng16 libpng14 libpng12; do
  AC_MSG_CHECKING(for $l)
  if $PKG_CONFIG --exists $l ; then
    AC_MSG_RESULT(yes)
    PKG_CHECK_MODULES(PNG, $l, [abi_cv_png=yes],[abi_cv_png=no])
    break
  else
    AC_MSG_RESULT(no)
  fi
done
if test x$abi_cv_png != xyes; then
  AC_CHECK_HEADER([png.h], [], 
  [
    AC_MSG_ERROR([`png.h' not found, install libpng or specify CPPFLAGS to include custom locations])
  ])
  PNG_CFLAGS=
  PNG_LIBS=-lpng
fi

# We need ljpeg
AC_CHECK_HEADER(jpeglib.h,[
	AC_CHECK_LIB(jpeg,jpeg_read_header,
	[], 
	[
	  AC_MSG_ERROR([libjpeg not found])
	])
],[
	  AC_MSG_ERROR([jpeg.h not found])
])

# TODO need to check for libz too, at least on win32
AC_CHECK_HEADER([zlib.h], [], 
[
	AC_MSG_ERROR([`zlib.h' not found, install zlib or specify CPPFLAGS to include custom locations])
])

AX_BOOST_BASE([1.40.0],,[AC_MSG_ERROR([`boost' not found.])])

deps_pkgs="$xp_pkgs $opt_pkgs"
SYSTEM_LIBS=
SYSTEM_CFLAGS=$PNG_CFLAGS
SYSTEM_LIBS="$PNG_LIBS -ljpeg"
deps_pkgs="$deps_pkgs $gtk_pkgs x11"

PKG_CHECK_MODULES(DEPS,[$deps_pkgs])

DEPS_CFLAGS="$DEPS_CFLAGS -DGDK_VERSION_MIN_REQUIRED=$GTK_ENCODED_VERSION -DGDK_VERSION_MAX_ALLOWED=$GTK_ENCODED_VERSION"

dnl wv is vendored under thirdparty/ (patched, no system libwv needed)
DEPS_CFLAGS="$DEPS_CFLAGS -I\${abs_top_srcdir}/thirdparty/wv-1.2.9"

dnl make that for all.
AC_DEFINE([WITH_CAIRO], [1], [Using Cairo])
WITH_CAIRO=1
AM_CONDITIONAL([WITH_CAIRO], test "$WITH_CAIRO" = "1")

AC_SUBST([DEPS_CFLAGS])

DEPS_LIBS="$SYSTEM_LIBS $DEPS_LIBS"
AC_SUBST([DEPS_LIBS])

BASE_CPPFLAGS_='
  ${DEPS_CFLAGS} 
  ${BOOST_CPPFLAGS}
  -I${top_srcdir} 
  ${GOFFICE_BUILTIN_CPPFLAGS}'
BASE_CPPFLAGS="$SYSTEM_CFLAGS"
for f in $(echo $BASE_CPPFLAGS_); do BASE_CPPFLAGS="$BASE_CPPFLAGS $f"; done
AC_SUBST([BASE_CPPFLAGS])

AF_CPPFLAGS_='
  ${BASE_CPPFLAGS} 
  -I${top_srcdir}/src/af/ev/${TOOLKIT} 
  -I${top_srcdir}/src/af/ev/xp 
  -I${top_srcdir}/src/af/gr/${TOOLKIT} 
  -I${top_srcdir}/src/af/gr/xp 
  -I${top_srcdir}/src/af/util/${PLATFORM} 
  -I${top_srcdir}/src/af/util/xp 
  -I${top_srcdir}/src/af/xap/${TOOLKIT} 
  -I${top_srcdir}/src/af/xap/xp'
AF_CPPFLAGS=
for f in $(echo $AF_CPPFLAGS_); do AF_CPPFLAGS="$AF_CPPFLAGS $f"; done
AC_SUBST([AF_CPPFLAGS])

AF_TEST_CPPFLAGS_='
  ${AF_CPPFLAGS} 
  -I${top_srcdir}/src/af/tf/xp'
AF_TEST_CPPFLAGS=
for f in $(echo $AF_TEST_CPPFLAGS_); do AF_TEST_CPPFLAGS="$AF_TEST_CPPFLAGS $f"; done
AC_SUBST([AF_TEST_CPPFLAGS])

TEXT_CPPFLAGS_='
  ${AF_CPPFLAGS} 
  -I${top_srcdir}/src/text/fmt/${TOOLKIT}
  -I${top_srcdir}/src/text/fmt/xp 
  -I${top_srcdir}/src/text/ptbl/xp'
TEXT_CPPFLAGS=
for f in $(echo $TEXT_CPPFLAGS_); do TEXT_CPPFLAGS="$TEXT_CPPFLAGS $f"; done
AC_SUBST([TEXT_CPPFLAGS])

TEXT_TEST_CPPFLAGS_='
  ${TEXT_CPPFLAGS} 
  -I${top_srcdir}/src/af/tf/xp'
TEXT_TEST_CPPFLAGS=
for f in $(echo $TEXT_TEST_CPPFLAGS_); do TEXT_TEST_CPPFLAGS="$TEXT_TEST_CPPFLAGS $f"; done
AC_SUBST([TEXT_TEST_CPPFLAGS])

IMPEXP_CPPFLAGS_='
  ${TEXT_CPPFLAGS} 
  -I${top_srcdir}/src/wp/impexp/${TOOLKIT} 
  -I${top_srcdir}/src/wp/impexp/xp'
IMPEXP_CPPFLAGS=
for f in $(echo $IMPEXP_CPPFLAGS_); do IMPEXP_CPPFLAGS="$IMPEXP_CPPFLAGS $f"; done
AC_SUBST([IMPEXP_CPPFLAGS])

IMPEXP_TEST_CPPFLAGS_='
  ${IMPEXP_CPPFLAGS} 
  -I${top_srcdir}/src/af/tf/xp'
IMPEXP_TEST_CPPFLAGS=
for f in $(echo $IMPEXP_TEST_CPPFLAGS_); do IMPEXP_TEST_CPPFLAGS="$IMPEXP_TEST_CPPFLAGS $f"; done
AC_SUBST([IMPEXP_TEST_CPPFLAGS])

WP_CPPFLAGS_='
  ${IMPEXP_CPPFLAGS} 
  -I${top_srcdir}/src/wp/ap/${TOOLKIT} 
  -I${top_srcdir}/src/wp/ap/xp
'
WP_CPPFLAGS=
for f in $(echo $WP_CPPFLAGS_); do WP_CPPFLAGS="$WP_CPPFLAGS $f"; done
AC_SUBST([WP_CPPFLAGS])

#
# Settings
#

ABIWORD_SERIES="abi_series"
AC_DEFINE_UNQUOTED([ABIWORD_SERIES], [ "$ABIWORD_SERIES" ], [major.minor])
AC_SUBST(ABIWORD_SERIES)
AC_SUBST(ABIWORD_HEADERSDIR, "${includedir}/${PACKAGE_NAME}-${ABIWORD_SERIES}")
AC_SUBST(ABIWORD_DATADIR,    "${datadir}/${PACKAGE_NAME}-${ABIWORD_SERIES}")
AC_SUBST(ABIWORD_UIDIR,   "${ABIWORD_DATADIR}/ui")
AC_SUBST(ABIWORD_LIBDIR,     "${libdir}/${PACKAGE_NAME}-${ABIWORD_SERIES}")
AC_SUBST(ABIWORD_OMMLXSLTDIR, "${ABIWORD_DATADIR}/omml_xslt")

if test "$enable_shared" = ""; then
	enable_shared="no"
fi

if test "$enable_static" = ""; then
	enable_static="no"
fi

AC_DEFINE([TOOLKIT_GTK], [1], [Build gtk+ user interface])
AC_DEFINE([TOOLKIT_GTK_ALL], [1], [Build gtk+ user interface])
AM_CONDITIONAL([ENABLE_DYNAMIC], test "$enable_shared" = "yes")
AM_CONDITIONAL([ENABLE_STATIC], test "$enable_static" = "yes")

if test "$abi_cv_disable_exports" = "yes"; then
	AC_DEFINE([DISABLE_EXPORTS], [1], [Define to prevent symbols from being exported dynamically])
fi

if test "$abi_cv_print" = "yes"; then
	AC_DEFINE([ENABLE_PRINT], [1], [Define if building printing support])
fi
AM_CONDITIONAL([ENABLE_PRINT], test "$abi_cv_print" = "yes")

if test "$abi_cv_spell" = "yes"; then
	AC_DEFINE([ENABLE_SPELL], [1], [Define if building spell checking support])
	# TODO get rid of this after all platforms are using enchant exclusively
	AC_DEFINE([WITH_ENCHANT], [1], [use Dom's enchanting spell checker abstraction library])
fi
AM_CONDITIONAL([ENABLE_SPELL], test "$abi_cv_spell" = "yes")

if test "$abi_cv_statusbar" = "yes"; then
	AC_DEFINE([ENABLE_STATUSBAR], [1], [Define if building status bar])
fi
AM_CONDITIONAL([ENABLE_STATUSBAR], test "$abi_cv_statusbar" = "yes")

if test "$abi_cv_embedded_target" != "no"; then
	AC_DEFINE_UNQUOTED(EMBEDDED_TARGET, $abi_cv_embedded_target, [Whether we are building for embedded device])
fi

if test "$abi_cv_emacs_keybinding" = "yes"; then
	AC_DEFINE([ENABLE_EMACS_KEYBINDING], [1], [Define to enable include emacs-compatible keyboard commands])
fi
AM_CONDITIONAL(ENABLE_EMACS_KEYBINDING, test "$abi_cv_emacs_keybinding" = "yes")

if test "$abi_cv_vi_keybinding" = "yes"; then
	AC_DEFINE([ENABLE_VI_KEYBINDING], [1], [Define to enable include vi-compatible keyboard commands])
fi
AM_CONDITIONAL(ENABLE_VI_KEYBINDING, test "$abi_cv_vi_keybinding" = "yes")

AM_CONDITIONAL(ENABLE_CLIPART, test "$abi_cv_clipart" = "yes")
AM_CONDITIONAL(ENABLE_TEMPLATES, test "$abi_cv_templates" = "yes")

if test "$abi_cv_debug" = "yes"; then
	AC_DEFINE([DEBUG], [1], [Define to enable debugging functionality])
else
	AC_DEFINE([NDEBUG], [1], [Define to disable debugging functionality])
fi
AM_CONDITIONAL(DEBUG, test "$abi_cv_debug" = "yes")

#
# Optional dependencies handling
#
AM_PATH_LIBGCRYPT( 1.4.5,
		   [
		   abi_cv_gcrypt=no
 		   AC_DEFINE([HAVE_GCRYPT], [1], [Use gcrypt for the cryptos])
		   ],
		   [
		   abi_cv_gcrypt=yes
		   ] )
AM_CONDITIONAL(HAVE_GCRYPT, test "$abi_cv_gcrypt" = "yes")

if test "$abi_cv_gio" = "yes"; then
	AC_DEFINE([WITH_GIO], [1], [Define if using GIO])
fi



# bundled third-party libraries in thirdparty/ are always built now
# that the WordPerfect, WPG, MHT and WMF importers are part of the core
# build, so no external dependency downloads are required
with_bundled_revenge=yes
AM_CONDITIONAL([WITH_BUNDLED_REVENGE], [test "$with_bundled_revenge" = "yes"])

# optional libtidy cleans up HTML before import (core MHT importer)
AC_CHECK_HEADERS([tidy/tidy.h], [libtidy_found="yes"], [libtidy_found="no"])
TIDY_LIBS=
IMPEXP_OPT_CFLAGS=
if test "$libtidy_found" = "yes"; then
	TIDY_LIBS="-ltidy"
	IMPEXP_OPT_CFLAGS="$IMPEXP_OPT_CFLAGS -DXHTML_HTML_TIDY_SUPPORTED"
else
	# gsf already pulls in libxml2, so the libxml2 HTML parser is
	# always available as the non-tidy fallback
	IMPEXP_OPT_CFLAGS="$IMPEXP_OPT_CFLAGS -DXHTML_HTML_XML2_SUPPORTED"
fi
AC_SUBST([TIDY_LIBS])
AC_SUBST([IMPEXP_OPT_CFLAGS])
AM_CONDITIONAL([WITH_LIBTIDY], [test "$libtidy_found" = "yes"])

# optional libwmf for WMF image import
AC_PATH_PROG(libwmfconfig, libwmf-config)
wmf_deps="no"
WMF_CFLAGS=
WMF_LIBS=
if test "$libwmfconfig" != ""; then
	IFS_old="$IFS"
	IFS='.'
	set -- `$libwmfconfig --version`
	libwmf_major_found="${1}"
	libwmf_minor_found="${2}"
	libwmf_micro_found="${3}"
	IFS="$IFS_old"
	if test "$libwmf_major_found" -gt 0 || \
	   test "$libwmf_minor_found" -gt 2 || \
	   ( test "$libwmf_minor_found" -eq 2 && test "$libwmf_micro_found" -ge 8 ); then
		wmf_deps="yes"
	fi
fi
if test "$wmf_deps" = "yes"; then
	WMF_CFLAGS=`$libwmfconfig --cflags`
	WMF_LIBS=`$libwmfconfig --libs`
	IMPEXP_OPT_CFLAGS="$IMPEXP_OPT_CFLAGS -DHAVE_LIBWMF"
fi
AC_SUBST([WMF_CFLAGS])
AC_SUBST([WMF_LIBS])
AM_CONDITIONAL([WITH_LIBWMF], [test "$wmf_deps" = "yes"])

# put all deps on a single line for the .pc file
ABIWORD_REQUIRED_PKGS=
for dep in `echo $deps_pkgs`; do
	ABIWORD_REQUIRED_PKGS="$ABIWORD_REQUIRED_PKGS $dep"
done
AC_SUBST(ABIWORD_REQUIRED_PKGS)

dnl Check for valgrind for the UniTest
AC_CHECK_PROG(VALGRIND, valgrind, valgrind)
AC_CHECK_HEADERS(valgrind/memcheck.h)
if test "$VALGRIND" = ""; then
        AC_MSG_WARN([Valgrind is missing. UnitTesting will run without])
fi
if test "$VALGRIND" != ""; then
dnl due to stupid m4 I had to double the [ and ] in the regexp
        vg_ver=`valgrind --version | sed -e  's/.*-\([[0-9]]\)\.[[0-9]].*/\1/'`
        VALGRIND="$VALGRIND --tool=memcheck --leak-check=yes --num-callers=10"
        if test "$vg_ver" != "3" ; then
           VALGRIND="$VALGRIND --logfile-fd=-1"
        fi
fi
AC_SUBST(VALGRIND)

dnl Introspection support
GIR_REQ=1.0.0
AC_ARG_ENABLE(introspection,
          AS_HELP_STRING([--enable-introspection[=@<:@no/auto/yes@:>@]],
                         [Enable introspection for this build]),, 
                         [enable_introspection=no])

AC_MSG_CHECKING([for gobject-introspection])

dnl presence/version checking
AS_CASE([$enable_introspection],
[no], [
    found_introspection="no (disabled, use --enable-introspection to enable)"
],
[yes],[
    PKG_CHECK_EXISTS([gobject-introspection-1.0],,
                     AC_MSG_ERROR([gobject-introspection-1.0 is not installed]))
    PKG_CHECK_EXISTS([gobject-introspection-1.0 >= $GIR_REQ],
                     found_introspection=yes,
                     AC_MSG_ERROR([You need to have gobject-introspection >= $1 installed to build AC_PACKAGE_NAME]))
],
[auto],[
    PKG_CHECK_EXISTS([gobject-introspection-1.0 >= $GIR_REQ], found_introspection=yes, found_introspection=no)
dnl Canonicalize enable_introspection
enable_introspection=$found_introspection
],
[
    AC_MSG_ERROR([invalid argument passed to --enable-introspection, should be one of @<:@no/auto/yes@:>@])
])

AC_MSG_RESULT([$found_introspection])

INTROSPECTION_SCANNER=
INTROSPECTION_COMPILER=
INTROSPECTION_GENERATE=
INTROSPECTION_GIRDIR=
INTROSPECTION_TYPELIBDIR=
if test "x$found_introspection" = "xyes"; then
   AM_PATH_PYTHON
   INTROSPECTION_SCANNER=`$PKG_CONFIG --variable=g_ir_scanner gobject-introspection-1.0`
   INTROSPECTION_COMPILER=`$PKG_CONFIG --variable=g_ir_compiler gobject-introspection-1.0`
   INTROSPECTION_GENERATE=`$PKG_CONFIG --variable=g_ir_generate gobject-introspection-1.0`
   INTROSPECTION_GIRDIR=`$PKG_CONFIG --variable=girdir gobject-introspection-1.0`
   INTROSPECTION_TYPELIBDIR="$($PKG_CONFIG --variable=typelibdir gobject-introspection-1.0)"
   INTROSPECTION_CFLAGS=`$PKG_CONFIG --cflags gobject-introspection-1.0`
   INTROSPECTION_LIBS=`$PKG_CONFIG --libs gobject-introspection-1.0`
   INTROSPECTION_MAKEFILE=`$PKG_CONFIG --variable=datadir gobject-introspection-1.0`/gobject-introspection-1.0/Makefile.introspection
fi
AC_SUBST(INTROSPECTION_SCANNER)
AC_SUBST(INTROSPECTION_COMPILER)
AC_SUBST(INTROSPECTION_GENERATE)
AC_SUBST(INTROSPECTION_GIRDIR)
AC_SUBST(INTROSPECTION_TYPELIBDIR)
AC_SUBST(INTROSPECTION_CFLAGS)
AC_SUBST(INTROSPECTION_LIBS)
AC_SUBST(INTROSPECTION_MAKEFILE)

ABI_SUB_SERIES="abi_version_major"_"abi_version_minor"
AC_SUBST(ABI_SUB_SERIES)

dnl we need to change the install directories for distcheck
AC_ARG_WITH([gir-dir],
	AS_HELP_STRING(
		[--with-gir-dir],
		[
		 path to gir repository
		 (automatically detected via pkg-config)
		]
	),
	[GIRDIR=$withval],
	[GIRDIR=$INTROSPECTION_GIRDIR]
)
AC_SUBST(GIRDIR)
AC_ARG_WITH([typelib-dir],
	AS_HELP_STRING(
		[--with-typelib-dir],
		[
		 path to typelibs repository
		 (automatically detected via pkg-config)
		]
	),
	[TYPELIBDIR=$withval],
	[TYPELIBDIR=$INTROSPECTION_TYPELIBDIR]
)
AC_SUBST(TYPELIBDIR)

AM_CONDITIONAL(HAVE_INTROSPECTION, test "x$found_introspection" = "xyes")

AC_CONFIG_FILES([libabiword.pc
  Makefile
  icons/Makefile
  src/Makefile
  thirdparty/Makefile
  src/af/Makefile
  src/af/ev/Makefile
  src/af/gr/Makefile
  src/af/tf/Makefile
  src/af/util/Makefile
  src/af/xap/Makefile
  src/gi-overrides/Makefile
  src/text/Makefile
  src/wp/Makefile
  src/wp/ap/Makefile
  src/wp/impexp/Makefile
  src/wp/test/Makefile
  src/wp/test/unix/testwrap.sh
  src/wp/main/Makefile
  user/Makefile
  user/wp/Makefile
  user/wp/clipart/Makefile
  user/wp/dictionary/Makefile
  user/wp/templates/Makefile
  tools/Makefile
  fonts/Makefile
  io.github.janos_szenfner.Abinova.metainfo.xml
])
AC_OUTPUT

AC_CONFIG_FILES(
)
AC_OUTPUT

echo "
Configuration:
  host                  ${host}
  dynamic binary        ${enable_shared}
  static binary         ${enable_static}
  platform              ${PLATFORM} (embedded: $abi_cv_embedded_platform)
  toolkit               ${TOOLKIT}
  debug                 ${abi_cv_debug}

Optional features:
  printing              ${abi_cv_print}
  spell checking        ${abi_cv_spell}
  status bar            ${abi_cv_statusbar}
  emacs keybinding      ${abi_cv_emacs_keybinding}
  vi keybinding         ${abi_cv_vi_keybinding}
  clipart               ${abi_cv_clipart}
  templates             ${abi_cv_templates}

Optional dependencies:
  gio			${abi_cv_gio}
  redland               ${abi_cv_redland}
  evolution_data_server ${abi_cv_evolution_data_server}
  libical               ${abi_cv_libical}
  champlain             ${abi_cv_champlain}
"

