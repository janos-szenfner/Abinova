
if WITH_BUILTIN_AIKSAURUS_GTK
SUBDIRS = aiksaurusgtk3
platform_lib = aiksaurusgtk3/libAiksaurusGtk3.la
endif


AM_CPPFLAGS = \
	$(AIKSAURUS_CFLAGS) \
	-I$(srcdir) \
	-DABI_BUILD_VERSION=\"$(VERSION)\"

aiksaurus_la_SOURCES = \
	xp/AiksaurusABI.cpp


plugindir = $(ABIWORD_PLUGINSDIR)
plugin_LTLIBRARIES = aiksaurus.la

aiksaurus_la_LIBADD = \
	$(platform_lib) \
	$(AIKSAURUS_LIBS)

aiksaurus_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined

EXTRA_DIST = \
	plugin.m4
