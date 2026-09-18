
ACLOCAL_AMFLAGS = -I m4

DIST_SUBDIRS = src thirdparty plugins po user icons tools

SUBDIRS = thirdparty src plugins po user


SUBDIRS += icons

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
	AUTHORS \
	BiDiReadme.txt \
	BUILD.TXT \
	COPYING	\
	COPYRIGHT.TXT \
	INSTALL \
	NEWS \
	README \
	ChangeLog \
	abiword.desktop \
	abiword.keys \
	abiword.png \
	libabiword.pc.in \
	$(xsltml_DATA)

DISTCLEANFILES = \
	abiword-$(ABIWORD_SERIES).pc \
	plugin-builtin.m4 \
	plugin-configure.m4 \
	plugin-list.m4 \
	plugin-makefiles.m4

abiword-$(ABIWORD_SERIES).pc: libabiword.pc
	cp $< $@


# needed to make sure we don't include .svn directories.
dist-hook:
	rm -rf `find $(distdir) -name .svn`

