
SUBDIRS = backends core

SYSTEM_LIBS=

plugindir = $(ABIWORD_PLUGINSDIR)
plugin_LTLIBRARIES = collab.la

collab_la_LIBADD = \
	backends/libbackends.la \
	core/libcore.la \
	$(COLLAB_LIBS) \
	$(SYSTEM_LIBS)

collab_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined

collab_la_SOURCES = 
nodist_EXTRA_collab_la_SOURCES = dummy.cpp

EXTRA_DIST = \
	plugin.m4

