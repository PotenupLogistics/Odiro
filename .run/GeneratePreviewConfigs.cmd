@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%GeneratePreviewConfigs.ps1"
exit /b %ERRORLEVEL%
