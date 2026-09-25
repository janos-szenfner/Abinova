#AUTOMAKE_OPTIONS = subdir-objects

SUBDIRS = af text wp

libabiword_cpp = wp/main/gtk/libabiword.cpp
main_cpp = wp/main/gtk/UnixMain.cpp

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = \
	-v --warn-all --accept-unprefixed \
	--symbol-prefix=abi --symbol-prefix=libabiword
INTROSPECTION_COMPILER_ARGS = \
	--includedir=$(srcdir) \
    --includedir=.

if HAVE_INTROSPECTION
SUBDIRS += gi-overrides
endif

lib_LTLIBRARIES = libabiword-@ABIWORD_SERIES@.la

libabiword_@ABIWORD_SERIES@_la_CPPFLAGS = \
	$(WP_CPPFLAGS) \
	-DABIWORD_DATADIR="\"$(ABIWORD_DATADIR)\""

libabiword_@ABIWORD_SERIES@_la_LDFLAGS = \
	$(DEPS_LIBS) \
	$(CHAMPLAIN_LIBS) \
	-no-undefined \
	-avoid-version \
	-export-dynamic

libabiword_@ABIWORD_SERIES@_la_LIBADD = \
	text/libtext.la \
	wp/ap/libap.la \
	wp/impexp/libimpexp.la \
	af/xap/libxap.la \
	af/gr/libgr.la \
	af/util/libutil.la \
	af/ev/libev.la \
	$(top_builddir)/thirdparty/libwv.la \
	$(top_builddir)/thirdparty/libwps.la \
	$(top_builddir)/thirdparty/libwpg.la \
	$(top_builddir)/thirdparty/libwpd.la \
	$(top_builddir)/thirdparty/librevenge.la \
	$(top_builddir)/thirdparty/libhunspell.la \
	$(WMF_LIBS) \
	$(TIDY_LIBS) \
	-lz

## Resources compilation
resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) \
	--generate-dependencies $(srcdir)/wp/main/gtk/resources.xml 2>/dev/null)

abi-resources.c: wp/main/gtk/resources.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources \
                --sourcedir=$(srcdir) --target=$@ \
                --generate-source --c-name abi $(srcdir)/wp/main/gtk/resources.xml

abi-resources.h: wp/main/gtk/resources.xml
	$(AM_V_GEN) glib-compile-resources                                  \
                --sourcedir=$(srcdir) --target=$@ \
		--generate-header --c-name abi $(srcdir)/wp/main/gtk/resources.xml

BUILT_SOURCES = \
        abi-resources.c \
        abi-resources.h

libabiword_cpp += abi-resources.c abi-resources.h
## End of resources

# headers are GTK only.
libabiword_@ABIWORD_SERIES@_includedir = @ABIWORD_HEADERSDIR@
libabiword_@ABIWORD_SERIES@_include_HEADERS = \
	wp/ap/gtk/abiwidget.h \
	wp/main/gtk/libabiword.h \
	af/xap/gtk/xap_UnixTableWidget.h

libabiword_@ABIWORD_SERIES@_la_DEPENDENCIES = \
	$(libabiword_@ABIWORD_SERIES@_la_LIBADD)

libabiword_@ABIWORD_SERIES@_la_SOURCES = \
	wp/main/xp/abi_ver.cpp \
	$(libabiword_cpp)

bin_PROGRAMS = abinova

abinova_CPPFLAGS = \
	$(libabiword_@ABIWORD_SERIES@_la_CPPFLAGS)

abinova_LDADD = \
	libabiword-@ABIWORD_SERIES@.la

abinova_DEPENDENCIES = \
	libabiword-@ABIWORD_SERIES@.la

abinova_SOURCES = \
	$(main_cpp)


if HAVE_INTROSPECTION

INTROSPECTION_GIRS += Abi-@ABIWORD_SERIES@.gir

Abi-@ABIWORD_SERIES@.gir: $(INTROSPECTION_SCANNER) libabiword-@ABIWORD_SERIES@.la Makefile
Abi_@ABI_SUB_SERIES@_gir_INCLUDES = GObject-2.0 Gtk-3.0 Gsf-1
Abi_@ABI_SUB_SERIES@_gir_CFLAGS = $(libabiword_@ABIWORD_SERIES@_la_CPPFLAGS)
Abi_@ABI_SUB_SERIES@_gir_LIBS = libabiword-@ABIWORD_SERIES@.la
Abi_@ABI_SUB_SERIES@_gir_FILES = \
	af/xap/gtk/xap_UnixTableWidget.h \
	af/xap/gtk/xap_UnixTableWidget.cpp \
	wp/ap/gtk/abiwidget.h \
	wp/ap/gtk/abiwidget.cpp \
	wp/main/gtk/libabiword.h \
	wp/main/gtk/libabiword.cpp

girdir = $(GIRDIR)
gir_DATA = $(INTROSPECTION_GIRS)

typelibsdir = $(TYPELIBDIR)
typelibs_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES = $(gir_DATA) $(typelibs_DATA)

endif # HAVE_INTROSPECTION
