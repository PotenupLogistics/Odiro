@rem Starts the client Python policy server for runtime policy evaluation.
@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%.") do set "CLIENT_ROOT=%%~fI"
set "SERVER_SCRIPT=%CLIENT_ROOT%\Resources\policy-runtime.py"
set "PYTHON_AGENT_HOST=127.0.0.1"
set "PYTHON_AGENT_PORT=8000"

call :ReadServerArgs %*

if not exist "%SERVER_SCRIPT%" (
	echo [run/policy] Python policy server not found: "%SERVER_SCRIPT%"
	exit /b 1
)

echo [run/policy] Starting Python policy runtime: "%SERVER_SCRIPT%"
echo [run/policy] Host: %PYTHON_AGENT_HOST%
echo [run/policy] Port: %PYTHON_AGENT_PORT%
echo [run/policy] Policy mode: runtime

py -3 "%SERVER_SCRIPT%" --host "%PYTHON_AGENT_HOST%" --port "%PYTHON_AGENT_PORT%" --policy-mode runtime --verbose-runtime-log %*
exit /b %ERRORLEVEL%

:ReadServerArgs
if "%~1"=="" exit /b 0
if /I "%~1"=="--host" (
	if not "%~2"=="" set "PYTHON_AGENT_HOST=%~2"
	shift
	shift
	goto :ReadServerArgs
)
if /I "%~1"=="--port" (
	if not "%~2"=="" set "PYTHON_AGENT_PORT=%~2"
	shift
	shift
	goto :ReadServerArgs
)
shift
goto :ReadServerArgs
