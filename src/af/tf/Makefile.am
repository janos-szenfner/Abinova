
AM_CPPFLAGS = \
	$(AF_CPPFLAGS)

lib_LTLIBRARIES = libabinova-@ABINOVA_SERIES@-test.la

libabinova_@ABINOVA_SERIES@_test_la_LDFLAGS = \
	-no-undefined

# tf_test.cpp calls g_file_new_for_uri/g_strdup — Mach-O requires them
# resolved at link time even though ELF tolerates leaving them undefined
libabinova_@ABINOVA_SERIES@_test_la_LIBADD = \
	$(DEPS_LIBS)

libabinova_@ABINOVA_SERIES@_test_la_SOURCES = \
	xp/tf_test.cpp \
	xp/tf_test.h \
	xp/tf_guard.h \
	xp/tf_actions.h \
	xp/tf_widgets.h \
	$(null)

EXTRA_DIST = \
	xp/t/tf_guard.t.cpp
