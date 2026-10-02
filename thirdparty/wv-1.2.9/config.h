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

/* all wv stream I/O is little-endian; defining MATCHED_TYPE on
   little-endian hosts makes the TO_LE_* macros in support.c no-ops.
   Without it write_*ubit emits big-endian while read_*ubit expects
   little-endian, corrupting every synthesized record (e.g. the
   escher wrapper wvGetPICF builds for pre-Word8 picture data) */
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define MATCHED_TYPE 1
#elif defined(_WIN32)
/* Windows targets are always little-endian */
#define MATCHED_TYPE 1
#endif

#endif /* WV_BUNDLED_CONFIG_H */
