@rem Bridge setup entrypoint
@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "POWERSHELL=powershell.exe"
set "ALLOW_MISSING_PREREQUISITES="

:parse_args
if "%~1"=="" goto run_setup
if /I "%~1"=="-AllowMissingPrerequisites" (
	set "ALLOW_MISSING_PREREQUISITES=1"
	shift
	goto parse_args
)

echo [setup/bridge] Unknown option: %~1
exit /b 1

:run_setup
echo [setup/bridge] Started.

if defined ALLOW_MISSING_PREREQUISITES (
	"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%tools\check-prerequisites.ps1" -AllowMissing
) else (
	"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%tools\check-prerequisites.ps1"
)
if errorlevel 1 exit /b 1

echo [setup/bridge] Complete.
exit /b 0
