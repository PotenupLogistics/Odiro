@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
set "PROJECT_FILE=%SCRIPT_DIR%ProtoRobotSim.uproject"
set "BUILD_BATCH="
set "ENGINE_ROOT="
set "ENGINE_ASSOCIATION="
set "TARGET_KIND=Editor"
set "TARGET_NAME=ProtoRobotSimEditor"
set "PLATFORM=Win64"
set "CONFIGURATION=Development"
set "WAIT_MUTEX_ARG=-WaitMutex"
set "DRY_RUN="
set "HELP_REQUESTED="
set "EXTRA_ARGS="

if not exist "%PROJECT_FILE%" (
	echo [ProtoRobotSim] Project file not found: "%PROJECT_FILE%"
	exit /b 1
)

call :ParseArgs %*
if errorlevel 1 exit /b %ERRORLEVEL%
if defined HELP_REQUESTED exit /b 0

if /I "%TARGET_KIND%"=="Editor" (
	set "TARGET_NAME=ProtoRobotSimEditor"
) else if /I "%TARGET_KIND%"=="Game" (
	set "TARGET_NAME=ProtoRobotSim"
) else (
	echo [ProtoRobotSim] Invalid target: "%TARGET_KIND%"
	echo [ProtoRobotSim] Valid targets: Editor, Game
	exit /b 1
)

call :ResolveBuildBatch
if not defined BUILD_BATCH (
	echo [ProtoRobotSim] Unreal Build.bat was not found.
	echo [ProtoRobotSim] Set UE_ENGINE_DIR, set UE_EDITOR_EXE, add UnrealEditor.exe to PATH, or check EngineAssociation.
	exit /b 1
)

echo [ProtoRobotSim] Unreal Engine path: "%ENGINE_ROOT%"
echo [ProtoRobotSim] Project: "%PROJECT_FILE%"
echo [ProtoRobotSim] Target: %TARGET_NAME% %PLATFORM% %CONFIGURATION%
echo [ProtoRobotSim] Build command:
echo   "%BUILD_BATCH%" %TARGET_NAME% %PLATFORM% %CONFIGURATION% "-Project=%PROJECT_FILE%" -NoHotReloadFromIDE %WAIT_MUTEX_ARG% %EXTRA_ARGS%

if defined DRY_RUN (
	echo [ProtoRobotSim] Dry run only. Build was not started.
	exit /b 0
)

call "%BUILD_BATCH%" "%TARGET_NAME%" "%PLATFORM%" "%CONFIGURATION%" "-Project=%PROJECT_FILE%" -NoHotReloadFromIDE %WAIT_MUTEX_ARG% %EXTRA_ARGS%
exit /b %ERRORLEVEL%

:ParseArgs
if "%~1"=="" exit /b 0
set "ARG=%~1"

if /I "!ARG!"=="-Help" (
	call :PrintUsage
	set "HELP_REQUESTED=1"
	exit /b 0
)

if /I "!ARG!"=="/?" (
	call :PrintUsage
	set "HELP_REQUESTED=1"
	exit /b 0
)

if /I "!ARG!"=="-Target" (
	if "%~2"=="" (
		echo [ProtoRobotSim] Missing value for -Target.
		exit /b 1
	)
	set "TARGET_KIND=%~2"
	shift
	shift
	goto ParseArgs
)

if /I "!ARG:~0,8!"=="-Target=" (
	set "TARGET_KIND=!ARG:~8!"
	shift
	goto ParseArgs
)

if /I "!ARG!"=="-Configuration" (
	if "%~2"=="" (
		echo [ProtoRobotSim] Missing value for -Configuration.
		exit /b 1
	)
	set "CONFIGURATION=%~2"
	shift
	shift
	goto ParseArgs
)

if /I "!ARG:~0,15!"=="-Configuration=" (
	set "CONFIGURATION=!ARG:~15!"
	shift
	goto ParseArgs
)

if /I "!ARG!"=="-Platform" (
	if "%~2"=="" (
		echo [ProtoRobotSim] Missing value for -Platform.
		exit /b 1
	)
	set "PLATFORM=%~2"
	shift
	shift
	goto ParseArgs
)

if /I "!ARG:~0,10!"=="-Platform=" (
	set "PLATFORM=!ARG:~10!"
	shift
	goto ParseArgs
)

if /I "!ARG!"=="-NoWaitMutex" (
	set "WAIT_MUTEX_ARG="
	shift
	goto ParseArgs
)

if /I "!ARG!"=="-DryRun" (
	set "DRY_RUN=1"
	shift
	goto ParseArgs
)

if /I "!ARG!"=="--" (
	shift
	goto AppendRemainingArgs
)

set "EXTRA_ARGS=!EXTRA_ARGS! %1"
shift
goto ParseArgs

:AppendRemainingArgs
if "%~1"=="" exit /b 0
set "EXTRA_ARGS=!EXTRA_ARGS! %1"
shift
goto AppendRemainingArgs

:PrintUsage
echo Usage: BuildProject.bat [-Target Editor^|Game] [-Configuration Development^|DebugGame^|Shipping] [-Platform Win64] [-NoWaitMutex] [-DryRun] [-- extra UBT args]
exit /b 0

:ResolveBuildBatch
if defined UE_ENGINE_DIR (
	call :ResolveBuildInDirectory "%UE_ENGINE_DIR%"
	if defined BUILD_BATCH exit /b 0
)

if defined UE_EDITOR_EXE (
	call :ResolveBuildFromEditor "%UE_EDITOR_EXE%"
	if defined BUILD_BATCH exit /b 0
)

for %%I in (UnrealEditor.exe) do (
	if not "%%~$PATH:I"=="" (
		call :ResolveBuildFromEditor "%%~$PATH:I"
		if defined BUILD_BATCH exit /b 0
	)
)

call :ReadEngineAssociation
if not defined ENGINE_ASSOCIATION exit /b 1

for /f "tokens=2,*" %%A in ('reg query "HKCU\Software\Epic Games\Unreal Engine\Builds" /v "%ENGINE_ASSOCIATION%" 2^>nul') do (
	if /I "%%A"=="REG_SZ" (
		call :ResolveBuildInDirectory "%%~B"
		if defined BUILD_BATCH exit /b 0
	)
)

for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\%ENGINE_ASSOCIATION%" /v InstalledDirectory 2^>nul') do (
	if /I "%%A"=="REG_SZ" (
		call :ResolveBuildInDirectory "%%~B"
		if defined BUILD_BATCH exit /b 0
	)
)

for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\%ENGINE_ASSOCIATION%" /v InstalledDirectory 2^>nul') do (
	if /I "%%A"=="REG_SZ" (
		call :ResolveBuildInDirectory "%%~B"
		if defined BUILD_BATCH exit /b 0
	)
)

call :ResolveBuildInDirectory "%ProgramFiles%\Epic Games\UE_%ENGINE_ASSOCIATION%"
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

:ResolveBuildFromEditor
if not exist "%~1" exit /b 1
call :ResolveBuildInDirectory "%~dp1..\..\.."
exit /b 0

:ResolveBuildInDirectory
set "ENGINE_CANDIDATE=%~1"
if not defined ENGINE_CANDIDATE exit /b 1

if exist "%ENGINE_CANDIDATE%\Engine\Build\BatchFiles\Build.bat" (
	for %%I in ("%ENGINE_CANDIDATE%") do set "ENGINE_ROOT=%%~fI"
	for %%I in ("%ENGINE_CANDIDATE%\Engine\Build\BatchFiles\Build.bat") do set "BUILD_BATCH=%%~fI"
	exit /b 0
)

if exist "%ENGINE_CANDIDATE%\Build\BatchFiles\Build.bat" (
	for %%I in ("%ENGINE_CANDIDATE%") do set "ENGINE_ROOT=%%~fI"
	for %%I in ("%ENGINE_CANDIDATE%\Build\BatchFiles\Build.bat") do set "BUILD_BATCH=%%~fI"
	exit /b 0
)

exit /b 1
