#include "Platform/SimulatorLaunchSubsystem.h"

#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSimulatorLaunch, Log, All);

namespace
{
	const TCHAR* SimulationSetupInputDirectory = TEXT("Json/Input");
	const TCHAR* EvaluationReportOutputDirectory = TEXT("Json/Output");
	const TCHAR* PreviewLauncherFileName = TEXT("RunPreview.bat");

	FString ToProjectRelativePath(FString filePath)
	{
		// UI에는 machine-specific absolute path 대신 packaged args와 같은 project-relative path를 노출한다.
		const FString projectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		FPaths::MakePathRelativeTo(filePath, *projectDir);
		return filePath.Replace(TEXT("\\"), TEXT("/"));
	}

	void FindProjectJsonFiles(const FString& relativeDirectory, TArray<FString>& outFiles)
	{
		outFiles.Reset();

		const FString searchRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), relativeDirectory));
		TArray<FString> foundFiles;
		IFileManager::Get().FindFilesRecursive(foundFiles, *searchRoot, TEXT("*.json"), true, false);

		for (FString filePath : foundFiles)
		{
			outFiles.Add(ToProjectRelativePath(filePath));
		}

		outFiles.Sort();
	}

	FString JoinDiagnostics(const TArray<FEpisodeCompileDiagnostic>& diagnostics)
	{
		TArray<FString> lines;
		lines.Reserve(diagnostics.Num());
		for (const FEpisodeCompileDiagnostic& diagnostic : diagnostics)
		{
			lines.Add(FString::Printf(TEXT("%s: %s"), *diagnostic.Code, *diagnostic.Message));
		}

		return FString::Join(lines, TEXT("\n"));
	}

	FString JoinStringDiagnostics(const TArray<FString>& diagnostics)
	{
		return FString::Join(diagnostics, TEXT("\n"));
	}

	FString MakeSimulatorRunId()
	{
		const FString timestamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S"));
		const FString guid = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
		return FString::Printf(TEXT("simulator-run-%s-%s"), *timestamp, *guid);
	}

	bool IsUnrealEditorExecutable()
	{
		// Editor preview에서만 RunPreview.bat fallback을 쓴다. Packaged game은 자기 executable을 다시 실행한다.
		const FString executableName = FPaths::GetBaseFilename(FPlatformProcess::ExecutablePath());
		return executableName.StartsWith(TEXT("UnrealEditor"), ESearchCase::IgnoreCase);
	}
}

void USimulatorLaunchSubsystem::Deinitialize()
{
	StopActiveRun();
	Super::Deinitialize();
}

TArray<FString> USimulatorLaunchSubsystem::ListSimulationSetupFiles() const
{
	TArray<FString> jsonFiles;
	FindProjectJsonFiles(SimulationSetupInputDirectory, jsonFiles);

	TArray<FString> setupFiles;
	for (const FString& jsonFile : jsonFiles)
	{
		if (FSimulationSetupJson::ParseFromFile(jsonFile).bSuccess)
		{
			setupFiles.Add(jsonFile);
		}
	}

	return setupFiles;
}

TArray<FString> USimulatorLaunchSubsystem::ListEvaluationReportFiles() const
{
	TArray<FString> reportFiles;
	FindProjectJsonFiles(EvaluationReportOutputDirectory, reportFiles);
	return reportFiles;
}

FSimulationSetupParseResult USimulatorLaunchSubsystem::LoadSimulationSetupFile(const FString& setupPath) const
{
	return FSimulationSetupJson::ParseFromFile(setupPath);
}

bool USimulatorLaunchSubsystem::SaveFixedStepFpsToSetupFile(
	const FString& setupPath,
	int32 fps,
	TArray<FString>& outDiagnostics) const
{
	outDiagnostics.Reset();

	if (setupPath.IsEmpty())
	{
		outDiagnostics.Add(TEXT("SimulationSetup path must not be empty."));
		return false;
	}

	if (fps <= 0)
	{
		outDiagnostics.Add(TEXT("fixed_step.fps must be > 0."));
		return false;
	}

	const FString resolvedSetupPath = FSimulationSetupJson::ResolveProjectPath(setupPath);
	FString jsonString;
	if (!FFileHelper::LoadFileToString(jsonString, *resolvedSetupPath))
	{
		outDiagnostics.Add(FString::Printf(TEXT("SimulationSetup read failed: %s"), *resolvedSetupPath));
		return false;
	}

	TSharedPtr<FJsonObject> rootObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(jsonString);
	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		outDiagnostics.Add(TEXT("SimulationSetup JSON parse failed."));
		return false;
	}

	TSharedPtr<FJsonObject> fixedStepObject;
	const TSharedPtr<FJsonValue> fixedStepValue = rootObject->TryGetField(TEXT("fixed_step"));
	if (fixedStepValue.IsValid() && fixedStepValue->Type == EJson::Object)
	{
		fixedStepObject = fixedStepValue->AsObject();
	}

	if (!fixedStepObject.IsValid())
	{
		// UI는 fixed_step.fps만 편집한다. 나머지 JSON 구조와 unknown field는 그대로 둔다.
		fixedStepObject = MakeShared<FJsonObject>();
		rootObject->SetObjectField(TEXT("fixed_step"), fixedStepObject);
	}

	fixedStepObject->SetNumberField(TEXT("fps"), fps);

	FString updatedJsonString;
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&updatedJsonString);
	if (!FJsonSerializer::Serialize(rootObject.ToSharedRef(), writer))
	{
		outDiagnostics.Add(TEXT("SimulationSetup JSON serialization failed."));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(
			updatedJsonString,
			*resolvedSetupPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		outDiagnostics.Add(FString::Printf(TEXT("SimulationSetup write failed: %s"), *resolvedSetupPath));
		return false;
	}

	const FSimulationSetupParseResult parseResult = FSimulationSetupJson::ParseFromFile(setupPath);
	if (!parseResult.bSuccess)
	{
		outDiagnostics.Add(JoinDiagnostics(parseResult.Diagnostics));
		return false;
	}

	return true;
}

bool USimulatorLaunchSubsystem::StartSimulationRun(const FString& setupPath, const FString& requestedRunId)
{
	if (ActiveProcessHandle.IsValid() && FPlatformProcess::IsProcRunning(ActiveProcessHandle))
	{
		ActiveRunInfo.LastError = TEXT("A simulator process is already running.");
		BroadcastRunInfoChanged();
		return false;
	}

	CloseActiveProcessHandle();

	const FSimulationSetupParseResult setupParseResult = FSimulationSetupJson::ParseFromFile(setupPath);
	if (!setupParseResult.bSuccess)
	{
		ActiveRunInfo = FSimulatorRunInfo{};
		ActiveRunInfo.SetupPath = setupPath;
		ActiveRunInfo.Status.State = ESimulationRunState::Failed;
		ActiveRunInfo.LastError = JoinDiagnostics(setupParseResult.Diagnostics);
		BroadcastRunInfoChanged();
		return false;
	}

	const FString runId = requestedRunId.IsEmpty() ? MakeSimulatorRunId() : requestedRunId;

	FString executable;
	FString arguments;
	bool bUsesPreviewLauncher = false;
	if (!BuildLaunchCommand(setupPath, runId, executable, arguments, bUsesPreviewLauncher))
	{
		ActiveRunInfo = FSimulatorRunInfo{};
		ActiveRunInfo.RunId = runId;
		ActiveRunInfo.SetupPath = setupPath;
		ActiveRunInfo.StatusPath = setupParseResult.Setup.Status.OutputPath;
		MarkActiveRunFailed(TEXT("Simulator launch command could not be built."));
		return false;
	}

	ActiveRunInfo = FSimulatorRunInfo{};
	ActiveRunInfo.RunId = runId;
	ActiveRunInfo.SetupPath = setupPath;
	ActiveRunInfo.StatusPath = setupParseResult.Setup.Status.OutputPath;
	ActiveRunInfo.LaunchExecutable = executable;
	ActiveRunInfo.LaunchArguments = arguments;
	ActiveRunInfo.bUsedPreviewLauncher = bUsesPreviewLauncher;
	ActiveRunInfo.Status.RunId = runId;
	ActiveRunInfo.Status.SetupPath = setupPath;
	ActiveRunInfo.Status.State = ESimulationRunState::Pending;

	// 동일 status path를 재사용할 수 있으므로 이전 run의 terminal status를 먼저 제거한다.
	const FString resolvedStatusPath = FSimulationSetupJson::ResolveProjectPath(ActiveRunInfo.StatusPath);
	IFileManager::Get().Delete(*resolvedStatusPath, false, true);

	uint32 processId = 0;
	const FString workingDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	ActiveProcessHandle = FPlatformProcess::CreateProc(
		*executable,
		*arguments,
		false,
		false,
		false,
		&processId,
		0,
		*workingDirectory,
		nullptr);

	if (!ActiveProcessHandle.IsValid())
	{
		ActiveRunInfo.bProcessStarted = false;
		MarkActiveRunFailed(FString::Printf(TEXT("Simulator process start failed: %s"), *executable));
		return false;
	}

	ActiveRunInfo.bProcessStarted = true;
	ActiveRunInfo.bProcessRunning = true;
	StartPolling();
	BroadcastRunInfoChanged();

	UE_LOG(
		LogSimulatorLaunch,
		Log,
		TEXT("Simulator process started | RunId: %s, Executable: %s, Arguments: %s"),
		*runId,
		*executable,
		*arguments);

	return true;
}

bool USimulatorLaunchSubsystem::RefreshActiveRunStatus()
{
	if (ActiveRunInfo.RunId.IsEmpty() && !ActiveRunInfo.bProcessStarted)
	{
		return false;
	}

	FSimulationRunStatus status;
	TArray<FString> diagnostics;
	const bool bStatusRead = FSimulationRunStatusJson::ParseFromFile(ActiveRunInfo.StatusPath, status, diagnostics);
	if (bStatusRead)
	{
		if (!status.RunId.Equals(ActiveRunInfo.RunId, ESearchCase::CaseSensitive))
		{
			// Status file은 setup마다 고정될 수 있어, 다른 run id의 stale status는 완료로 처리하지 않는다.
			ActiveRunInfo.Diagnostics.Reset();
			ActiveRunInfo.Diagnostics.Add(FString::Printf(
				TEXT("Waiting for simulator status run id '%s'. Current status file has '%s'."),
				*ActiveRunInfo.RunId,
				*status.RunId));
		}
		else
		{
			ActiveRunInfo.Status = status;
			ActiveRunInfo.Diagnostics.Reset();
			ActiveRunInfo.LastError = status.State == ESimulationRunState::Failed ? status.Error : FString();
		}
	}
	else
	{
		SetActiveRunDiagnostics(diagnostics);
	}

	RefreshActiveProcessState();

	if (IsTerminalRunState(ActiveRunInfo.Status.State))
	{
		if (ActiveRunInfo.bProcessRunning && ActiveProcessHandle.IsValid())
		{
			// Simulator가 terminal status를 쓴 뒤 창이 남아 있으면 launcher가 run lifecycle을 닫는다.
			FPlatformProcess::TerminateProc(ActiveProcessHandle, true);
			ActiveRunInfo.bProcessRunning = false;
		}

		StopPolling();
		CloseActiveProcessHandle();
		BroadcastRunInfoChanged();
		return bStatusRead;
	}

	if (ActiveRunInfo.bProcessStarted && !ActiveRunInfo.bProcessRunning)
	{
		// Process start failure와 구분되는 simulator-side abnormal exit 경로다.
		ActiveRunInfo.Status.State = ESimulationRunState::Failed;
		ActiveRunInfo.LastError = TEXT("Simulator process exited before writing a terminal status.");
		StopPolling();
		CloseActiveProcessHandle();
		BroadcastRunInfoChanged();
		return false;
	}

	BroadcastRunInfoChanged();
	return bStatusRead;
}

void USimulatorLaunchSubsystem::StopActiveRun()
{
	if (ActiveProcessHandle.IsValid())
	{
		if (FPlatformProcess::IsProcRunning(ActiveProcessHandle))
		{
			FPlatformProcess::TerminateProc(ActiveProcessHandle, true);
		}

		CloseActiveProcessHandle();
	}

	if (!ActiveRunInfo.RunId.IsEmpty() && !IsTerminalRunState(ActiveRunInfo.Status.State))
	{
		ActiveRunInfo.Status.State = ESimulationRunState::Canceled;
		ActiveRunInfo.bProcessRunning = false;
		BroadcastRunInfoChanged();
	}

	StopPolling();
}

bool USimulatorLaunchSubsystem::IsTerminalRunState(ESimulationRunState state)
{
	return state == ESimulationRunState::Completed
		|| state == ESimulationRunState::Failed
		|| state == ESimulationRunState::Canceled;
}

FString USimulatorLaunchSubsystem::QuoteCommandLineArgument(const FString& value)
{
	FString escapedValue = value;
	escapedValue.ReplaceInline(TEXT("\""), TEXT("\\\""));
	return FString::Printf(TEXT("\"%s\""), *escapedValue);
}

FString USimulatorLaunchSubsystem::BuildSimulatorArgumentString(const FString& setupPath, const FString& runId)
{
	return FString::Printf(
		TEXT("%s %s"),
		*QuoteCommandLineArgument(FString::Printf(TEXT("-Simulate=%s"), *setupPath)),
		*QuoteCommandLineArgument(FString::Printf(TEXT("-RunId=%s"), *runId)));
}

FString USimulatorLaunchSubsystem::BuildPreviewLauncherArgumentString(
	const FString& previewBatPath,
	const FString& setupPath,
	const FString& runId)
{
	// cmd.exe quoting is intentionally centralized here; CreateProc receives cmd.exe as executable.
	return FString::Printf(
		TEXT("/d /s /c \"\"%s\" %s %s\""),
		*previewBatPath,
		*QuoteCommandLineArgument(FString::Printf(TEXT("-Simulate=%s"), *setupPath)),
		*QuoteCommandLineArgument(FString::Printf(TEXT("-RunId=%s"), *runId)));
}

bool USimulatorLaunchSubsystem::BuildLaunchCommand(
	const FString& setupPath,
	const FString& runId,
	FString& outExecutable,
	FString& outArguments,
	bool& bOutUsesPreviewLauncher) const
{
	bOutUsesPreviewLauncher = false;

	FString previewBatPath;
	if (ShouldUsePreviewLauncher(previewBatPath))
	{
		outExecutable = TEXT("cmd.exe");
		outArguments = BuildPreviewLauncherArgumentString(previewBatPath, setupPath, runId);
		bOutUsesPreviewLauncher = true;
		return true;
	}

	outExecutable = FPlatformProcess::ExecutablePath();
	outArguments = BuildSimulatorArgumentString(setupPath, runId);
	return !outExecutable.IsEmpty();
}

bool USimulatorLaunchSubsystem::ShouldUsePreviewLauncher(FString& outPreviewBatPath) const
{
	outPreviewBatPath.Reset();

	if (!IsUnrealEditorExecutable())
	{
		return false;
	}

	// Packaged executable이 없는 개발 중에도 packaged-style public args를 검증하기 위한 fallback이다.
	const FString previewBatPath = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectDir(), PreviewLauncherFileName));
	if (!FPaths::FileExists(previewBatPath))
	{
		return false;
	}

	outPreviewBatPath = previewBatPath;
	return true;
}

void USimulatorLaunchSubsystem::PollActiveRunStatus()
{
	RefreshActiveRunStatus();
}

void USimulatorLaunchSubsystem::StartPolling()
{
	UWorld* world = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!world)
	{
		return;
	}

	world->GetTimerManager().SetTimer(
		PollTimerHandle,
		this,
		&USimulatorLaunchSubsystem::PollActiveRunStatus,
		1.0f,
		true);
}

void USimulatorLaunchSubsystem::StopPolling()
{
	UWorld* world = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (world && PollTimerHandle.IsValid())
	{
		world->GetTimerManager().ClearTimer(PollTimerHandle);
	}

	PollTimerHandle.Invalidate();
}

void USimulatorLaunchSubsystem::CloseActiveProcessHandle()
{
	if (ActiveProcessHandle.IsValid())
	{
		FPlatformProcess::CloseProc(ActiveProcessHandle);
		ActiveProcessHandle.Reset();
	}
}

void USimulatorLaunchSubsystem::RefreshActiveProcessState()
{
	if (!ActiveProcessHandle.IsValid())
	{
		ActiveRunInfo.bProcessRunning = false;
		return;
	}

	ActiveRunInfo.bProcessRunning = FPlatformProcess::IsProcRunning(ActiveProcessHandle);

	int32 returnCode = INDEX_NONE;
	if (FPlatformProcess::GetProcReturnCode(ActiveProcessHandle, &returnCode))
	{
		ActiveRunInfo.ProcessReturnCode = returnCode;
	}
}

void USimulatorLaunchSubsystem::MarkActiveRunFailed(const FString& error)
{
	ActiveRunInfo.Status.State = ESimulationRunState::Failed;
	ActiveRunInfo.LastError = error;
	ActiveRunInfo.bProcessRunning = false;
	StopPolling();
	CloseActiveProcessHandle();
	BroadcastRunInfoChanged();
}

void USimulatorLaunchSubsystem::BroadcastRunInfoChanged()
{
	OnRunInfoChanged.Broadcast(ActiveRunInfo);
}

void USimulatorLaunchSubsystem::SetActiveRunDiagnostics(const TArray<FString>& diagnostics)
{
	ActiveRunInfo.Diagnostics = diagnostics;
	if (!diagnostics.IsEmpty())
	{
		UE_LOG(LogSimulatorLaunch, Verbose, TEXT("Simulator status polling diagnostic | %s"), *JoinStringDiagnostics(diagnostics));
	}
}
