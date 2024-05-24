@echo off
setlocal

IF "%1"=="install" (
    rem Run install and start commands
    ParsecVDAAC.exe install
    ParsecVDAAC.exe start
)

IF "%1"=="uninstall" (
    rem Run stop and uninstall commands, delete created files
    ParsecVDAAC.exe stop
    ParsecVDAAC.exe uninstall
    del "%~dp0Logfile ParsecVDA - Always Connected.txt"
    del "%~dp0ParsecVDAAC.err.log"
    del "%~dp0ParsecVDAAC.out.log"
    del "%~dp0ParsecVDAAC.wrapper.log"
)

IF "%1"=="" (
    echo Missing argument! Valid arguments are "install" or "uninstall".
)

endlocal