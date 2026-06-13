@rem Launches the Unreal project in game preview mode without packaging.
@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "POWERSHELL=powershell.exe"

"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%Tools\RunPreview.ps1" %*
exit /b %ERRORLEVEL%
