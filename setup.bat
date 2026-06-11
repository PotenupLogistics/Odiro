@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "POWERSHELL=powershell.exe"

"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -Command "Write-Host 'Setup repository...' -ForegroundColor Cyan"

"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%tools\setup-git-hooks.ps1"
if errorlevel 1 (
    "%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -Command "Write-Host 'Setup Failed.' -ForegroundColor Red"
    exit /b 1
)

"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -Command "Write-Host 'Setup Complete.' -ForegroundColor Green"
exit /b 0
