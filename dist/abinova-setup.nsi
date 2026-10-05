; abinova-setup.nsi — NSIS installer for the self-contained Windows
; bundle produced by tools/build-windows-msys2.sh --bundle.
;
; Build (inside an MSYS2 shell, or anywhere makensis + the bundle dir
; are reachable):
;
;   makensis -DVERSION=4.0.0 -DARCH=x86_64 \
;     -DSRCDIR='C:\path\to\abinova-4.0.0-windows-x86_64' \
;     dist/abinova-setup.nsi
;
; Produces abinova-<VERSION>-windows-<ARCH>-setup.exe beside the .nsi
; (override with -DOUTFILE=...).  MSYS2 provides makensis via
;   pacman -S mingw-w64-ucrt-x86_64-nsis    (needs NSIS >= 3.05)
;
; To Authenticode-sign the installer AND the uninstaller it embeds,
; pass a sign command (substituting the bundled-file placeholder %1):
;
;   makensis -D... -DSIGNCMD="signtool sign /fd sha256 /td sha256 ^
;     /tr http://timestamp.digicert.com /sha1 <thumbprint> /f" ...
;
; The bundled binaries themselves should be signed FIRST via
; dist/sign-windows.sh (or the build script's ABINOVA_SIGN_* hook) —
; see dist/SIGNING.md.

Unicode true
SetCompressor /SOLID lzma
SetCompressorDictSize 64
ManifestDPIAware true
ManifestLongPathAware true
ManifestSupportedOS Win10

!define APPNAME   "Abinova"
!define PROGID    "Abinova.Document"
!define PUBLISHER "Abinova contributors"
!define APPURL    "https://github.com/janos-szenfner/Abinova"
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Abinova"

!ifndef VERSION
  !error "VERSION is required: makensis -DVERSION=x.y.z"
!endif
!ifndef SRCDIR
  !error "SRCDIR is required: the bundle dir from build-windows-msys2.sh --bundle"
!endif
!ifndef ARCH
  !define ARCH "x86_64"
!endif
!ifndef OUTFILE
  !define OUTFILE "abinova-${VERSION}-windows-${ARCH}-setup.exe"
!endif
!ifndef INSTALLDIR_SUFFIX
  !define INSTALLDIR_SUFFIX "Abinova"
!endif

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"

Name "${APPNAME} ${VERSION}"
OutFile "${OUTFILE}"
; per-machine install; drop to RequestExecutionLevel user +
; $LOCALAPPDATA for an unprivileged variant
InstallDir "$PROGRAMFILES64\${INSTALLDIR_SUFFIX}"
InstallDirRegKey HKLM "Software\${APPNAME}" "InstallDir"
RequestExecutionLevel admin
ShowInstDetails show
ShowUninstDetails show

; sign the installer and the embedded uninstaller it writes, when a
; -DSIGNCMD was supplied (NSIS >= 3.03 !finalize/!uninstfinalize)
!ifdef SIGNCMD
  !finalize '${SIGNCMD} "%1"'
  !uninstfinalize '${SIGNCMD} "%1"'
!endif

!define MUI_ICON   "${SRCDIR}\abinova.ico"
!define MUI_UNICON "${SRCDIR}\abinova.ico"
!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; --------------------------------------------------------------- macros

; own the extension: ProgID + shell open command (only used for
; Abinova-native formats — foreign formats go through OpenWithList so
; existing defaults are never stolen)
!macro RegisterOwnedExt EXT
  WriteRegStr HKCR ".${EXT}" "" "${PROGID}"
!macroend

; remove an owned extension only while it still points at OUR ProgID —
; if the user re-associated .abw (e.g. to upstream AbiWord) after
; install, their choice wins and the key stays
!macro UnregisterOwnedExt EXT
  ReadRegStr $0 HKCR ".${EXT}" ""
  ${If} $0 == "${PROGID}"
    DeleteRegKey HKCR ".${EXT}"
  ${EndIf}
!macroend

; offer a foreign extension in Explorer's "Open with" without
; touching the current default handler
!macro RegisterOpenWith EXT
  WriteRegStr HKCR ".${EXT}\OpenWithList\abinova.exe" "" ""
  WriteRegStr HKCR ".${EXT}\OpenWithProgids" "${PROGID}" ""
  WriteRegStr HKLM "Software\${APPNAME}\Capabilities\FileAssociations" \
    ".${EXT}" "${PROGID}"
!macroend

!macro UnregisterOpenWith EXT
  DeleteRegKey HKCR ".${EXT}\OpenWithList\abinova.exe"
  DeleteRegValue HKCR ".${EXT}\OpenWithProgids" "${PROGID}"
  DeleteRegValue HKLM \
    "Software\${APPNAME}\Capabilities\FileAssociations" ".${EXT}"
!macroend

!define SHCNE_ASSOCCHANGED 0x08000000
!macro RefreshShellAssociations
  System::Call 'shell32::SHChangeNotify(i ${SHCNE_ASSOCCHANGED}, i 0, i 0, i 0)'
!macroend

; write HKLM keys into the 64-bit registry view — matches $PROGRAMFILES64
Function .onInit
  ${If} ${RunningX64}
    SetRegView 64
  ${EndIf}
FunctionEnd

Function un.onInit
  ${If} ${RunningX64}
    SetRegView 64
  ${EndIf}
FunctionEnd

; ------------------------------------------------------------- sections

Section "Abinova (required)" SEC_APP
  SectionIn RO
  SetOutPath "$INSTDIR"
  File /r "${SRCDIR}\*.*"

  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\${APPNAME}" "InstallDir" "$INSTDIR"

  ; Add/Remove Programs entry
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayName" "${APPNAME} ${VERSION}"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${UNINSTKEY}" "Publisher" "${PUBLISHER}"
  WriteRegStr HKLM "${UNINSTKEY}" "URLInfoAbout" "${APPURL}"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayIcon" '"$INSTDIR\bin\abinova.exe",0'
  WriteRegStr HKLM "${UNINSTKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNINSTKEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoRepair" 1
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  WriteRegDWORD HKLM "${UNINSTKEY}" "EstimatedSize" $0

  ; native document types — Abinova owns these extensions
  WriteRegStr HKCR "${PROGID}" "" "Abinova Document"
  WriteRegStr HKCR "${PROGID}\DefaultIcon" "" '"$INSTDIR\abinova.ico"'
  WriteRegStr HKCR "${PROGID}\shell\open\command" "" \
    '"$INSTDIR\bin\abinova.exe" "%1"'
  !insertmacro RegisterOwnedExt "abwn"
  !insertmacro RegisterOwnedExt "abw"
  !insertmacro RegisterOwnedExt "zabw"
  !insertmacro RegisterOwnedExt "zabwn"
  !insertmacro RegisterOwnedExt "awt"

  ; foreign formats — listed in "Open with" + Default Apps, existing
  ; associations untouched
  !insertmacro RegisterOpenWith "docx"
  !insertmacro RegisterOpenWith "docm"
  !insertmacro RegisterOpenWith "doc"
  !insertmacro RegisterOpenWith "odt"
  !insertmacro RegisterOpenWith "ott"
  !insertmacro RegisterOpenWith "fodt"
  !insertmacro RegisterOpenWith "rtf"
  !insertmacro RegisterOpenWith "wpd"
  !insertmacro RegisterOpenWith "wps"
  !insertmacro RegisterOpenWith "epub"
  !insertmacro RegisterOpenWith "mht"
  !insertmacro RegisterOpenWith "mhtml"
  !insertmacro RegisterOpenWith "md"
  !insertmacro RegisterOpenWith "tex"

  WriteRegStr HKLM "Software\${APPNAME}\Capabilities" \
    "ApplicationName" "${APPNAME}"
  WriteRegStr HKLM "Software\${APPNAME}\Capabilities" \
    "ApplicationDescription" "A GTK4 word processor"
  WriteRegStr HKLM "Software\${APPNAME}\Capabilities\FileAssociations" \
    ".abwn" "${PROGID}"
  WriteRegStr HKLM "Software\${APPNAME}\Capabilities\FileAssociations" \
    ".abw" "${PROGID}"
  WriteRegStr HKLM "Software\${APPNAME}\Capabilities\FileAssociations" \
    ".zabw" "${PROGID}"
  WriteRegStr HKLM "Software\${APPNAME}\Capabilities\FileAssociations" \
    ".zabwn" "${PROGID}"
  WriteRegStr HKLM "Software\${APPNAME}\Capabilities\FileAssociations" \
    ".awt" "${PROGID}"
  WriteRegStr HKLM "Software\RegisteredApplications" "${APPNAME}" \
    "Software\${APPNAME}\Capabilities"

  ; App Paths — "start abinova" / URL-less invocation resolves the exe
  WriteRegStr HKLM \
    "Software\Microsoft\Windows\CurrentVersion\App Paths\abinova.exe" \
    "" "$INSTDIR\bin\abinova.exe"
  WriteRegStr HKLM \
    "Software\Microsoft\Windows\CurrentVersion\App Paths\abinova.exe" \
    "Path" "$INSTDIR\bin"

  !insertmacro RefreshShellAssociations
SectionEnd

Section "Start Menu shortcuts" SEC_STARTMENU
  CreateDirectory "$SMPROGRAMS\${APPNAME}"
  CreateShortcut "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk" \
    "$INSTDIR\bin\abinova.exe" "" "$INSTDIR\bin\abinova.exe" 0
  CreateShortcut "$SMPROGRAMS\${APPNAME}\Uninstall.lnk" \
    "$INSTDIR\Uninstall.exe"
SectionEnd

Section "Desktop shortcut" SEC_DESKTOP
  CreateShortcut "$DESKTOP\${APPNAME}.lnk" "$INSTDIR\bin\abinova.exe"
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_APP} \
    "The Abinova word processor and its bundled runtime libraries."
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_STARTMENU} \
    "Shortcuts in the Start Menu."
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_DESKTOP} \
    "A shortcut on the desktop."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; ------------------------------------------------------------ uninstall

Section "Uninstall"
  ; file associations we own — removed only while they still point at us
  !insertmacro UnregisterOwnedExt "abwn"
  !insertmacro UnregisterOwnedExt "abw"
  !insertmacro UnregisterOwnedExt "zabw"
  !insertmacro UnregisterOwnedExt "zabwn"
  !insertmacro UnregisterOwnedExt "awt"
  DeleteRegKey HKCR "${PROGID}"
  !insertmacro UnregisterOpenWith "docx"
  !insertmacro UnregisterOpenWith "docm"
  !insertmacro UnregisterOpenWith "doc"
  !insertmacro UnregisterOpenWith "odt"
  !insertmacro UnregisterOpenWith "ott"
  !insertmacro UnregisterOpenWith "fodt"
  !insertmacro UnregisterOpenWith "rtf"
  !insertmacro UnregisterOpenWith "wpd"
  !insertmacro UnregisterOpenWith "wps"
  !insertmacro UnregisterOpenWith "epub"
  !insertmacro UnregisterOpenWith "mht"
  !insertmacro UnregisterOpenWith "mhtml"
  !insertmacro UnregisterOpenWith "md"
  !insertmacro UnregisterOpenWith "tex"

  DeleteRegKey HKLM \
    "Software\Microsoft\Windows\CurrentVersion\App Paths\abinova.exe"
  DeleteRegKey HKLM "Software\${APPNAME}"
  DeleteRegValue HKLM "Software\RegisteredApplications" "${APPNAME}"
  DeleteRegKey HKLM "${UNINSTKEY}"

  Delete "$DESKTOP\${APPNAME}.lnk"
  Delete "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk"
  Delete "$SMPROGRAMS\${APPNAME}\Uninstall.lnk"
  RMDir "$SMPROGRAMS\${APPNAME}"

  Delete "$INSTDIR\Uninstall.exe"
  RMDir /r "$INSTDIR"

  !insertmacro RefreshShellAssociations
SectionEnd
