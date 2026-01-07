@echo off
setlocal

set "TARGET=%~dp0build\Pressure-Win64.exe"

if exist "%TARGET%" (
    start "" "%TARGET%"
) else (
    echo Binary not found, building
    call Build.cmd
    if %ERRORLEVEL% EQU 0 (
        echo Build successful
        echo Launching
        start "" "%TARGET%"
    ) else (
        echo Launch failed
        pause
        exit /b %ERRORLEVEL%
    )
)