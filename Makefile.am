
ACLOCAL_AMFLAGS = -I m4

DIST_SUBDIRS = src thirdparty plugins po user icons tools fonts

SUBDIRS = thirdparty src plugins po user


SUBDIRS += icons fonts

desktopdir = $(datadir)/applications
desktop_DATA = com.abisource.AbiWord.desktop

mimedir = @ABIWORD_DATADIR@/mime-info
mime_DATA = abiword.keys

# MathML -> LaTeX stylesheets used by the core math import path
# (needed by the openxml and opendocument plugins)
xsltmldir = @ABIWORD_DATADIR@/xsltml
xsltml_DATA = \
	xsltml/README \
	xsltml/cmarkup.xsl \
	xsltml/entities.xsl \
	xsltml/glayout.xsl \
	xsltml/mmltex.xsl \
	xsltml/scripts.xsl \
	xsltml/tables.xsl \
	xsltml/tokens.xsl

metainfodir = $(datarootdir)/metainfo
metainfo_DATA = com.abisource.AbiWord.metainfo.xml

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = abiword-$(ABIWORD_SERIES).pc

dist_man1_MANS = abiword.1


# we disable introspection for distcheck as it fails because of
# installation needing sudo
DISTCHECK_CONFIGURE_FLAGS = \
	--enable-introspection=no \
	--with-gir-dir=\$${datadir}/gir-1.0 \
	--with-typelib-dir=\$${libdir}/girepository-1.0

EXTRA_DIST = \
	AUTHORS.md \
	Old-Doc/BiDiReadme.txt \
	Old-Doc/BUILD.TXT \
	COPYING	\
	COPYRIGHT.TXT \
	Old-Doc/INSTALL \
	Old-Doc/NEWS \
	Old-Doc/README \
	Old-Doc/README.md \
	Old-Doc/ChangeLog \
	Old-Doc/AUTHORS.old \
	README.md \
	abiword.desktop \
	abiword.keys \
	abiword.png \
	libabiword.pc.in \
	artwork \
	$(xsltml_DATA)

DISTCLEANFILES = \
	abiword-$(ABIWORD_SERIES).pc \
	plugin-builtin.m4 \
	plugin-configure.m4 \
	plugin-list.m4 \
	plugin-makefiles.m4

abiword-$(ABIWORD_SERIES).pc: libabiword.pc
	cp $< $@


# bundled user manual (generated from AbiWord/abiword-docs .abw sources)
install-data-local:
	cp -r $(srcdir)/help $(DESTDIR)@ABIWORD_DATADIR@/
	cp -r $(srcdir)/artwork $(DESTDIR)@ABIWORD_DATADIR@/

uninstall-local:
	rm -rf $(DESTDIR)@ABIWORD_DATADIR@/help
	rm -rf $(DESTDIR)@ABIWORD_DATADIR@/artwork

# needed to make sure we don't include .svn directories.
dist-hook:
	rm -rf `find $(distdir) -name .svn`
	cp -r $(srcdir)/help $(distdir)/help
	cp -r $(srcdir)/artwork $(distdir)/artwork

