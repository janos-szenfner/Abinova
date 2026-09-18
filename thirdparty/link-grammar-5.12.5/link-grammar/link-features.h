#ifndef LINK_FEATURES_H
#define LINK_FEATURES_H

#if (defined(_WIN32) || defined(__CYGWIN__)) && !defined(LINK_GRAMMAR_DLL_EXPORT)
#define LINK_GRAMMAR_DLL_EXPORT 1
#endif

#ifdef  __cplusplus
# define LINK_BEGIN_DECLS  extern "C" {
# define LINK_END_DECLS    }
#else
# define LINK_BEGIN_DECLS
# define LINK_END_DECLS
#endif

#ifndef link_public_api
# if (defined(_WIN32) || defined(__CYGWIN__))
#  if defined(LINK_GRAMMAR_STATIC)
#   define link_public_api(x)
#  else
#   if !defined LINK_GRAMMAR_DLL_EXPORT
#    error !defined LINK_GRAMMAR_DLL_EXPORT
#   endif
#   if LINK_GRAMMAR_DLL_EXPORT
#    define link_public_api(x) __declspec(dllexport) x
#   else
#    define link_public_api(x) __declspec(dllimport) x
#   endif
#  endif
# else
#  define link_public_api(x) __attribute__((__visibility__("default"))) x
# endif
#endif

/* Users are discouraged from using functions in the experimental API,
 * unless they are willing to commit to the possibility that these functions
 * may change without notice or may simply be removed at any time. */
#define link_experimental_api(x) link_public_api(x)

#define LINK_MAJOR_VERSION 5
#define LINK_MINOR_VERSION 12
#define LINK_MICRO_VERSION 5

#define LINK_VERSION_STRING "5.12.5"

/* __VA_ARGS__ must be used because arguments may contain commas. */
#define lg_xstr(...) lg_str(__VA_ARGS__)
#define lg_str(...) #__VA_ARGS__

#define LG_HOST_OS lg_str(linux-gnu)
#define LG_CPPFLAGS "CPPFLAGS=" lg_str()
#define LG_CFLAGS "CFLAGS=" lg_str(-D_DEFAULT_SOURCE -std=c11 -D_BSD_SOURCE -D_SVID_SOURCE -D_GNU_SOURCE -D_ISOC11_SOURCE  -I/tmp/vendor/flex/usr/include)
#define LG_DEFS lg_str(-DPACKAGE_NAME="link-grammar" -DPACKAGE_TARNAME="link-grammar" -DPACKAGE_VERSION="5.12.5" -DPACKAGE_STRING="link-grammar 5.12.5" -DPACKAGE_BUGREPORT="https://github.com/opencog/link-grammar" -DPACKAGE_URL="https://opencog.github.io/link-grammar-website" -DPACKAGE="link-grammar" -DVERSION="5.12.5" -DHAVE_STDIO_H=1 -DHAVE_STDLIB_H=1 -DHAVE_STRING_H=1 -DHAVE_INTTYPES_H=1 -DHAVE_STDINT_H=1 -DHAVE_STRINGS_H=1 -DHAVE_SYS_STAT_H=1 -DHAVE_SYS_TYPES_H=1 -DHAVE_UNISTD_H=1 -DHAVE_THREADS_H=1 -DSTDC_HEADERS=1 -DHAVE_DLFCN_H=1 -DLT_OBJDIR=".libs/" -DYYTEXT_POINTER=1 -DHAVE___BUILTIN_TYPES_COMPATIBLE_P=1 -DHAVE_TYPEOF=1 -Dtypeof=__typeof__ -DHAVE_STRNDUP=1 -DHAVE_STRTOK_R=1 -DHAVE_SIGACTION=1 -DHAVE_MALLOC_TRIM=1 -DHAVE_ASPRINTF=1 -DHAVE_ALIGNED_ALLOC=1 -DHAVE_POSIX_MEMALIGN=1 -DHAVE_FORK=1 -DHAVE_VFORK=1 -DHAVE_WORKING_VFORK=1 -DHAVE_WORKING_FORK=1 -DHAVE_PRCTL=1 -DHAVE_ALLOCA_H=1 -DHAVE_ALLOCA=1 -DTLS=_Thread_local -DHAVE_PTHREAD_PRIO_INHERIT=1 -DHAVE_PTHREAD=1 -DHAVE_VISIBILITY=1 -D__STDC_FORMAT_MACROS=1 -D__STDC_LIMIT_MACROS=1 -DHAVE_LOCALE_T_IN_LOCALE_H=1 -DHAVE_STDATOMIC_H=1 -DUSE_WORDGRAPH_DISPLAY=1 -DHAVE_PCRE2_H=1 -DHAVE_MAYBE_UNINITIALIZED=1 -DHAVE_DECL_STRERROR_R=1 -DHAVE_STRERROR_R=1 -DSTRERROR_R_CHAR_P=1)

#define DISCUSSION_GROUP "https://groups.google.com/d/forum/link-grammar"
#define OVERVIEW "https://en.wikipedia.org/wiki/Link_grammar"
/* Must appear here to support Windows. */
#undef PACKAGE_BUGREPORT
#define PACKAGE_BUGREPORT "https://github.com/opencog/link-grammar"
#undef PACKAGE_URL
#define PACKAGE_URL "https://opencog.github.io/link-grammar-website"

#endif
