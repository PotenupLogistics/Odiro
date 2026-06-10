@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "SERVER_SCRIPT=%SCRIPT_DIR%Tools\PythonPolicyServer\server.py"

if not exist "%SERVER_SCRIPT%" (
	echo [ProtoRobotSim] Python policy server not found: "%SERVER_SCRIPT%"
	exit /b 1
)

echo [ProtoRobotSim] Starting Python policy server: "%SERVER_SCRIPT%"
echo [ProtoRobotSim] Host: 127.0.0.1
echo [ProtoRobotSim] Port: 8000
echo [ProtoRobotSim] Policy mode: runtime

py -3 "%SERVER_SCRIPT%" --host 127.0.0.1 --port 8000 --policy-mode runtime --verbose-runtime-log %*
exit /b %ERRORLEVEL%
