@echo OFF
setlocal

reg Query "HKLM\Hardware\Description\System\CentralProcessor\0" | find /i "AMD64" > NUL && set OSVersion=64BIT || reg Query "HKLM\Hardware\Description\System\CentralProcessor\0" | find /i "Intel64" > NUL && set OSVersion=64BIT || set OSVersion=32BIT


if %OSVersion%==64BIT (
    REM download file from "https://github.com/winsw/winsw/releases/download/v2.12.0/WinSW-x64.exe" to current directory
    echo Downloading WinSW-x64.exe
    curl -o "%cd%\ParsecVDAAC.exe" -L "https://github.com/winsw/winsw/releases/download/v2.12.0/WinSW-x64.exe"
)

if %OSVersion%==32BIT (
    REM download file from "https://github.com/winsw/winsw/releases/download/v2.12.0/WinSW-x86.exe" to current directory
    echo Downloading WinSW-x86.exe
    curl -o "%cd%\ParsecVDAAC.exe" -L "https://github.com/winsw/winsw/releases/download/v2.12.0/WinSW-x86.exe"
)

REM Run command "ParsecVDAAC.exe install"
ParsecVDAAC.exe install
REM Run command "ParsecVDAAC.exe start"
ParsecVDAAC.exe start

endlocal

set /P "=Press any key to quit... " <nul & pause >nul & echo(
exit
