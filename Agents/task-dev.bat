@rem Entry point for running the Agents API server with development reload.
@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "POWERSHELL=powershell.exe"

"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%tools\dev.ps1" %*
exit /b %ERRORLEVEL%
