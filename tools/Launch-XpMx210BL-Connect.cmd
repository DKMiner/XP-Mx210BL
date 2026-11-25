@echo off
setlocal
set "SCRIPT=%~dp0XpMx210BL-Connect.ps1"

if not exist "%SCRIPT%" (
    echo Could not find XpMx210BL-Connect.ps1 next to this launcher.
    pause
    exit /b 1
)

start "" "%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "%SCRIPT%"
exit /b 0
