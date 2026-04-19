; SPDX-License-Identifier: GPL-3.0-or-later
; SPDX-FileCopyrightText: 2026 tjdeveng
;
; KeepTower NSIS Installer Script
;
; Usage (called from package-windows.sh):
;   makensis -DVERSION=v0.4.0 -DDIST_DIR=dist\keeptower-v0.4.0-windows-x86_64 \
;            -DOUTFILE=dist\keeptower-v0.4.0-setup.exe scripts\keeptower.nsi

Unicode True

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"

; ---------------------------------------------------------------------------
; Metadata
; ---------------------------------------------------------------------------
!ifndef VERSION
  !define VERSION "dev"
!endif
!ifndef DIST_DIR
  !define DIST_DIR "dist\keeptower-dev-windows-x86_64"
!endif
!ifndef OUTFILE
  !define OUTFILE "dist\keeptower-${VERSION}-setup.exe"
!endif

Name                "KeepTower ${VERSION}"
OutFile             "${OUTFILE}"
InstallDir          "$PROGRAMFILES64\KeepTower"
InstallDirRegKey    HKLM "Software\KeepTower" "InstallDir"
RequestExecutionLevel admin
SetCompressor       /SOLID lzma
SetCompressorDictSize 32

BrandingText        "KeepTower ${VERSION} — Password Vault"

; ---------------------------------------------------------------------------
; MUI2 settings
; ---------------------------------------------------------------------------
!define MUI_ABORTWARNING
!define MUI_ICON          "..\data\icons\hicolor\scalable\apps\com.tjdeveng.keeptower.png"
!define MUI_UNICON        "..\data\icons\hicolor\scalable\apps\com.tjdeveng.keeptower.png"

!define MUI_WELCOMEFINISHPAGE_BITMAP_NOSTRETCH
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT

; Welcome page text
!define MUI_WELCOMEPAGE_TITLE    "Welcome to KeepTower ${VERSION}"
!define MUI_WELCOMEPAGE_TEXT     "KeepTower is a secure password vault application.$\r$\n$\r$\nThis installer will guide you through the installation.$\r$\n$\r$\nClick Next to continue."

; Finish page — offer to launch the app
!define MUI_FINISHPAGE_RUN         "$INSTDIR\keeptower.exe"
!define MUI_FINISHPAGE_RUN_TEXT    "Launch KeepTower"
!define MUI_FINISHPAGE_SHOWREADME  "$INSTDIR\README.md"
!define MUI_FINISHPAGE_SHOWREADME_TEXT "View README"

; ---------------------------------------------------------------------------
; Installer pages
; ---------------------------------------------------------------------------
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE    "..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; ---------------------------------------------------------------------------
; Version info embedded in the .exe
; ---------------------------------------------------------------------------
VIProductVersion    "0.4.0.0"
VIAddVersionKey     "ProductName"      "KeepTower"
VIAddVersionKey     "ProductVersion"   "${VERSION}"
VIAddVersionKey     "CompanyName"      "tjdeveng"
VIAddVersionKey     "LegalCopyright"   "Copyright 2026 tjdeveng"
VIAddVersionKey     "FileDescription"  "KeepTower Password Vault Installer"
VIAddVersionKey     "FileVersion"      "${VERSION}"

; ---------------------------------------------------------------------------
; Installer sections
; ---------------------------------------------------------------------------
Section "KeepTower" SecMain
  SectionIn RO   ; Required — cannot be deselected

  SetOutPath "$INSTDIR"
  File /r "${DIST_DIR}\*.*"

  ; Write install path to registry (for uninstaller and repair)
  WriteRegStr HKLM "Software\KeepTower" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "Software\KeepTower" "Version"    "${VERSION}"

  ; Add/Remove Programs entry
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\KeepTower" \
    "DisplayName"          "KeepTower"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\KeepTower" \
    "DisplayVersion"       "${VERSION}"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\KeepTower" \
    "Publisher"            "tjdeveng"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\KeepTower" \
    "UninstallString"      "$INSTDIR\uninstall.exe"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\KeepTower" \
    "InstallLocation"      "$INSTDIR"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\KeepTower" \
    "DisplayIcon"          "$INSTDIR\keeptower.exe"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\KeepTower" \
    "NoModify"             1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\KeepTower" \
    "NoRepair"             1

  ; Estimate installed size
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\KeepTower" \
    "EstimatedSize" "$0"

  ; Start Menu shortcut
  CreateDirectory "$SMPROGRAMS\KeepTower"
  CreateShortcut  "$SMPROGRAMS\KeepTower\KeepTower.lnk" \
    "$INSTDIR\keeptower-launch.bat" "" "$INSTDIR\keeptower.exe" 0

  ; Desktop shortcut (optional — user can delete)
  CreateShortcut  "$DESKTOP\KeepTower.lnk" \
    "$INSTDIR\keeptower-launch.bat" "" "$INSTDIR\keeptower.exe" 0

  ; Write uninstaller
  WriteUninstaller "$INSTDIR\uninstall.exe"

SectionEnd

; ---------------------------------------------------------------------------
; Uninstaller
; ---------------------------------------------------------------------------
Section "Uninstall"

  ; Remove all installed files
  RMDir /r "$INSTDIR"

  ; Remove Start Menu entries
  Delete "$SMPROGRAMS\KeepTower\KeepTower.lnk"
  RMDir  "$SMPROGRAMS\KeepTower"

  ; Remove Desktop shortcut
  Delete "$DESKTOP\KeepTower.lnk"

  ; Remove registry entries
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\KeepTower"
  DeleteRegKey HKLM "Software\KeepTower"

SectionEnd
