# PACK01 dependency manifest — Abinova 4.0.0

Measured on Linux x86-64 (Ubuntu-family, Zorin) from
`ldd src/.libs/libabinova-4.0.so` + a `readelf -d` NEEDED-edge map,
2026-10-05. Re-run the measurement after any dependency-changing work
(new -l flags, vendored-lib swaps, importer additions).

The `abinova` executable needs only `libabinova-4.0.so` + `libc.so.6` —
the shared library carries the entire dependency surface.

Classes:
- `system`  — kernel/glibc contract, present on every Linux host;
              bundling is actively harmful (vdso, ld.so, glibc).
- `bundle`  — copy into the package and relocate (rpath $ORIGIN /
              @executable_path / beside-the-exe).
- `vendored`— built from `thirdparty/` and linked statically into
              `libabinova-4.0.so`; nothing to ship.
- `runtime` — dlopen'd / module-loaded / data files; invisible to ldd.
              See `dist/RUNTIME-DEPS.md` for the seed list.

## 1. Direct NEEDED entries of libabinova-4.0.so (27)

| Library | Role | Class |
|---|---|---|
| libgtk-4.so.1 | GTK4 UI toolkit | bundle |
| libgdk_pixbuf-2.0.so.0 | image loading | bundle |
| libgraphene-1.0.so.0 | GTK4 geometry/types | bundle |
| libpango-1.0.so.0 | text shaping | bundle |
| libpangocairo-1.0.so.0 | text rendering on cairo | bundle |
| libfontconfig.so.1 | font discovery | bundle |
| libcairo.so.2 | 2D rendering (screen + PDF/PS export) | bundle |
| libfribidi.so.0 | bidi algorithm | bundle |
| libglib-2.0.so.0 / libgobject-2.0.so.0 / libgio-2.0.so.0 | GLib core stack | bundle |
| libgsf-1.so.114 | structured file I/O (zip/ole/streams) | bundle |
| libxml2.so.2 | XML parse/serialize | bundle |
| libxslt.so.1 | XSLT (RDF/ODF transforms) | bundle |
| librsvg-2.so.2 | SVG rendering | bundle |
| libenchant-2.so.2 | spell-check front-end | bundle |
| libwmf-0.2.so.7 / libwmflite-0.2.so.7 | WMF image import | bundle |
| libpng16.so.16 | PNG read/write | bundle |
| libjpeg.so.8 | JPEG read (turbo ABI) | bundle |
| libz.so.1 | deflate | bundle |
| libX11.so.6 | X11 client (also pulled by gtk) | bundle |
| libstdc++.so.6 | C++ runtime — bundle; hosts may ship older | bundle |
| libgcc_s.so.1 | GCC runtime | bundle |
| libc.so.6 | glibc | system |
| libm.so.6 | glibc | system |
| ld-linux-x86-64.so.2 | ELF loader | system |

## 2. Transitive closure (50 libs — every one must resolve if its
   parent is bundled, so they are all `bundle`)

| Library | Pulled by | Class |
|---|---|---|
| libgmodule-2.0.so.0 | gio, gdk_pixbuf, gtk, enchant | bundle |
| libcairo-gobject.so.2 | gtk, rsvg | bundle |
| libpangoft2-1.0.so.0 | pangocairo, gtk | bundle |
| libharfbuzz.so.0 | pango, gtk | bundle |
| libgraphite2.so.3 | harfbuzz | bundle |
| libthai.so.0 | pango (Thai line-break engine) | bundle |
| libdatrie.so.1 | thai | bundle |
| libepoxy.so.0 | gtk (GL dispatch) | bundle |
| libvulkan.so.1 | gtk (hard NEEDED of the gtk build; app works on non-vulkan hosts only if the lib still resolves — bundle it) | bundle |
| libtiff.so.6 | gtk (TIFF pixbuf support) | bundle |
| libwebp.so.7 | tiff | bundle |
| libsharpyuv.so.0 | webp | bundle |
| libzstd.so.1 | tiff | bundle |
| liblzma.so.5 | xml2, tiff | bundle |
| libLerc.so.4 | tiff | bundle |
| libjbig.so.0 | tiff | bundle |
| libdeflate.so.0 | tiff | bundle |
| libbz2.so.1.0 | gsf, freetype | bundle |
| libxkbcommon.so.0 | gtk | bundle |
| libwayland-client.so.0 / libwayland-egl.so.1 | gtk (Wayland backend) | bundle |
| libXext.so.6 / libXi.so.6 / libXcursor.so.1 / libXdamage.so.1 / libXfixes.so.3 / libXrandr.so.2 / libXinerama.so.1 | gtk (X11 backend) | bundle |
| libxcb.so.1 / libxcb-render.so.0 / libxcb-shm.so.0 | X11, cairo | bundle |
| libXau.so.6 / libXdmcp.so.6 | xcb | bundle |
| libbsd.so.0 | Xdmcp (arc4random) | bundle |
| libmd.so.0 | bsd | bundle |
| libXrender.so.1 | cairo, Xcursor, Xrandr | bundle |
| libpixman-1.so.0 | cairo | bundle |
| libcairo-script-interpreter.so.2 | gtk (Debian cairo feature) | bundle |
| liblzo2.so.2 | cairo-script-interpreter | bundle |
| libfreetype.so.6 | cairo, fontconfig, pangoft2, harfbuzz, wmf | bundle |
| libbrotlidec.so.1 / libbrotlicommon.so.1 | freetype (WOFF2 fonts) | bundle |
| libexpat.so.1 | fontconfig, wmf | bundle |
| libmount.so.1 | gio (unix volume monitor) | bundle |
| libselinux.so.1 | gio, mount | bundle |
| libblkid.so.1 | mount | bundle |
| libffi.so.8 | gobject, wayland-client | bundle |
| libpcre2-8.so.0 | glib, selinux | bundle |
| libicuuc.so.74 / libicudata.so.74 | xml2 (Debian links ICU) | bundle |
| linux-vdso.so.1 | kernel (not a file) | system |

Total: 78 ldd entries = 27 direct + 50 transitive + vdso.

### Safe-system boundary

Only `libc.so.6`, `libm.so.6`, `ld-linux-x86-64.so.2` and `linux-vdso`
are classified `system`. Everything else — including libstdc++,
libgcc_s and the selinux/mount/blkid tail that distros otherwise
guarantee — is `bundle` because a self-contained package must not
assume them. Revisit only if PACK08's clean-container test shows a
bundled lib fighting the host (SELinux-integrated libselinux is the
known-risky candidate).

## 3. Vendored into libabinova (nothing to ship)

| Library | Source dir | Notes |
|---|---|---|
| hunspell 1.7.4 | thirdparty/hunspell-1.7.4 | libhunspell.la, noinst (the `hunspell-1.7.0/` dir is stale leftover — unreferenced by Makefile.am) |
| libwv (wvWare 1.2.9) | thirdparty/wv-1.2.9 | libwv.la, noinst — .doc import |
| librevenge 0.0.6 | thirdparty/librevenge-0.0.6 | noinst — libwpd/libwps base |
| libwpd 0.10.3 | thirdparty/libwpd-0.10.3 | noinst — .wpd import |
| libwpg 0.3.4 | thirdparty/libwpg-0.3.4 | noinst — WPG graphics |
| libwps 0.4.14 | thirdparty/libwps-0.4.14 | noinst — MS Works import |
| boost (header-only) | system headers | `boost/algorithm/string.hpp` in ODe_ManifestWriter.cpp; AX_BOOST_BASE(1.83). No compiled boost lib is linked — nothing to bundle. |

## 4. Runtime-loaded modules (invisible to ldd — copy + locate)

| Module | Found at (this box) | Needed for |
|---|---|---|
| enchant-2 backends | /usr/lib/x86_64-linux-gnu/enchant-2/{hunspell,aspell,hspell}.so | spellcheck |
| gdk-pixbuf loaders + loaders.cache | /usr/lib/x86_64-linux-gnu/gdk-pixbuf-2.0/2.10.0/loaders/ (15 loaders incl. svg/heif/webp/tiff/wmf) | image decode at insert/open |
| GIO modules | .../gio/modules/{libdconfsettings,libgiognomeproxy,libgiognutls,libgiolibproxy,libgioremote-volume-monitor,libgvfsdbus}.so | dconf settings, proxies, **TLS (libgiognutls — update check)**, GVFS |
| GTK4 modules | .../gtk-4.0/4.0.0/{printbackends/libprintbackend-cups,file.so, immodules/libim-ibus.so, media/libmedia-gstreamer.so} | printing, CJK input, **embedded-media playback (MED01)** |
| GStreamer libs + plugins | gstreamer-1.0 lib set + /usr/lib/x86_64-linux-gnu/gstreamer-1.0/*.so | pulled by libmedia-gstreamer — bundle or media objects won't play |
| libcrypto (OpenSSL/LibreSSL) | dlopen candidate list in ut_abwncrypt.cpp (PORT12/13) | encrypted .abwn |
| gschemas.compiled | /usr/share/glib-2.0/schemas/ | GSettings-backed prefs |
| CA certificate bundle | /etc/ssl/certs | TLS verification (update check) |
| hunspell dictionaries | PORT04 search-path list | spellcheck dictionaries |
| hyphenation dictionaries | /usr/share/hyphen/hyph_*.dic (ABINOVA_HYPHEN_PATH) | auto-hyphenation (RBN06) |
| fontconfig config + fonts | /etc/fonts/fonts.conf + font dirs | font resolution — bundle on win/mac (PACK07), system on Linux |
| artwork/ tree | in-repo `artwork/` | ribbon Shapes/3D/Icons galleries (PACK07) |
| gconv modules | glibc-internal | iconv charset conversion — safe-system |
| XDG Screenshot portal + session bus | D-Bus service | RBN01 screenshot — service dep, cannot be bundled |

## 5. Platform notes for PACK03-05

- **Linux (PACK03)**: bundle everything marked `bundle` under
  dist/lib + `patchelf --set-rpath '$ORIGIN/../lib'` on both the exe
  and each bundled lib (transitive NEEDEDs resolve by soname against
  the same dir).
- **macOS (PACK04)**: same set as .dylib via brew closure;
  install_name_tool to @executable_path/../Frameworks; Quartz backend
  replaces the X11/Wayland subtrees.
- **Windows (PACK05)**: MSYS2 DLL equivalents beside the exe; the
  X11/Wayland/cairo-script subtrees collapse; libcrypto resolves via
  the PORT13 LoadLibrary candidate list.
