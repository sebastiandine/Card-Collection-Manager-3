Unicode True
!include "MUI2.nsh"

!define APP_NAME "Card Collection Manager 3"
!define APP_DIR "Card Collection Manager 3"
!define APP_EXE "ccm3.exe"
!define APP_ICON "installer_icon.ico"
!define REG_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Card Collection Manager 3"
!define APP_PATHS_KEY "Software\Microsoft\Windows\CurrentVersion\App Paths\ccm3.exe"

; APP_VERSION is supplied by CI via `makensis -DAPP_VERSION=...`. Falls back to
; "localbuild" so manual runs from a developer machine still work.
!ifndef APP_VERSION
  !define APP_VERSION "localbuild"
!endif

Name "${APP_NAME} ${APP_VERSION}"
OutFile "..\ccm3-windows-installer.exe"
Icon "${APP_ICON}"
UninstallIcon "${APP_ICON}"
BrandingText "${APP_NAME} ${APP_VERSION}"
InstallDir "$PROGRAMFILES64\${APP_DIR}"
RequestExecutionLevel admin

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Section "!Core files (required)"
  SectionIn RO
  SetOutPath "$INSTDIR"
  File /r "..\build\bin\*.*"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "${REG_KEY}" "DisplayName" "${APP_NAME}"
  WriteRegStr HKLM "${REG_KEY}" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKLM "${REG_KEY}" "DisplayIcon" "$INSTDIR\${APP_EXE}"
  WriteRegStr HKLM "${REG_KEY}" "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
  WriteRegStr HKLM "${REG_KEY}" "QuietUninstallString" "$\"$INSTDIR\Uninstall.exe$\" /S"
  WriteRegStr HKLM "${REG_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegDWORD HKLM "${REG_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${REG_KEY}" "NoRepair" 1
  WriteRegStr HKLM "${APP_PATHS_KEY}" "" "$INSTDIR\${APP_EXE}"
  WriteRegStr HKLM "${APP_PATHS_KEY}" "Path" "$INSTDIR"
SectionEnd

Section "Start Menu shortcuts"
  SetShellVarContext all
  CreateDirectory "$SMPROGRAMS\${APP_DIR}"
  CreateShortcut "$SMPROGRAMS\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}" 0
  CreateShortcut "$SMPROGRAMS\${APP_DIR}\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}" 0
  CreateShortcut "$SMPROGRAMS\${APP_DIR}\Uninstall ${APP_NAME}.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd

Section "Desktop shortcut"
  SetShellVarContext all
  CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}" 0
SectionEnd

Section "Uninstall"
  SetShellVarContext all
  Delete "$DESKTOP\${APP_NAME}.lnk"
  Delete "$SMPROGRAMS\${APP_NAME}.lnk"
  Delete "$SMPROGRAMS\${APP_DIR}\${APP_NAME}.lnk"
  Delete "$SMPROGRAMS\${APP_DIR}\Uninstall ${APP_NAME}.lnk"
  RMDir "$SMPROGRAMS\${APP_DIR}"

  RMDir /r "$INSTDIR"
  DeleteRegKey HKLM "${REG_KEY}"
  DeleteRegKey HKLM "${APP_PATHS_KEY}"
SectionEnd
