@rem Entry point for Agents setup; runs Agents prerequisite and install phases.
@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "POWERSHELL=powershell.exe"
set "SKIP_PREREQUISITES="
set "ALLOW_MISSING_PREREQUISITES="
set "SKIP_DEPENDENCIES="

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
if /I "%~1"=="-SkipDependencies" (
	set "SKIP_DEPENDENCIES=1"
	shift
	goto parse_args
)

echo [setup/agents] Unknown option: %~1
exit /b 1

:run_setup
echo [setup/agents] Started.

if not defined SKIP_PREREQUISITES (
	if defined ALLOW_MISSING_PREREQUISITES (
		"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%tools\check-prerequisites.ps1" -AllowMissing
	) else (
		"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%tools\check-prerequisites.ps1"
	)
	if errorlevel 1 exit /b 1
)

if not defined SKIP_DEPENDENCIES (
	"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%tools\install.ps1"
	if errorlevel 1 exit /b 1
)

echo [setup/agents] Complete.
exit /b 0
