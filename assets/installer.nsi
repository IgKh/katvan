!include "MUI.nsh"
!include "WinVer.nsh"

!ifndef OUTFILE
    !define OUTFILE "installer.exe"
!endif

Name "Katvan"
InstallDir "$PROGRAMFILES64\Katvan"
InstallDirRegKey HKLM "Software\Katvan" ""
OutFile "${OUTFILE}"
ShowInstDetails show

SetCompressor /SOLID lzma

!getdllversion "katvan.exe" katvanver_

!define VERSION "${katvanver_1}.${katvanver_2}.${katvanver_3}"
!define REG_UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Katvan"

Var StartMenuFolder

Function .onInit
    ${IfNot} ${AtLeastWin10}
        MessageBox MB_OK "Windows 10 or above is required to install Katvan"
        Quit
    ${EndIf}

    SetShellVarContext all
FunctionEnd

Function un.onInit
    SetShellVarContext all
FunctionEnd

!define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKCU"
!define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\Katvan\Katvan"
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "LICENSE.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_STARTMENU Application $StartMenuFolder
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

VIProductVersion "${katvanver_1}.${katvanver_2}.${katvanver_3}.${katvanver_4}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductName" "Katvan"
VIAddVersionKey /LANG=${LANG_ENGLISH} "LegalCopyright" "Copyright (c) 2024 - 2026 Igor Khanin"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileDescription" "Katvan ${VERSION} Installer"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductVersion" "${VERSION}"

Section
    SetOutPath $INSTDIR

    File "katvan.exe"
    File "typstdriver.dll"
    File "Qt6Core.dll"
    File "Qt6Gui.dll"
    File "Qt6Widgets.dll"
    File "Qt6Network.dll"
    File "Qt6Svg.dll"
    File "archive.dll"
    File "zlib1.dll"
    File "LICENSE.txt"
    File /r "iconengines"
    File /r "platforms"
    File /r "styles"
    File /r "tls"
    File /r "translations"

    ; Ensure VCRT is installed
    File "vc_redist.x64.exe"
    ExecWait '"$INSTDIR\vc_redist.x64.exe" /install /passive'
    Delete "$INSTDIR\vc_redist.x64.exe"

    ; Start menu entry
    !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
    CreateDirectory "$SMPrograms\$StartMenuFolder"
    CreateShortCut "$SMPrograms\$StartMenuFolder\Katvan.lnk" "$INSTDIR\katvan.exe"
    !insertmacro MUI_STARTMENU_WRITE_END

    ; Shell association
    WriteRegStr HKLM "Software\Classes\.typ" "Content Type" "text/x-typst"
    WriteRegStr HKLM "Software\Classes\.typ" "PerceivedType" "text"
    WriteRegNone HKLM "Software\Classes\.typ\OpenWithProgids" "Katvan.Document.0"

    WriteRegStr HKLM "Software\Classes\Katvan.Document.0" "" "Typst document"
    WriteRegExpandStr HKLM "Software\Classes\Katvan.Document.0\shell\open\command" "" '"$INSTDIR\katvan.exe" "%1"'

    ; Uninstaller
    WriteUninstaller "uninstall.exe"

    WriteRegStr HKLM "${REG_UNINSTALL_KEY}" "DisplayName" "Katvan"
    WriteRegStr HKLM "${REG_UNINSTALL_KEY}" "DisplayVersion" "${VERSION}"
    WriteRegStr HKLM "${REG_UNINSTALL_KEY}" "Publisher" "Igor Khanin"
    WriteRegStr HKLM "${REG_UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\katvan.exe"
    WriteRegStr HKLM "${REG_UNINSTALL_KEY}" "UninstallString" "$INSTDIR\uninstall.exe"
    WriteRegStr HKLM "${REG_UNINSTALL_KEY}" "QuietUninstallString" '"$INSTDIR\uninstall.exe" /S'
    WriteRegDWORD HKLM "${REG_UNINSTALL_KEY}" "NoRepair" 1
    WriteRegDWORD HKLM "${REG_UNINSTALL_KEY}" "NoModify" 1

    WriteRegStr HKLM "Software\Katvan" "" "$INSTDIR"
SectionEnd

Section "Uninstall"
    Delete "$INSTDIR\katvan.exe"
    Delete "$INSTDIR\typstdriver.dll"
    Delete "$INSTDIR\Qt6Core.dll"
    Delete "$INSTDIR\Qt6Gui.dll"
    Delete "$INSTDIR\Qt6Widgets.dll"
    Delete "$INSTDIR\Qt6Network.dll"
    Delete "$INSTDIR\Qt6Svg.dll"
    Delete "$INSTDIR\archive.dll"
    Delete "$INSTDIR\zlib1.dll"
    Delete "$INSTDIR\LICENSE.txt"
    RMDir /r "$INSTDIR\iconengines"
    RMDir /r "$INSTDIR\platforms"
    RMDir /r "$INSTDIR\styles"
    RMDir /r "$INSTDIR\tls"
    RMDir /r "$INSTDIR\translations"

    Delete "$INSTDIR\uninstall.exe"

    ; Only if empty:
    RMDir "$INSTDIR\hunspell"
    RMDir "$INSTDIR"

    ; Remove Start menu entry
    !insertmacro MUI_STARTMENU_GETFOLDER Application $StartMenuFolder
    Delete "$SMPROGRAMS\$StartMenuFolder\Katvan.lnk"
    RMDir "$SMPROGRAMS\$StartMenuFolder"

    ; Remove shell association
    DeleteRegKey HKLM "Software\Classes\.typ\OpenWithProgids\Katvan.Document.0"
    DeleteRegKey HKLM "Software\Classes\Katvan.Document.0"

    DeleteRegKey HKLM "${REG_UNINSTALL_KEY}"
    DeleteRegKey HKLM "Software\Katvan"
SectionEnd
