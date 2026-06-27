@rem Entry point for repository setup; runs prerequisite and install phases.
@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "POWERSHELL=powershell.exe"
set "SKIP_PREREQUISITES="
set "SKIP_GIT_HOOKS="
set "SKIP_AGENTS="

:parse_args
if "%~1"=="" goto run_setup
if /I "%~1"=="-SkipPrerequisites" (
	set "SKIP_PREREQUISITES=1"
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
set "TASK_EXIT_CODE=1"
goto exit_task

:run_setup
echo [setup] Started.

if not defined SKIP_PREREQUISITES (
	"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%tools\check-prerequisites.ps1"
	if errorlevel 1 (
		set "TASK_EXIT_CODE=1"
		goto exit_task
	)
)

set "INSTALL_ARGS="
if defined SKIP_GIT_HOOKS set "INSTALL_ARGS=%INSTALL_ARGS% -SkipGitHooks"
if defined SKIP_AGENTS set "INSTALL_ARGS=%INSTALL_ARGS% -SkipAgents"

"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%tools\install.ps1" %INSTALL_ARGS%
if errorlevel 1 (
	set "TASK_EXIT_CODE=1"
	goto exit_task
)

echo [setup] Complete.
set "TASK_EXIT_CODE=0"
goto exit_task

:exit_task
call "%SCRIPT_DIR%tools\pause-task-on-exit.bat" "%~nx0"
exit /b %TASK_EXIT_CODE%
