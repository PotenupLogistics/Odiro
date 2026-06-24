@rem Pauses task wrappers only when they were opened as transient windows.
@echo off
setlocal EnableExtensions EnableDelayedExpansion

if defined ODIRO_TASK_DISABLE_PAUSE exit /b 0
if "%~1"=="" exit /b 0

echo(!CMDCMDLINE!| findstr /I /C:"/c" >nul
if errorlevel 1 exit /b 0

echo(!CMDCMDLINE!| findstr /I /C:"%~1" >nul
if errorlevel 1 exit /b 0

set "TASK_PAUSE_ON_EXIT="
for /f "delims=" %%I in ('powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0test-task-pause-on-exit.ps1" 2^>nul') do set "TASK_PAUSE_ON_EXIT=%%I"
if "!TASK_PAUSE_ON_EXIT!"=="1" pause

exit /b 0
