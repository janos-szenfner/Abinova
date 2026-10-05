# Runtime-loaded dependencies (invisible to ldd/otool)

Seed list for PACK01's `dist/BUNDLED-DEPS` manifest. Everything here is
loaded at runtime — via `dlopen`, GIO modules, gdk-pixbuf loaders or
GSettings schemas — so none of it appears in the linker's NEEDED list.
A bundle that copies only `ldd` output will ship these features dead.

| Module | Loaded by | Loaded how | Failure mode if missing |
|---|---|---|---|
| libcrypto (OpenSSL 3 / 1.1 or LibreSSL) | `src/wp/impexp/xp/ut_abwncrypt.cpp` (.abwn AES-256-GCM) | `dlopen`/`LoadLibrary` of a candidate list; macOS tries `libcrypto.3.dylib`/`libcrypto.dylib` + Homebrew/MacPorts/system paths; Windows tries `libcrypto-3-x64.dll`/`-arm64.dll`/`libcrypto-3.dll`/`libcrypto-1_1*.dll`/`libcrypto.dll` (exe dir + System32 first, then standard search) | `.abwn` open/save of encrypted docs fails "unavailable" |
| enchant-2 provider backends (hunspell, aspell, ...) | enchant-2 library | `g_module_open` of `enchant/*.so` under libdir | spellcheck silently reports no dictionaries/providers |
| gdk-pixbuf loaders (+ loaders.cache) | gtk/gdk-pixbuf | `g_module_open`; loaders.cache read from `GDK_PIXBUF_MODULE_FILE`/libdir | image formats decode-fail at insert/open |
| GIO modules (incl. glib-networking TLS backend) | GIO/GTLS | `g_io_modules_scan_all_in_directory*` | `GTlsBackend` absent — update check reports "TLS support is unavailable" |
| CA certificate bundle | glib-networking/openssl | file read | TLS verification fails |
| glib schemas (gschemas.compiled) | GSettings | `g_settings` schema lookup (set `GSETTINGS_SCHEMA_DIR` in-bundle) | settings-backed prefs/dialogs misbehave |
| hunspell dictionaries | HunspellWrap (PORT04 path list) | file read | no spell dictionaries found |

See PACK01 (manifest), PACK06 (runtime-module bundling), PACK07
(dictionaries/artwork) in `.devin/TASKS.md`.
