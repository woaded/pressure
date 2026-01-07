@echo off
setlocal

set MSYS_PATH=C:\msys64\msys2_shell.cmd

if not exist "%MSYS_PATH%" (
    echo MSYS2 not found
    echo Please install MSYS2 to C:\msys64\
    pause
    exit /b 1
)

echo Starting MinGW64 build
call "%MSYS_PATH%" -mingw64 -where "%CD%/src" -defterm -no-start -shell bash -c "make clean && make"

if %ERRORLEVEL% EQU 0 (
    echo Build successful
) else (
    echo Build failed
    exit /b 1
)