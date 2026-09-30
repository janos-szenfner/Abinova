
ACLOCAL_AMFLAGS = -I m4

DIST_SUBDIRS = src thirdparty user icons tools fonts

SUBDIRS = thirdparty src user


SUBDIRS += icons fonts

desktopdir = $(datadir)/applications
desktop_DATA = io.github.janos_szenfner.Abinova.desktop

mimedir = @ABINOVA_DATADIR@/mime-info
mime_DATA = abinova.keys

# MathML -> LaTeX stylesheets used by the core math import path
xsltmldir = @ABINOVA_DATADIR@/xsltml
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
metainfo_DATA = io.github.janos_szenfner.Abinova.metainfo.xml

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = abinova-$(ABINOVA_SERIES).pc

dist_man1_MANS = abinova.1


# we disable introspection for distcheck as it fails because of
# installation needing sudo
DISTCHECK_CONFIGURE_FLAGS = \
	--enable-introspection=no \
	--with-gir-dir=\$${datadir}/gir-1.0 \
	--with-typelib-dir=\$${libdir}/girepository-1.0

EXTRA_DIST = \
	AUTHORS.md \
	COPYING	\
	COPYRIGHT.TXT \
	README.md \
	io.github.janos_szenfner.Abinova.desktop \
	abinova.keys \
	libabinova.pc.in \
	artwork \
	$(xsltml_DATA)

DISTCLEANFILES = \
	abinova-$(ABINOVA_SERIES).pc

abinova-$(ABINOVA_SERIES).pc: libabinova.pc
	cp $< $@


# the Changelog help page is generated from CHANGELOG.md so the
# in-app viewer never drifts out of sync with the repository file
all-local: $(srcdir)/help/en-US/changelog.html

$(srcdir)/help/en-US/changelog.html: $(srcdir)/CHANGELOG.md $(srcdir)/tools/changelog2html.sh
	$(srcdir)/tools/changelog2html.sh $(srcdir)/CHANGELOG.md $@

# bundled user manual (single-page HTML for the built-in help
# browser); wipe first so removed pages don't linger from older
# installs
install-data-local:
	rm -rf $(DESTDIR)@ABINOVA_DATADIR@/help
	cp -r $(srcdir)/help $(DESTDIR)@ABINOVA_DATADIR@/
	cp -r $(srcdir)/artwork $(DESTDIR)@ABINOVA_DATADIR@/

uninstall-local:
	rm -rf $(DESTDIR)@ABINOVA_DATADIR@/help
	rm -rf $(DESTDIR)@ABINOVA_DATADIR@/artwork

# needed to make sure we don't include .svn directories.
dist-hook:
	rm -rf `find $(distdir) -name .svn`
	cp -r $(srcdir)/help $(distdir)/help
	cp -r $(srcdir)/artwork $(distdir)/artwork

