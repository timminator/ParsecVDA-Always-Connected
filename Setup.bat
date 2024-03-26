@echo off
setlocal

REM Get the current directory
for %%A in ("%~dp0.") do set "currentDirectory=%%~fA"

REM Create the .vbs file
(
  echo Dim WShell
  echo Set WShell = CreateObject^("WScript.Shell"^)
  echo WShell.Run """%currentDirectory%\ParsecVDA - Always Connected.exe""", 0
  echo Set WShell = Nothing
) > "%currentDirectory%\ParsecVDA - Always Connected.vbs"

REM Add registry entry
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "ParsecVDA - Always Connected" /t REG_SZ /d "\"%currentDirectory%\ParsecVDA - Always Connected.vbs\"" /f

endlocal