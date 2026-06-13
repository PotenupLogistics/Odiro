@rem Entry point for the development session; runs Agents reload server and Unreal Editor.
@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "POWERSHELL=powershell.exe"

"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%tools\dev-session.ps1" %*
exit /b %ERRORLEVEL%
