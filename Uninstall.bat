@echo off
setlocal

REM Remove registry entry
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "ParsecVDA - Always Connected" /f

endlocal
