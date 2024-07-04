@echo off
setlocal

rem Check if ParsecVDAAC service is installed
sc query ParsecVDAAC >nul 2>&1
set SERVICE_EXIST=%ERRORLEVEL%
if "%SERVICE_EXIST%"=="0" (
    rem ParsecVDAAC service is already installed, stopping and uninstalling it...
    ParsecVDAAC.exe stop
    ParsecVDAAC.exe uninstall
) else (
    rem ParsecVDAAC service is not installed, nothing to be done here...
)

endlocal