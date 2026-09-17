
AM_CPPFLAGS = \
	$(LATEX_CFLAGS) \
	-DABI_BUILD_VERSION=\"$(VERSION)\"

if LATEX_BUILTIN
noinst_LTLIBRARIES = latex.la

else

plugindir = $(ABIWORD_PLUGINSDIR)
plugin_LTLIBRARIES = latex.la

endif

latex_la_LIBADD = \
	$(LATEX_LIBS)

latex_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined

latex_la_SOURCES = \
	xp/ie_exp_LaTeX.cpp \
	xp/ie_exp_LaTeX.h

if HAVE_LIBXSLT
xslt_DATA = $(EXTRA_DIST)
xsltdir = $(ABIWORD_XSLTMLDIR)
endif


EXTRA_DIST = \
	xsltml/cmarkup.xsl \
	xsltml/entities.xsl \
	xsltml/glayout.xsl \
	xsltml/mmltex.xsl \
	xsltml/scripts.xsl \
	xsltml/tables.xsl \
	xsltml/tokens.xsl \
	xsltml/README \
	plugin.m4

