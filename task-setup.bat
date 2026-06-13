@rem Entry point for repository setup; runs prerequisite and install phases.
@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "POWERSHELL=powershell.exe"
set "SKIP_PREREQUISITES="
set "ALLOW_MISSING_PREREQUISITES="
set "SKIP_GIT_HOOKS="
set "SKIP_AGENTS="

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
if /I "%~1"=="-SkipGitHooks" (
	set "SKIP_GIT_HOOKS=1"
	shift
	goto parse_args
)
if /I "%~1"=="-SkipAgents" (
	set "SKIP_AGENTS=1"
	shift
	goto parse_args
)

echo [setup] Unknown option: %~1
exit /b 1

:run_setup
echo [setup] Started.

if not defined SKIP_PREREQUISITES (
	if defined ALLOW_MISSING_PREREQUISITES (
		"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%tools\check-prerequisites.ps1" -AllowMissing
	) else (
		"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%tools\check-prerequisites.ps1"
	)
	if errorlevel 1 exit /b 1
)

set "INSTALL_ARGS="
if defined SKIP_GIT_HOOKS set "INSTALL_ARGS=%INSTALL_ARGS% -SkipGitHooks"
if defined SKIP_AGENTS set "INSTALL_ARGS=%INSTALL_ARGS% -SkipAgents"

"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%tools\install.ps1" %INSTALL_ARGS%
if errorlevel 1 exit /b 1

echo [setup] Complete.
exit /b 0
