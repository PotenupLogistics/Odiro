@rem Entry point for pushing the current branch and opening a pull request.
@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "POWERSHELL=powershell.exe"

"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%tools\open-pull-request.ps1" %*
exit /b %ERRORLEVEL%
