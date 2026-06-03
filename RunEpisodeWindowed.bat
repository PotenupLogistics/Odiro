@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "PROJECT_FILE=%SCRIPT_DIR%ProtoRobotSim.uproject"
set "RES_X=640"
set "RES_Y=360"
set "DRY_RUN="
set "EXTRA_ARGS="

if /I "%~1"=="--dry-run" (
	set "DRY_RUN=1"
	shift /1
)

:CollectExtraArgs
if "%~1"=="" goto ExtraArgsCollected
set "EXTRA_ARGS=%EXTRA_ARGS% %1"
shift /1
goto CollectExtraArgs
:ExtraArgsCollected

if not exist "%PROJECT_FILE%" (
	echo [ProtoRobotSim] Project file not found: "%PROJECT_FILE%"
	exit /b 1
)

call :ResolveUnrealEditor
if not defined UNREAL_EDITOR (
	echo [ProtoRobotSim] UnrealEditor.exe was not found.
	echo Set UE_ENGINE_DIR, add UnrealEditor.exe to PATH, or check the uproject EngineAssociation.
	exit /b 1
)

set "EXEC_CMDS=viewmode unlit"
set "LOG_CMDS=global none, LogEpisodeSimulation Log, LogEpisodeRunner Log, LogEpisodeEvaluation Log"
set "UE_ARGS="%PROJECT_FILE%" -game -windowed -ResX=%RES_X% -ResY=%RES_Y% -NoSplash -log -FORCELOGFLUSH -ExecCmds="%EXEC_CMDS%" -LogCmds="%LOG_CMDS%"%EXTRA_ARGS%"

echo [ProtoRobotSim] UnrealEditor: "%UNREAL_EDITOR%"
echo [ProtoRobotSim] Project: "%PROJECT_FILE%"
echo [ProtoRobotSim] Window: %RES_X%x%RES_Y%
echo [ProtoRobotSim] Startup exec: %EXEC_CMDS%
echo [ProtoRobotSim] Log filter: %LOG_CMDS%

if defined DRY_RUN (
	echo [ProtoRobotSim] Dry run command:
	echo start "" "%UNREAL_EDITOR%" %UE_ARGS%
	exit /b 0
)

start "" "%UNREAL_EDITOR%" %UE_ARGS%
exit /b 0

:ResolveUnrealEditor
if defined UE_ENGINE_DIR (
	if exist "%UE_ENGINE_DIR%\Binaries\Win64\UnrealEditor.exe" (
		set "UNREAL_EDITOR=%UE_ENGINE_DIR%\Binaries\Win64\UnrealEditor.exe"
		exit /b 0
	)
	if exist "%UE_ENGINE_DIR%\Engine\Binaries\Win64\UnrealEditor.exe" (
		set "UNREAL_EDITOR=%UE_ENGINE_DIR%\Engine\Binaries\Win64\UnrealEditor.exe"
		exit /b 0
	)
)

for %%I in (UnrealEditor.exe) do (
	if not "%%~$PATH:I"=="" (
		set "UNREAL_EDITOR=%%~$PATH:I"
		exit /b 0
	)
)

call :ReadEngineAssociation
if not defined ENGINE_ASSOCIATION exit /b 1

for /f "tokens=2,*" %%A in ('reg query "HKCU\Software\Epic Games\Unreal Engine\Builds" /v "%ENGINE_ASSOCIATION%" 2^>nul') do (
	if /I "%%A"=="REG_SZ" (
		call :ResolveEditorInDirectory "%%~B"
		if defined UNREAL_EDITOR exit /b 0
	)
)

for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\%ENGINE_ASSOCIATION%" /v InstalledDirectory 2^>nul') do (
	if /I "%%A"=="REG_SZ" (
		call :ResolveEditorInDirectory "%%~B"
		if defined UNREAL_EDITOR exit /b 0
	)
)

for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\%ENGINE_ASSOCIATION%" /v InstalledDirectory 2^>nul') do (
	if /I "%%A"=="REG_SZ" (
		call :ResolveEditorInDirectory "%%~B"
		if defined UNREAL_EDITOR exit /b 0
	)
)

exit /b 1

:ReadEngineAssociation
set "ENGINE_ASSOCIATION="
for /f "tokens=2 delims=:" %%A in ('findstr /I /C:"EngineAssociation" "%PROJECT_FILE%"') do (
	set "ENGINE_ASSOCIATION=%%~A"
)
if not defined ENGINE_ASSOCIATION exit /b 1
set "ENGINE_ASSOCIATION=%ENGINE_ASSOCIATION:"=%"
set "ENGINE_ASSOCIATION=%ENGINE_ASSOCIATION:,=%"
for /f "tokens=*" %%A in ("%ENGINE_ASSOCIATION%") do set "ENGINE_ASSOCIATION=%%~A"
exit /b 0

:ResolveEditorInDirectory
set "EDITOR_DIR=%~1"
if not defined EDITOR_DIR exit /b 1
if exist "%EDITOR_DIR%\Binaries\Win64\UnrealEditor.exe" (
	set "UNREAL_EDITOR=%EDITOR_DIR%\Binaries\Win64\UnrealEditor.exe"
	exit /b 0
)
if exist "%EDITOR_DIR%\Engine\Binaries\Win64\UnrealEditor.exe" (
	set "UNREAL_EDITOR=%EDITOR_DIR%\Engine\Binaries\Win64\UnrealEditor.exe"
	exit /b 0
)
exit /b 1
