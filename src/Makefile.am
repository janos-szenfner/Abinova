#AUTOMAKE_OPTIONS = subdir-objects

SUBDIRS = af text wp

libabinova_cpp = wp/main/gtk/libabinova.cpp
main_cpp = wp/main/gtk/UnixMain.cpp

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = \
	-v --warn-all --accept-unprefixed \
	--symbol-prefix=abi --symbol-prefix=libabinova
INTROSPECTION_COMPILER_ARGS = \
	--includedir=$(srcdir) \
    --includedir=.

if HAVE_INTROSPECTION
SUBDIRS += gi-overrides
endif

lib_LTLIBRARIES = libabinova-@ABINOVA_SERIES@.la

libabinova_@ABINOVA_SERIES@_la_CPPFLAGS = \
	$(WP_CPPFLAGS) \
	-DABINOVA_DATADIR="\"$(ABINOVA_DATADIR)\""

libabinova_@ABINOVA_SERIES@_la_LDFLAGS = \
	$(DEPS_LIBS) \
	$(CHAMPLAIN_LIBS) \
	-no-undefined \
	-avoid-version \
	-export-dynamic

libabinova_@ABINOVA_SERIES@_la_LIBADD = \
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

libabinova_cpp += abi-resources.c abi-resources.h
## End of resources

# headers are GTK only.
libabinova_@ABINOVA_SERIES@_includedir = @ABINOVA_HEADERSDIR@
libabinova_@ABINOVA_SERIES@_include_HEADERS = \
	wp/ap/gtk/abiwidget.h \
	wp/main/gtk/libabinova.h \
	af/xap/gtk/xap_UnixTableWidget.h

libabinova_@ABINOVA_SERIES@_la_DEPENDENCIES = \
	$(libabinova_@ABINOVA_SERIES@_la_LIBADD)

libabinova_@ABINOVA_SERIES@_la_SOURCES = \
	wp/main/xp/abi_ver.cpp \
	$(libabinova_cpp)

bin_PROGRAMS = abinova

abinova_CPPFLAGS = \
	$(libabinova_@ABINOVA_SERIES@_la_CPPFLAGS)

abinova_LDADD = \
	libabinova-@ABINOVA_SERIES@.la

abinova_DEPENDENCIES = \
	libabinova-@ABINOVA_SERIES@.la

abinova_SOURCES = \
	$(main_cpp)


if HAVE_INTROSPECTION

INTROSPECTION_GIRS += Abi-@ABINOVA_SERIES@.gir

Abi-@ABINOVA_SERIES@.gir: $(INTROSPECTION_SCANNER) libabinova-@ABINOVA_SERIES@.la Makefile
Abi_@ABI_SUB_SERIES@_gir_INCLUDES = GObject-2.0 Gtk-3.0 Gsf-1
Abi_@ABI_SUB_SERIES@_gir_CFLAGS = $(libabinova_@ABINOVA_SERIES@_la_CPPFLAGS)
Abi_@ABI_SUB_SERIES@_gir_LIBS = libabinova-@ABINOVA_SERIES@.la
Abi_@ABI_SUB_SERIES@_gir_FILES = \
	af/xap/gtk/xap_UnixTableWidget.h \
	af/xap/gtk/xap_UnixTableWidget.cpp \
	wp/ap/gtk/abiwidget.h \
	wp/ap/gtk/abiwidget.cpp \
	wp/main/gtk/libabinova.h \
	wp/main/gtk/libabinova.cpp

girdir = $(GIRDIR)
gir_DATA = $(INTROSPECTION_GIRS)

typelibsdir = $(TYPELIBDIR)
typelibs_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES = $(gir_DATA) $(typelibs_DATA)

endif # HAVE_INTROSPECTION
