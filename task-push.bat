@rem Entry point for pushing the current branch and opening a pull request.
@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "POWERSHELL=powershell.exe"

"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%tools\push-pr.ps1" %*
set "TASK_EXIT_CODE=%ERRORLEVEL%"
call "%SCRIPT_DIR%tools\pause-task-on-exit.bat" "%~nx0"
exit /b %TASK_EXIT_CODE%
