# Runtime-loaded dependencies (invisible to ldd/otool)

Seed list for PACK01's `dist/BUNDLED-DEPS` manifest. Everything here is
loaded at runtime — via `dlopen`, GIO modules, gdk-pixbuf loaders or
GSettings schemas — so none of it appears in the linker's NEEDED list.
A bundle that copies only `ldd` output will ship these features dead.

PACK06 ships them; the "Located by" column records how the bundle makes
each one findable at runtime.

| Module | Loaded by | Loaded how | Located by (bundle) | Failure mode if missing |
|---|---|---|---|---|
| libcrypto (OpenSSL 3 / 1.1 or LibreSSL) | `src/wp/impexp/xp/ut_abwncrypt.cpp` (.abwn AES-256-GCM) | `dlopen`/`LoadLibrary` of a candidate list; macOS tries `libcrypto.3.dylib`/`libcrypto.dylib` + Homebrew/MacPorts/system paths; Windows tries `libcrypto-3-x64.dll`/`-arm64.dll`/`libcrypto-3.dll`/`libcrypto-1_1*.dll`/`libcrypto.dll` (exe dir + System32 first, then standard search) | Linux: soname copied into `lib/` (RUNPATH closure picks it up since the dlopen search honors the module's own rpath + the closure). Windows: beside `abinova.exe`. macOS: brew dylib closure. | `.abwn` open/save of encrypted docs fails "unavailable" |
| enchant-2 provider backends (hunspell, aspell, ...) | enchant-2 library | `g_module_open` of `enchant/*.so` under its compiled-in libdir | `lib/enchant-2/` (+ `lib/<multiarch>/enchant-2` mirror for relocatable builds). When the broker finds zero providers the app **self-loads** them via `init_enchant_provider`/`EnchantProvider` (enchant_checker.cpp); `ABINOVA_ENCHANT_MODULE_PATH` overrides the probe. Windows enchant resolves `lib/enchant-2` from its own DLL. | spellcheck silently reports no dictionaries/providers; session/PWL wordlists degrade to no-ops in self-loaded mode |
| gdk-pixbuf loaders (+ loaders.cache) | gtk/gdk-pixbuf | `g_module_open`; loaders.cache read from `GDK_PIXBUF_MODULE_FILE`/libdir | `lib/gdk-pixbuf-2.0/<ver>/loaders*` with cache-relative module paths; at startup the app rewrites the cache to absolute paths in `$XDG_CACHE_HOME/abinova/` and exports `GDK_PIXBUF_MODULE_FILE` (non-relocatable pixbuf builds — everything but Windows — would otherwise resolve them against CWD) | image formats decode-fail at insert/open |
| GIO modules (incl. glib-networking TLS backend) | GIO/GTLS | `g_io_modules_scan_all_in_directory*` | `lib/gio/modules/` + regenerated `giomodule.cache`; app exports `GIO_EXTRA_MODULES` (scanned before the compiled dir) | `GTlsBackend` absent — update check reports "TLS support is unavailable" |
| CA certificate bundle | glib-networking/openssl | file read | `certs/ca-certificates.crt` at the bundle root; `xap_UpdateCheck` loads it into a `GTlsFileDatabase` on each TLS connection (`GSocketClient::event` `TLS_HANDSHAKING` hook); `ABINOVA_CA_BUNDLE` overrides | TLS verification fails |
| glib schemas (gschemas.compiled) | GSettings | `g_settings` schema lookup | `share/glib-2.0/schemas/`; app exports `GSETTINGS_SCHEMA_DIR` when the compiled cache exists (Windows GLib also finds it via its own prefix probe) | settings-backed prefs/dialogs misbehave |
| GTK4 module dirs | gtk | `g_module_open` under `GTK_PATH`/`GTK_EXE_PREFIX`-derived dirs | `lib/gtk-4.0/<ver>/{immodules,media,printbackends}` + their `giomodule.cache`; app exports `GTK_PATH=<root>/lib/gtk-4.0` | no CUPS/file print backends, no IM context modules, no media playback backend |
| GStreamer plugins | libmedia-gstreamer | gst registry scan of `GST_PLUGIN_SYSTEM_PATH_1_0` | `lib/gstreamer-1.0/*.so`; app exports `GST_PLUGIN_SYSTEM_PATH_1_0` (replaces the default scan — keeps a bundle self-contained) | embedded media silently unplayable |
| hunspell dictionaries | HunspellWrap (PORT04 path list) | file read | PACK07 | no spell dictionaries found |

See PACK01 (manifest), PACK06 (runtime-module bundling), PACK07
(dictionaries/artwork) in `.devin/TASKS.md`.
