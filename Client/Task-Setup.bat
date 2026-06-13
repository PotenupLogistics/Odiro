@rem Entry point for Client setup; runs Client prerequisite checks.
@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "POWERSHELL=powershell.exe"
set "SKIP_PREREQUISITES="
set "ALLOW_MISSING_PREREQUISITES="

:parse_args
if "%~1"=="" goto run_setup
if /I "%~1"=="-SkipPrerequisites" (
	set "SKIP_PREREQUISITES=1"
	shift
	goto parse_args
)
if /I "%~1"=="-AllowMissingPrerequisites" (
	set "ALLOW_MISSING_PREREQUISITES=1"
	shift
	goto parse_args
)

echo [setup/client] Unknown option: %~1
exit /b 1

:run_setup
echo [setup/client] Started.

if not defined SKIP_PREREQUISITES (
	if defined ALLOW_MISSING_PREREQUISITES (
		"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%Tools\CheckPrerequisites.ps1" -AllowMissing
	) else (
		"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%Tools\CheckPrerequisites.ps1"
	)
	if errorlevel 1 exit /b 1
)

echo [setup/client] Complete.
exit /b 0
