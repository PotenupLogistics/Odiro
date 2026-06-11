@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%"
set "PROTO_AI_DIR=%SCRIPT_DIR%Proto-AI"

if not exist "%PROTO_AI_DIR%\app\main.py" (
	echo [ProtoRobotSim] Proto-AI FastAPI app was not found: "%PROTO_AI_DIR%\app\main.py"
	exit /b 1
)

where uv >nul 2>nul
if errorlevel 1 (
	echo [ProtoRobotSim] uv was not found on PATH.
	exit /b 1
)

if not defined OPENAI_API_KEY (
	echo [ProtoRobotSim] OPENAI_API_KEY is not set.
	set /p "OPENAI_API_KEY=Enter OPENAI_API_KEY: "
)

if not defined OPENAI_API_KEY (
	echo [ProtoRobotSim] OPENAI_API_KEY is required.
	exit /b 1
)

set "SCENARIO_ARTIFACT_OUTPUT_DIR=%PROJECT_ROOT%"
set "SCENARIO_ARTIFACT_WRITE_ENABLED=true"
set "GOOGLE_DRIVE_UPLOAD_ENABLED=false"

echo [ProtoRobotSim] Starting Proto-AI LLM authoring server.
echo [ProtoRobotSim] Proto-AI dir: "%PROTO_AI_DIR%"
echo [ProtoRobotSim] Artifact output dir: "%SCENARIO_ARTIFACT_OUTPUT_DIR%"
echo [ProtoRobotSim] Host: 127.0.0.1
echo [ProtoRobotSim] Port: 8711

pushd "%PROTO_AI_DIR%"
uv run uvicorn app.main:app --host 127.0.0.1 --port 8711 --reload %*
set "EXIT_CODE=%ERRORLEVEL%"
popd

exit /b %EXIT_CODE%
