@rem Entry point for Client builds; runs the Unreal editor target build.
@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "POWERSHELL=powershell.exe"

"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%Tools\Build.ps1" %*
exit /b %ERRORLEVEL%
