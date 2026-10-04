## AbiSource Applications
## Copyright (C) 2001 Sam Tobin-Hochstadt
## Copyright (C) 2023 Hubert Figuière
# Copyright (C) 2025-2026 Abinova contributors
##
## This program is free software; you can redistribute it and/or
## modify it under the terms of the GNU General Public License
## as published by the Free Software Foundation; either version 2
## of the License, or (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
## 02110-1301 USA.

TESTS = unix/testwrap.sh unix/rtwrap.sh unix/portwrap.sh unix/dlgswrap.sh

check_SCRIPTS = testwrap.sh-stamp rtwrap.sh-stamp portwrap.sh-stamp \
	dlgswrap.sh-stamp unix/testwrap.sh unix/rtwrap.sh \
	unix/portwrap.sh unix/dlgswrap.sh
check_PROGRAMS = Abinova-test dialog-smoke

Abinova_test_CPPFLAGS = $(WP_CPPFLAGS) \
	-DABINOVA_DATADIR="\"$(ABINOVA_DATADIR)\"" \
	-I$(top_srcdir)/src/af/tf/xp/

Abinova_test_LDFLAGS =  -L$(top_builddir)/src -labinova-@ABINOVA_SERIES@ \
	-L$(top_builddir)/src/af/tf -labinova-@ABINOVA_SERIES@-test

Abinova_test_LDADD =  $(DEPS_LIBS)

Abinova_test_SOURCES = unix/test_main.cpp \
	unix/all_test_unix.h

dialog_smoke_CPPFLAGS = $(WP_CPPFLAGS) \
	-DABINOVA_DATADIR="\"$(ABINOVA_DATADIR)\""

dialog_smoke_LDFLAGS =  -L$(top_builddir)/src -labinova-@ABINOVA_SERIES@

dialog_smoke_LDADD =  $(DEPS_LIBS)

dialog_smoke_SOURCES = unix/dialog_smoke.cpp

# The wrapper scripts are config.status outputs living in a
# subdirectory of this Makefile — automake gives them no
# regeneration rule, so a .in edit would never reach an already
# configured build tree (e.g. san-build).  Re-run config.status
# ourselves when the .in is newer.
unix/testwrap.sh: $(top_builddir)/config.status $(srcdir)/unix/testwrap.sh.in
	cd $(top_builddir) && $(SHELL) ./config.status src/wp/test/$@

unix/rtwrap.sh: $(top_builddir)/config.status $(srcdir)/unix/rtwrap.sh.in
	cd $(top_builddir) && $(SHELL) ./config.status src/wp/test/$@

unix/portwrap.sh: $(top_builddir)/config.status $(srcdir)/unix/portwrap.sh.in
	cd $(top_builddir) && $(SHELL) ./config.status src/wp/test/$@

unix/dlgswrap.sh: $(top_builddir)/config.status $(srcdir)/unix/dlgswrap.sh.in
	cd $(top_builddir) && $(SHELL) ./config.status src/wp/test/$@

testwrap.sh-stamp: unix/testwrap.sh
	chmod a+x unix/testwrap.sh
	touch $@

rtwrap.sh-stamp: unix/rtwrap.sh
	chmod a+x unix/rtwrap.sh
	touch $@

portwrap.sh-stamp: unix/portwrap.sh
	chmod a+x unix/portwrap.sh
	touch $@

dlgswrap.sh-stamp: unix/dlgswrap.sh
	chmod a+x unix/dlgswrap.sh
	touch $@

CLEANFILES = vgcore.* \
	supp.txt \
	testwrap.sh-stamp \
	rtwrap.sh-stamp \
	portwrap.sh-stamp \
	dlgswrap.sh-stamp \
	tf-vgdump

EXTRA_DIST = \
	xp/main.cpp \
	xp/all_test.h \
	unix/dialog_smoke.cpp
