@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "PROJECT_FILE=%SCRIPT_DIR%ProtoRobotSim.uproject"
set "UNREAL_EDITOR="
set "PREVIEW_WINDOW_ARGS=-windowed -ResX=960 -ResY=540"

if not exist "%PROJECT_FILE%" (
	echo [ProtoRobotSim] Project file not found: "%PROJECT_FILE%"
	exit /b 1
)

call :ResolveUnrealEditor
if not defined UNREAL_EDITOR (
	echo [ProtoRobotSim] UnrealEditor.exe was not found.
	echo [ProtoRobotSim] Set UE_EDITOR_EXE, set UE_ENGINE_DIR, add UnrealEditor.exe to PATH, or check EngineAssociation.
	exit /b 1
)

echo [ProtoRobotSim] UnrealEditor: "%UNREAL_EDITOR%"
echo [ProtoRobotSim] Project: "%PROJECT_FILE%"
echo [ProtoRobotSim] Preview window args: %PREVIEW_WINDOW_ARGS%
echo [ProtoRobotSim] Packaged-style args: %*

start "" /wait "%UNREAL_EDITOR%" "%PROJECT_FILE%" -game -NoSplash %PREVIEW_WINDOW_ARGS% %*
exit /b %ERRORLEVEL%

:ResolveUnrealEditor
if defined UE_EDITOR_EXE (
	if exist "%UE_EDITOR_EXE%" (
		set "UNREAL_EDITOR=%UE_EDITOR_EXE%"
		exit /b 0
	)
)

if defined UE_ENGINE_DIR (
	call :ResolveEditorInDirectory "%UE_ENGINE_DIR%"
	if defined UNREAL_EDITOR exit /b 0
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

call :ResolveEditorInDirectory "%ProgramFiles%\Epic Games\UE_%ENGINE_ASSOCIATION%"
exit /b 0

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
