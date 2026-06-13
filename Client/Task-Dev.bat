@rem Entry point for opening the Unreal Editor for Client development.
@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "POWERSHELL=powershell.exe"

"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%Tools\Dev.ps1" %*
exit /b %ERRORLEVEL%
