@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "SERVER_SCRIPT=%SCRIPT_DIR%Tools\PythonAgent\server.py"

if not exist "%SERVER_SCRIPT%" (
	echo [ProtoRobotSim] PythonAgent server not found: "%SERVER_SCRIPT%"
	exit /b 1
)

echo [ProtoRobotSim] Starting PythonAgent server: "%SERVER_SCRIPT%"
echo [ProtoRobotSim] Host: 127.0.0.1
echo [ProtoRobotSim] Port: 8000

python "%SERVER_SCRIPT%" %*
exit /b %ERRORLEVEL%
