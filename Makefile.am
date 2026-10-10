
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
	io.github.janos_szenfner.Abinova.yml \
	abinova.keys \
	abwn.dtd \
	libabinova.pc.in \
	artwork \
	dist \
	fuzz \
	$(xsltml_DATA)

DISTCLEANFILES = \
	abinova-$(ABINOVA_SERIES).pc

abinova-$(ABINOVA_SERIES).pc: libabinova.pc
	cp $< $@

# libFuzzer smoke pass: replays every built fuzz/fuzz_<fmt> target over
# its fuzz/corpus/<fmt> seeds plus pinned reproducers in
# fuzz/regress/<fmt>.  Non-failing by design (skips cleanly when the
# instrumented fuzz-build/ tree is absent); FUZZ_CHECK_STRICT=1 makes
# crashes/reproductions fail.  Build the harnesses with
# tools/build-fuzz.sh first.
.PHONY: check-fuzz
check-fuzz:
	$(srcdir)/tools/check-fuzz.sh

# gcov/lcov report over the existing `make check` run.  Requires the
# tree to be configured with --enable-coverage; captures .gcda counters
# into coverage.info and renders coverage-html/ via the vendored lcov in
# tools/lcov/ (perl + gcov only, no system lcov needed).
.PHONY: coverage
if ENABLE_COVERAGE
coverage:
	$(srcdir)/tools/coverage.sh $(abs_top_builddir)
else
coverage:
	@echo "coverage requires configuring with --enable-coverage" >&2
	@exit 1
endif

# Coverage ratchet (COV10): `make check-coverage` runs the whole test
# suite in the instrumented build, captures the report, and fails if
# total first-party line coverage regresses below the 72% floor
# (tools/coverage-gate.sh; COVERAGE_MIN_PCT overrides).  Requires
# --enable-coverage like `make coverage`.
.PHONY: check-coverage
if ENABLE_COVERAGE
check-coverage: check
	$(srcdir)/tools/coverage.sh $(abs_top_builddir)
	$(srcdir)/tools/coverage-gate.sh $(abs_top_builddir)/coverage.info
else
check-coverage:
	@echo "check-coverage requires configuring with --enable-coverage" >&2
	@exit 1
endif

# AddressSanitizer + UndefinedBehaviorSanitizer gate: tools/check-san.sh
# keeps a separate instrumented copy of the tree under san-build/ (the
# same scratch-tree pattern as fuzz-build/) configured
# --with-sanitizer=address,undefined, builds it and runs the make-check
# suite inside it.  One combined build backs both targets; each run
# reports every sanitizer error class.  EXPENSIVE: a full tree rebuild
# on first run plus an instrumented suite run — opt-in only, the
# normal build stays sanitizer-free.
.PHONY: check-asan check-ubsan
check-asan:
	$(srcdir)/tools/check-san.sh asan
check-ubsan:
	$(srcdir)/tools/check-san.sh ubsan

# Bounded corpus of headless ->pdf conversions under valgrind
# (--errors-for-leak-kinds=definite): guards the DOC15/DOC20/UB
# hardening.  Uses the normal uninstrumented build.
.PHONY: check-valgrind
check-valgrind:
	$(srcdir)/tools/check-valgrind.sh

# Freeze regression gate (TST12): unit time bounds for the FRZ01-03
# fixes (redraw notify, status-bar urgent message, content serial,
# clipboard payload ownership, the backup serialize) plus the
# ui-drive --freeze heartbeat leg — a 40ms GSource measuring
# main-context service gaps across the scripted ops the fixes touch.
# The drive leg needs a display (xvfb-run preferred, live session
# otherwise); it skips cleanly without one so the unit half still
# gates.  FREEZE_NO_DRIVE=1 skips the drive leg entirely.
.PHONY: check-freeze
check-freeze:
	$(srcdir)/tools/check-freeze.sh


# the Changelog help page is generated from CHANGELOG.md so the
# in-app viewer never drifts out of sync with the repository file
all-local: $(srcdir)/help/en-US/changelog.html

$(srcdir)/help/en-US/changelog.html: $(srcdir)/CHANGELOG.md $(srcdir)/tools/changelog2html.sh
	$(srcdir)/tools/changelog2html.sh $(srcdir)/CHANGELOG.md $@

# bundled user manual (single-page HTML for the built-in help
# browser); wipe first so removed pages don't linger from older
# installs. License texts for the statically-linked vendored
# libraries and the consolidated third-party notice file install
# into the datadir so they ride inside every bundle.
install-data-local:
	rm -rf $(DESTDIR)@ABINOVA_DATADIR@/help
	cp -r $(srcdir)/help $(DESTDIR)@ABINOVA_DATADIR@/
	cp -r $(srcdir)/artwork $(DESTDIR)@ABINOVA_DATADIR@/
	rm -rf $(DESTDIR)@ABINOVA_DATADIR@/licenses
	for d in hunspell-1.7.4 librevenge-0.0.6 libwpd-0.10.3 libwpg-0.3.4 libwps-0.4.14 wv-1.2.9; do \
		mkdir -p $(DESTDIR)@ABINOVA_DATADIR@/licenses/$$d; \
		for f in $(srcdir)/thirdparty/$$d/COPYING* $(srcdir)/thirdparty/$$d/LICENSE* $(srcdir)/thirdparty/$$d/license*; do \
			if [ -f "$$f" ]; then cp "$$f" $(DESTDIR)@ABINOVA_DATADIR@/licenses/$$d/; fi; \
		done; \
	done
	mkdir -p $(DESTDIR)@ABINOVA_DATADIR@/licenses/openssl-blowfish
	cp $(srcdir)/src/wp/impexp/odf/common/xp/crypto/blowfish/LICENSE.txt \
		$(DESTDIR)@ABINOVA_DATADIR@/licenses/openssl-blowfish/
	cp $(srcdir)/COPYING $(srcdir)/COPYRIGHT.TXT $(DESTDIR)@ABINOVA_DATADIR@/
	cp $(srcdir)/dist/THIRD-PARTY-NOTICES.md $(DESTDIR)@ABINOVA_DATADIR@/

uninstall-local:
	rm -rf $(DESTDIR)@ABINOVA_DATADIR@/help
	rm -rf $(DESTDIR)@ABINOVA_DATADIR@/artwork
	rm -rf $(DESTDIR)@ABINOVA_DATADIR@/licenses
	rm -f $(DESTDIR)@ABINOVA_DATADIR@/COPYING \
		$(DESTDIR)@ABINOVA_DATADIR@/COPYRIGHT.TXT \
		$(DESTDIR)@ABINOVA_DATADIR@/THIRD-PARTY-NOTICES.md

# needed to make sure we don't include .svn directories.
dist-hook:
	rm -rf `find $(distdir) -name .svn`
	cp -r $(srcdir)/help $(distdir)/help
	cp -r $(srcdir)/artwork $(distdir)/artwork

