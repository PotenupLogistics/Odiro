@rem Entry point for product-like local run; runs Agents API, Bridge service, and Client preview.
@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "POWERSHELL=powershell.exe"

"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%tools\run-preview.ps1" %*
set "TASK_EXIT_CODE=%ERRORLEVEL%"
call "%SCRIPT_DIR%tools\pause-task-on-exit.bat" "%~nx0"
exit /b %TASK_EXIT_CODE%
