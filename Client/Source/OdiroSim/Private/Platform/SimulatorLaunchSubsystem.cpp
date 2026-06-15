
#include "Platform/SimulatorLaunchSubsystem.h"
#include "Scenario/ScenarioSampleWorldSpecAdapter.h"
#include "Scenario/ScenarioSimulationProfileAdapter.h"
#include "Scenario/ScenarioTemplateWorldSpecAdapter.h"

DEFINE_LOG_CATEGORY_STATIC(LogSimulatorLaunch, Log, All);

namespace
{
	const TCHAR* ScenarioInputDirectory = TEXT("Json/Input");
	const TCHAR* ExperimentInputDirectory = TEXT("Json/Experiments");
	const TCHAR* SimulatorLaunchPolicySpecInputDirectory = TEXT("Json/Input/PolicySpecs");
	const TCHAR* LegacyEvaluationReportOutputDirectory = TEXT("Json/Output");
	const TCHAR* SimulationRunStatusDirectory = TEXT("Saved/SimulationRuns");
	const TCHAR* PreviewLauncherFileName = TEXT("Task-RunPreview.bat");
	const TCHAR* LegacyEvaluationReportSchema = TEXT("episode_evaluation_report");
	const TCHAR* EpisodeResultSchema = TEXT("episode_result");
	const TCHAR* RunSummarySchema = TEXT("run_summary");
	const TCHAR* SimulatorProcessFlags = TEXT("-nosound -unattended -NoLoadingScreen");

	FString ToProjectRelativePath(FString filePath)
	{
		// UI에는 machine-specific absolute path 대신 packaged args와 같은 project-relative path를 노출한다.
		const FString projectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		FPaths::MakePathRelativeTo(filePath, *projectDir);
		return filePath.Replace(TEXT("\\"), TEXT("/"));
	}

	void FindProjectFiles(const FString& relativeDirectory, const TCHAR* filePattern, TArray<FString>& outFiles)
	{
		outFiles.Reset();
		if (relativeDirectory.TrimStartAndEnd().IsEmpty())
		{
			return;
		}

		const FString searchRoot = FExperimentSettingJson::ResolveProjectPath(relativeDirectory);
		TArray<FString> foundFiles;
		IFileManager::Get().FindFilesRecursive(foundFiles, *searchRoot, filePattern, true, false);

		for (FString filePath : foundFiles)
		{
			outFiles.Add(ToProjectRelativePath(filePath));
		}

		outFiles.Sort();
	}

	void FindProjectJsonFiles(const FString& relativeDirectory, TArray<FString>& outFiles)
	{
		FindProjectFiles(relativeDirectory, TEXT("*.json"), outFiles);
	}

	bool IsReferenceSampleJsonFile(const FString& jsonFile)
	{
		return FPaths::GetBaseFilename(jsonFile).Contains(TEXT("Sample"), ESearchCase::IgnoreCase);
	}

	bool TryReadJsonSchema(const FString& jsonFile, FString& outSchema)
	{
		outSchema.Reset();

		FString jsonString;
		if (!FFileHelper::LoadFileToString(jsonString, *FExperimentSettingJson::ResolveProjectPath(jsonFile)))
		{
			return false;
		}

		TSharedPtr<FJsonObject> rootObject;
		const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(jsonString);
		if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
		{
			return false;
		}

		return rootObject->TryGetStringField(TEXT("schema"), outSchema);
	}

	bool HasJsonSchema(const FString& jsonFile, const TCHAR* expectedSchema)
	{
		FString schema;
		return TryReadJsonSchema(jsonFile, schema)
			&& schema.Equals(expectedSchema, ESearchCase::CaseSensitive);
	}

	void FindJsonFilesWithSchema(const FString& relativeDirectory, const TCHAR* expectedSchema, TArray<FString>& outFiles)
	{
		TArray<FString> jsonFiles;
		FindProjectJsonFiles(relativeDirectory, jsonFiles);
		for (const FString& jsonFile : jsonFiles)
		{
			if (HasJsonSchema(jsonFile, expectedSchema))
			{
				outFiles.AddUnique(jsonFile);
			}
		}
	}

	FString JoinStringDiagnostics(const TArray<FString>& diagnostics)
	{
		return FString::Join(diagnostics, TEXT("\n"));
	}

	void AppendExperimentSettingDiagnostics(
		const TArray<FScenarioSchemaDiagnostic>& diagnostics,
		TArray<FString>& outDiagnostics)
	{
		for (const FScenarioSchemaDiagnostic& diagnostic : diagnostics)
		{
			outDiagnostics.Add(FString::Printf(TEXT("%s: %s"), *diagnostic.Code, *diagnostic.Message));
		}
	}

	FString ResolvePolicySpecReferencePath(const FString& policySpecJsonPath)
	{
		FString normalizedPath = policySpecJsonPath.TrimStartAndEnd();
		if (normalizedPath.IsEmpty())
		{
			return FString{};
		}

		FPaths::NormalizeFilename(normalizedPath);
		if (FPaths::GetExtension(normalizedPath).IsEmpty())
		{
			normalizedPath = FPaths::SetExtension(normalizedPath, TEXT("json"));
		}

		if (FPaths::IsRelative(normalizedPath) && FPaths::GetPath(normalizedPath).IsEmpty())
		{
			normalizedPath = FPaths::Combine(SimulatorLaunchPolicySpecInputDirectory, normalizedPath);
		}

		return FPaths::IsRelative(normalizedPath)
			? FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), normalizedPath))
			: normalizedPath;
	}

	FString MakeSimulatorRunId()
	{
		const FString timestamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S"));
		const FString guid = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
		return FString::Printf(TEXT("simulator-run-%s-%s"), *timestamp, *guid);
	}

	bool IsUnrealEditorExecutable()
	{
		// Editor preview에서만 Task-RunPreview.bat fallback을 쓴다. Packaged game은 자기 executable을 다시 실행한다.
		const FString executableName = FPaths::GetBaseFilename(FPlatformProcess::ExecutablePath());
		return executableName.StartsWith(TEXT("UnrealEditor"), ESearchCase::IgnoreCase);
	}

	bool IsScenarioSourceFile(const FString& jsonFile)
	{
		if (FScenarioSampleWorldSpecAdapter::IsScenarioSampleFile(jsonFile))
		{
			return FScenarioSampleWorldSpecAdapter::CompileScenarioWorldSpecFromSampleFile(jsonFile).bSuccess;
		}

		if (FScenarioTemplateWorldSpecAdapter::IsScenarioTemplateFile(jsonFile))
		{
			const FScenarioTemplateSampleRequest request =
				FScenarioTemplateWorldSpecAdapter::MakeDefaultSampleRequest(jsonFile);
			return FScenarioTemplateWorldSpecAdapter::CompileScenarioWorldSpecFromTemplateFile(jsonFile, request).bSuccess;
		}

		return false;
	}

	bool IsSimulationProfileFile(const FString& jsonFile)
	{
		if (FScenarioSimulationProfileAdapter::IsSimulationProfileFile(jsonFile))
		{
			return FScenarioSimulationProfileAdapter::CompileProfileFromJsonFile(jsonFile).bSuccess;
		}

		return false;
	}

	bool IsPolicySpecFile(const FString& jsonFile)
	{
		FString jsonString;
		if (!FFileHelper::LoadFileToString(jsonString, *ResolvePolicySpecReferencePath(jsonFile)))
		{
			return false;
		}

		TSharedPtr<FJsonObject> rootObject;
		const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(jsonString);
		if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* policySpecObject = nullptr;
		return rootObject->TryGetObjectField(TEXT("policySpec"), policySpecObject)
			&& policySpecObject != nullptr
			&& policySpecObject->IsValid();
	}

}

void USimulatorLaunchSubsystem::Deinitialize()
{
	StopActiveRun();
	Super::Deinitialize();
}

TArray<FString> USimulatorLaunchSubsystem::ListExperimentRefs() const
{
	TArray<FString> settingFiles;
	FindProjectFiles(ExperimentInputDirectory, TEXT("setting.json"), settingFiles);

	TArray<FString> experimentRefs;
	for (const FString& settingFile : settingFiles)
	{
		const FString experimentRef = FPaths::GetPath(settingFile);
		const FExperimentSettingParseResult settingResult =
			FExperimentSettingJson::ParseFromFile(FExperimentSettingJson::BuildExperimentSettingPath(experimentRef));
		if (settingResult.bSuccess)
		{
			experimentRefs.AddUnique(experimentRef);
		}
	}

	experimentRefs.Sort();
	return experimentRefs;
}

TArray<FString> USimulatorLaunchSubsystem::ListScenarioSetupFiles() const
{
	TArray<FString> jsonFiles;
	FindProjectJsonFiles(ScenarioInputDirectory, jsonFiles);

	TArray<FString> setupFiles;
	for (const FString& jsonFile : jsonFiles)
	{
		if (IsReferenceSampleJsonFile(jsonFile))
		{
			continue;
		}

		if (IsScenarioSourceFile(jsonFile))
		{
			setupFiles.Add(jsonFile);
		}
	}

	return setupFiles;
}

TArray<FString> USimulatorLaunchSubsystem::ListDeliveryBotSetupFiles() const
{
	TArray<FString> jsonFiles;
	FindProjectJsonFiles(ScenarioInputDirectory, jsonFiles);

	TArray<FString> setupFiles;
	for (const FString& jsonFile : jsonFiles)
	{
		if (IsReferenceSampleJsonFile(jsonFile))
		{
			continue;
		}

		if (IsSimulationProfileFile(jsonFile))
		{
			setupFiles.Add(jsonFile);
		}
	}

	return setupFiles;
}

TArray<FString> USimulatorLaunchSubsystem::ListPolicySpecFiles() const
{
	TArray<FString> jsonFiles;
	FindProjectJsonFiles(SimulatorLaunchPolicySpecInputDirectory, jsonFiles);

	TArray<FString> policySpecFiles;
	for (const FString& jsonFile : jsonFiles)
	{
		if (IsPolicySpecFile(jsonFile))
		{
			policySpecFiles.Add(jsonFile);
		}
	}

	return policySpecFiles;
}

TArray<FString> USimulatorLaunchSubsystem::ListLegacyEvaluationReportFiles() const
{
	TArray<FString> reportFiles;
	FindJsonFilesWithSchema(LegacyEvaluationReportOutputDirectory, LegacyEvaluationReportSchema, reportFiles);
	FindJsonFilesWithSchema(SimulationRunStatusDirectory, LegacyEvaluationReportSchema, reportFiles);
	reportFiles.Sort();
	return reportFiles;
}

TArray<FString> USimulatorLaunchSubsystem::ListSimulationRunResultDirectories() const
{
	TArray<FString> candidateFiles;
	FindProjectFiles(SimulationRunStatusDirectory, TEXT("*.json"), candidateFiles);
	TArray<FString> experimentRunFiles;
	FindProjectFiles(ExperimentInputDirectory, TEXT("summary.json"), experimentRunFiles);
	candidateFiles.Append(experimentRunFiles);
	experimentRunFiles.Reset();
	FindProjectFiles(ExperimentInputDirectory, TEXT("status.json"), experimentRunFiles);
	candidateFiles.Append(experimentRunFiles);

	TArray<FString> resultDirectories;
	for (const FString& candidateFile : candidateFiles)
	{
		FString schema;
		if (!TryReadJsonSchema(candidateFile, schema))
		{
			continue;
		}

		if (!schema.Equals(TEXT("simulation_run_status"), ESearchCase::CaseSensitive)
			&& !schema.Equals(LegacyEvaluationReportSchema, ESearchCase::CaseSensitive)
			&& !schema.Equals(RunSummarySchema, ESearchCase::CaseSensitive))
		{
			continue;
		}

		const FString resultDirectory = FPaths::GetPath(candidateFile);
		if (resultDirectory.Equals(SimulationRunStatusDirectory, ESearchCase::IgnoreCase))
		{
			continue;
		}
		resultDirectories.AddUnique(resultDirectory);
	}

	resultDirectories.Sort();
	return resultDirectories;
}

TArray<FString> USimulatorLaunchSubsystem::ListEpisodeResultFilesInDirectory(const FString& runDirectory) const
{
	TArray<FString> candidateFiles;
	FindProjectFiles(runDirectory, TEXT("*.json"), candidateFiles);

	TArray<FString> resultFiles;
	for (const FString& candidateFile : candidateFiles)
	{
		if (HasJsonSchema(candidateFile, EpisodeResultSchema))
		{
			resultFiles.Add(candidateFile);
		}
	}

	resultFiles.Sort();
	return resultFiles;
}

TArray<FString> USimulatorLaunchSubsystem::ListMeasurementLogFilesInDirectory(const FString& runDirectory) const
{
	TArray<FString> logFiles;
	FindProjectFiles(runDirectory, TEXT("*.jsonl"), logFiles);
	return logFiles;
}

TArray<FString> USimulatorLaunchSubsystem::ListSimulationRunStatusFiles() const
{
	TArray<FString> jsonFiles;
	FindProjectJsonFiles(SimulationRunStatusDirectory, jsonFiles);

	TArray<FString> statusFiles;
	for (const FString& jsonFile : jsonFiles)
	{
		FSimulationRunStatus status;
		TArray<FString> diagnostics;
		if (FSimulationRunStatusJson::ParseFromFile(jsonFile, status, diagnostics))
		{
			statusFiles.Add(jsonFile);
		}
	}

	return statusFiles;
}

FExperimentSettingParseResult USimulatorLaunchSubsystem::LoadExperimentSettingFile(const FString& experimentRef) const
{
	return FExperimentSettingJson::ParseFromFile(FExperimentSettingJson::BuildExperimentSettingPath(experimentRef));
}

bool USimulatorLaunchSubsystem::StartExperimentRun(const FString& experimentRef, const FString& requestedRunId)
{
	if (ActiveProcessHandle.IsValid() && FPlatformProcess::IsProcRunning(ActiveProcessHandle))
	{
		ActiveRunInfo.LastError = TEXT("A simulator process is already running.");
		BroadcastRunInfoChanged();
		return false;
	}

	CloseActiveProcessHandle();

	const FString normalizedExperimentRef = experimentRef.TrimStartAndEnd().Replace(TEXT("\\"), TEXT("/"));
	const FExperimentSettingParseResult settingResult = LoadExperimentSettingFile(normalizedExperimentRef);
	if (!settingResult.bSuccess)
	{
		ActiveRunInfo = FSimulatorRunInfo{};
		ActiveRunInfo.SetupPath = normalizedExperimentRef;
		ActiveRunInfo.Status.State = ESimulationRunState::Failed;
		AppendExperimentSettingDiagnostics(settingResult.Diagnostics, ActiveRunInfo.Diagnostics);
		ActiveRunInfo.LastError = JoinStringDiagnostics(ActiveRunInfo.Diagnostics);
		BroadcastRunInfoChanged();
		return false;
	}

	const FString runId = requestedRunId.IsEmpty() ? MakeSimulatorRunId() : requestedRunId;
	const FString statusPath = FExperimentSettingJson::BuildExperimentRunStatusPath(normalizedExperimentRef, runId);

	FString executable;
	FString arguments;
	bool bUsesPreviewLauncher = false;
	if (!BuildLaunchCommand(normalizedExperimentRef, runId, executable, arguments, bUsesPreviewLauncher))
	{
		ActiveRunInfo = FSimulatorRunInfo{};
		ActiveRunInfo.RunId = runId;
		ActiveRunInfo.SetupPath = normalizedExperimentRef;
		ActiveRunInfo.StatusPath = statusPath;
		MarkActiveRunFailed(TEXT("Simulator launch command could not be built."));
		return false;
	}

	ActiveRunInfo = FSimulatorRunInfo{};
	ActiveRunInfo.RunId = runId;
	ActiveRunInfo.SetupPath = normalizedExperimentRef;
	ActiveRunInfo.StatusPath = statusPath;
	ActiveRunInfo.LaunchExecutable = executable;
	ActiveRunInfo.LaunchArguments = arguments;
	ActiveRunInfo.bUsedPreviewLauncher = bUsesPreviewLauncher;
	ActiveRunInfo.Status.RunId = runId;
	ActiveRunInfo.Status.SetupPath = normalizedExperimentRef;
	ActiveRunInfo.Status.State = ESimulationRunState::Pending;

	// 동일 status path를 재사용할 수 있으므로 이전 run의 terminal status를 먼저 제거한다.
	const FString resolvedStatusPath = FExperimentSettingJson::ResolveProjectPath(ActiveRunInfo.StatusPath);
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

FString USimulatorLaunchSubsystem::BuildSimulatorArgumentString(const FString& experimentRef, const FString& runId)
{
	return FString::Printf(
		TEXT("%s %s %s"),
		*QuoteCommandLineArgument(FString::Printf(TEXT("-Experiment=%s"), *experimentRef)),
		*QuoteCommandLineArgument(FString::Printf(TEXT("-RunId=%s"), *runId)),
		SimulatorProcessFlags);
}

FString USimulatorLaunchSubsystem::BuildPreviewLauncherArgumentString(
	const FString& previewBatPath,
	const FString& experimentRef,
	const FString& runId)
{
	// cmd.exe quoting is intentionally centralized here; CreateProc receives cmd.exe as executable.
	return FString::Printf(
		TEXT("/d /s /c \"\"%s\" %s %s %s\""),
		*previewBatPath,
		*QuoteCommandLineArgument(FString::Printf(TEXT("-Experiment=%s"), *experimentRef)),
		*QuoteCommandLineArgument(FString::Printf(TEXT("-RunId=%s"), *runId)),
		SimulatorProcessFlags);
}

bool USimulatorLaunchSubsystem::BuildLaunchCommand(
	const FString& experimentRef,
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
		outArguments = BuildPreviewLauncherArgumentString(previewBatPath, experimentRef, runId);
		bOutUsesPreviewLauncher = true;
		return true;
	}

	outExecutable = FPlatformProcess::ExecutablePath();
	outArguments = BuildSimulatorArgumentString(experimentRef, runId);
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
