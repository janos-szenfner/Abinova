
AM_CPPFLAGS = \
	$(OPENWRITER_CFLAGS) \
	-DABI_BUILD_VERSION=\"$(VERSION)\"

if OPENWRITER_BUILTIN
noinst_LTLIBRARIES = openwriter.la

else

plugindir = $(ABIWORD_PLUGINSDIR)
plugin_LTLIBRARIES = openwriter.la

endif

openwriter_la_LIBADD = \
	$(OPENWRITER_LIBS)

openwriter_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined

openwriter_la_SOURCES = \
	xp/ie_exp_OpenWriter.cpp \
	xp/ie_exp_OpenWriter.h	\
	xp/ie_impexp_OpenWriter.cpp \
	xp/ie_impexp_OpenWriter.h \
	xp/ie_imp_OpenWriter.cpp


EXTRA_DIST = \
	plugin.m4

