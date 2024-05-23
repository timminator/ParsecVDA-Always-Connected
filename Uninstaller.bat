@echo off

REM Run command "ParsecVDAAC.exe stop"
ParsecVDAAC.exe stop
REM Run command "ParsecVDAAC.exe uninstall"
ParsecVDAAC.exe uninstall

set /P "=Press any key to quit... " <nul & pause >nul & echo(
exit