@rem Entry point for repository builds; runs project build tasks.
@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "POWERSHELL=powershell.exe"

"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%tools\build.ps1" %*
set "TASK_EXIT_CODE=%ERRORLEVEL%"
call "%SCRIPT_DIR%tools\pause-task-on-exit.bat" "%~nx0"
exit /b %TASK_EXIT_CODE%
