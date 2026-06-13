@rem Entry point for product-like local run; runs Agents API and Client preview.
@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "POWERSHELL=powershell.exe"

"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%tools\run-preview.ps1" %*
exit /b %ERRORLEVEL%
