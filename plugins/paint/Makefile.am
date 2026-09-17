
AM_CPPFLAGS = \
	$(PAINT_CFLAGS) \
	-I$(srcdir)/unix \
	-DABI_BUILD_VERSION=\"$(VERSION)\"

if PAINT_BUILTIN
noinst_LTLIBRARIES = paint.la

else

plugindir = $(ABIWORD_PLUGINSDIR)
plugin_LTLIBRARIES = paint.la

endif

paint_la_LIBADD = \
	$(PAINT_LIBS)

paint_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined

paint_la_SOURCES = \
	xp/AbiGeneric.cpp \
	xp/AbiGeneric.h \
	xp/AbiPaint.cpp \
	xp/AbiPaint.h

paint_la_SOURCES += \
	unix/ut_process.h \
	unix/ut_UnixProcess.cpp


EXTRA_DIST = \
	plugin.m4
