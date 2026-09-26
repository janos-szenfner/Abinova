/* Static config.h for the vendored wv build (no autoconf). */
#ifndef WV_BUNDLED_CONFIG_H
#define WV_BUNDLED_CONFIG_H

#define VERSION "1.2.9"
#define PACKAGE "wv"
#define PACKAGE_NAME "wv"

#define HAVE_ZLIB 1
#define HAVE_LIBXML2 1
#define HAVE_STRING_H 1

#ifndef _WIN32
#define HAVE_UNISTD_H 1
#ifdef __GLIBC__
#define HAVE_MALLOC_H 1
#endif
#endif

#endif /* WV_BUNDLED_CONFIG_H */
