@rem Entry point for the development session; runs Agents reload server and Unreal Editor.
@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "POWERSHELL=powershell.exe"

"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%tools\dev-session.ps1" %*
set "TASK_EXIT_CODE=%ERRORLEVEL%"
call "%SCRIPT_DIR%tools\pause-task-on-exit.bat" "%~nx0"
exit /b %TASK_EXIT_CODE%
