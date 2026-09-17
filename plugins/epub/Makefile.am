
AM_CPPFLAGS = \
	$(EPUB_CFLAGS) \
	-I$(srcdir)/imp/xp \
	-I$(srcdir)/exp/xp \
	-I$(srcdir)/dialogs/xp \
	-DABI_BUILD_VERSION=\"$(VERSION)\"

if EPUB_BUILTIN
noinst_LTLIBRARIES = epub.la

else

plugindir = $(ABIWORD_PLUGINSDIR)
plugin_LTLIBRARIES = epub.la

endif

epub_la_LIBADD = $(EPUB_LIBS)

epub_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined

epub_la_SOURCES = \
	common/xp/ie_impexp_EPUB.cpp \
	common/xp/ie_impexp_EPUB.h \
	imp/xp/ie_imp_EPUB.cpp \
	imp/xp/ie_imp_EPUB.h \
	imp/xp/ie_imp_EPUB_Sniffer.cpp \
	imp/xp/ie_imp_EPUB_Sniffer.h \
	exp/xp/ie_exp_EPUB.cpp \
	exp/xp/ie_exp_EPUB.h \
	exp/xp/ie_exp_EPUB_Sniffer.cpp \
	exp/xp/ie_exp_EPUB_Sniffer.h \
	exp/xp/ie_exp_EPUB_EPUB3Writer.cpp \
	exp/xp/ie_exp_EPUB_EPUB3Writer.h

epub_la_SOURCES += \
	dialogs/xp/ap_Dialog_EpubExportOptions.cpp \
	dialogs/xp/ap_Dialog_EpubExportOptions.h

epub_la_SOURCES += \
	dialogs/gtk/ap_UnixDialog_EpubExportOptions.cpp \
	dialogs/gtk/ap_UnixDialog_EpubExportOptions.h



EXTRA_DIST = plugin.m4
