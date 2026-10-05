# Signing & installer layer (PACK09)

The pack scripts (`dist/linux-bundle.sh`, `tools/build-macos.sh
--bundle`, `tools/build-windows-msys2.sh --bundle`) produce
self-contained, relocatable bundles. This document covers the layer on
top: signing, notarization and per-OS installers.

No certificate material is ever committed — every path below reads
credentials from the environment or the OS certificate store.

## macOS — codesign + notarization

`dist/sign-macos.sh` signs an already-assembled `Abinova.app` (it is
also invoked automatically at the end of `build-macos.sh --bundle`):

```bash
# development / local run — ad-hoc signature (the default). This is
# the minimum Apple Silicon needs to launch the binary at all.
dist/sign-macos.sh dist/Abinova.app

# release — Developer-ID signature with hardened runtime + timestamp
export ABINOVA_CODESIGN_IDENTITY="Developer ID Application: Name (TEAMID)"
dist/sign-macos.sh dist/Abinova.app

# release + notarization (staples the ticket into the .app)
export ABINOVA_NOTARY_PROFILE="abinova-notary"
dist/sign-macos.sh dist/Abinova.app
```

Equivalent flags on `tools/build-macos.sh`: `--sign-identity "..."`,
`--notarize-profile <profile>`, `--no-sign`.

Requirements for a release signature:

- A **Developer ID Application** certificate in the login keychain
  (Apple Developer Program membership; create/download via Xcode →
  Settings → Accounts → Manage Certificates, or
  https://developer.apple.com/account/resources/certificates).
- Signing applies `--options runtime` (hardened runtime — mandatory
  for notarization; the app requests no special entitlements) and
  `--timestamp` (Apple's timestamp server).
- Notarization additionally needs a `notarytool` keychain profile:

  ```bash
  xcrun notarytool store-credentials "abinova-notary" \
    --apple-id you@example.com --team-id TEAMID \
    --password <app-specific-password>
  ```

- The submit leg runs `ditto -c -k --keepParent` →
  `xcrun notarytool submit --wait` → `xcrun stapler staple`.
  Ad-hoc-signed apps are rejected by the notary service, so
  `--notarize-profile` without an identity is refused.

Verification of a signed app on the target machine:

```bash
codesign --verify --deep --strict --verbose=2 Abinova.app   # done by the script
spctl -a -vv Abinova.app        # "accepted" only for Developer-ID + notarized
xcrun stapler validate Abinova.app
```

## Windows — Authenticode + NSIS installer

`dist/sign-windows.sh` signs every PE file (`.exe`/`.dll`) in the
bundle with `signtool`:

```bash
# inside an MSYS2 shell, after tools/build-windows-msys2.sh --bundle
export ABINOVA_SIGN_SHA1=<cert thumbprint>        # cert-store route
# or: export ABINOVA_SIGN_PFX=<file.pfx> ABINOVA_SIGN_PFX_PASSWORD=...
dist/sign-windows.sh dist/abinova-4.0.0-windows-x86_64
```

The build script calls it automatically when `ABINOVA_SIGN_SHA1` or
`ABINOVA_SIGN_PFX` is set. Signature spec: `/fd sha256 /td sha256`
dual digest + RFC3161 timestamp (`ABINOVA_SIGN_TSA`, default
DigiCert). `signtool` ships with the Windows SDK
(`Windows Kits\10\bin\<ver>\x64\signtool.exe`); point
`ABINOVA_SIGNTOOL` at it when it is not on PATH.

Certificates: any CA-issued **code-signing** certificate works
(OV certs can live in a PFX; EV certs live on hardware tokens — use
the store/thumbprint route, which is why both forms exist). The
signtool password `/p` argument is visible in the process list — on
shared/CI machines prefer the certificate store.

The installer is `dist/abinova-setup.nsi` (NSIS >= 3.03,
`pacman -S mingw-w64-ucrt-x86_64-nsis` in MSYS2):

```bash
makensis -DVERSION=4.0.0 -DARCH=x86_64 \
  -DSRCDIR='C:\path\to\abinova-4.0.0-windows-x86_64' \
  dist/abinova-setup.nsi
```

It installs the bundle tree to `%PROGRAMFILES64%\Abinova`, registers
`.abwn`/`.abw`/`.zabw`/`.zabwn`/`.awt` under the `Abinova.Document`
ProgID (owned associations), lists the app in "Open with" / Default
Apps for `.docx` `.docm` `.doc` `.odt` `.ott` `.fodt` `.rtf` `.wpd`
`.wps` `.epub` `.mht` `.mhtml` `.md` `.tex` without stealing existing
defaults, adds Start-Menu + optional desktop shortcuts, writes the
Add/Remove Programs entry, and embeds a signed-aware uninstaller
(reverses everything including the registry).

To sign the installer and its embedded uninstaller during the build:

```bash
makensis -DVERSION=... -DARCH=... -DSRCDIR=... \
  -DSIGNCMD="signtool sign /fd sha256 /td sha256 \
             /tr http://timestamp.digicert.com /sha1 <thumbprint>" \
  dist/abinova-setup.nsi
```

`build-windows-msys2.sh --bundle` runs makensis automatically when it
is on PATH and forwards the `ABINOVA_SIGN_*` credentials as `-DSIGNCMD`.
Signing order matters: sign the bundle's PE files **before** makensis
packs them, and let `!finalize` sign the produced setup.exe.

## Linux — desktop integration (no signing step)

Linux bundles ship unsigned (that is the norm for tarball/dir
distribution; signing arrives with real package formats — see the
Flatpak task for the store-signed route). The bundle carries
`install-desktop.sh` / `uninstall-desktop.sh` which wire it into the
desktop environment:

```bash
abinova-4.0.0-linux-x86_64/install-desktop.sh            # ~/.local/share
sudo abinova-4.0.0-linux-x86_64/install-desktop.sh --system  # /usr/local/share
```

The installer rewrites the `.desktop` `Exec=` line to the bundle's
absolute `bin/abinova` (the bundle is relocatable, so this is resolved
at install time, not bundle time), installs the hicolor icons, an
`application/x-abinova` shared-mime-info package (`.abwn` + compressed
variants) and the AppStream metainfo, then refreshes
update-desktop-database / update-mime-database / icon caches when the
tools exist. Re-run it after moving the bundle directory.
`uninstall-desktop.sh` removes exactly the installed files.
