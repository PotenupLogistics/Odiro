@rem Entry point for repository builds; runs project build tasks.
@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "POWERSHELL=powershell.exe"

"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%tools\build.ps1" %*
exit /b %ERRORLEVEL%
