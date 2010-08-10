Name "SafeSex"

; The file to write
OutFile "safesex-setup.exe"

LicenseText "You must agree to the following license before installing SafeSex. I know, it's a lot to ask."
LicenseData license.txt
InstallDir $PROGRAMFILES\SafeSex
InstallDirRegKey HKLM SOFTWARE\SafeSex ""
ShowInstDetails show
ShowUnInstDetails show
AutoCloseWindow true

ComponentText "This will install SafeSex on your computer."
DirText "Choose a directory to install in to:"

; The stuff to install
Section "SafeSex (required)"
  SetOutPath $INSTDIR

again:
  Delete $INSTDIR\safesex.exe
  IfFileExists $INSTDIR\Safesex.exe "" diddelete
    MessageBox MB_OKCANCEL "Error removing SafeSex. $\n$\nPlease close all instances of SafeSex, then hit OK to try again, or cancel to abort the install." IDOK again
   Abort "Install aborted."
diddelete:

  File "release\safesex.exe"
  File "readme.txt"
  WriteRegStr HKLM SOFTWARE\SafeSex "" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SafeSex" "DisplayName" "SafeSex (remove only)"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SafeSex" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteUninstaller "uninstall.exe"
SectionEnd

Section "SafeSex Source Code"
  SetOutPath $INSTDIR\Source
  File license.txt
  File *.ds?
  File *.cpp
  File *.c
  File *.h
  File *.rc
  File *.ico
  File *.nsi
  File readme.txt
  SetOutPath $INSTDIR
SectionEnd

Section "Start Menu Shortcuts"
  CreateDirectory "$SMPROGRAMS\SafeSex"
  CreateShortCut "$SMPROGRAMS\SafeSex\Uninstall SafeSex.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\SafeSex\Run SafeSex.lnk" "$INSTDIR\safesex.exe" "" "$INSTDIR\safesex.exe" 0
  CreateShortCut "$SMPROGRAMS\SafeSex\SafeSex Readme.lnk" "$INSTDIR\readme.txt"
  IFFileExists $INSTDIR\Source\*.* "" nosource
    CreateShortCut "$SMPROGRAMS\SafeSex\SafeSex Project Workspace.lnk" "$INSTDIR\Source\SafeSex.dsw"
  
nosource:
SectionEnd

Section "View SafeSex Readme"
  ExecShell "open" "$INSTDIR\readme.txt"
SectionEnd

Section "Run SafeSex"
  Exec '"$INSTDIR\Safesex.exe"'
SectionEnd

UninstallText "This will uninstall SafeSex. Hit next to continue."

; special uninstall section.
Section "Uninstall"
again:
  Delete $INSTDIR\safesex.exe
  IfFileExists $INSTDIR\Safesex.exe "" diddelete
    MessageBox MB_OKCANCEL "Error removing SafeSex. $\n$\nPlease close all instances of SafeSex, then hit OK to try again, or cancel to abort the uninstall." IDOK again
   Abort "Uninstall aborted."
diddelete:
  IfFileExists "$INSTDIR\*.sex" "" noprofiles
  MessageBox MB_YESNO "You have SafeSex profiles in the install directory. Remove them?$\n$\n(If you answer yes, you will lose all of your SafeSex notes)" IDNO noprofiles
  Delete $INSTDIR\*.sex
  Delete $INSTDIR\*.ini
noprofiles:

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SafeSex"
  DeleteRegKey HKLM SOFTWARE\SafeSex

  Delete $INSTDIR\license.txt
  Delete $INSTDIR\readme.txt
  Delete $INSTDIR\uninstall.exe
  RMDir /r "$INSTDIR\Source"

  Delete "$SMPROGRAMS\SafeSex\*.*"
  RMDir "$SMPROGRAMS\SafeSex"
  RMDir "$INSTDIR"
SectionEnd

; eof
