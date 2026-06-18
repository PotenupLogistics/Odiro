
#include "Platform/SimulatorLaunchSubsystem.h"
#include "DeliveryBot/DeliveryBotSetupCompiler.h"
#include "Scenario/ScenarioCompiler.h"
#include "Scenario/ScenarioSampleWorldSpecAdapter.h"

DEFINE_LOG_CATEGORY_STATIC(LogSimulatorLaunch, Log, All);

namespace
{
	const TCHAR* SimulationSetupInputDirectory = TEXT("Json/Input");
	const TCHAR* SimulatorLaunchPolicySpecInputDirectory = TEXT("Json/Input/PolicySpecs");
	const TCHAR* SimulationRunStatusDirectory = TEXT("Saved/SimulationRuns");
	const TCHAR* PreviewLauncherFileName = TEXT("Task-RunPreview.bat");
	const TCHAR* LaunchScenarioSetupSchema = TEXT("scenario_actor_spawn_mvp");
	const TCHAR* LaunchDeliveryBotSetupSchema = TEXT("delivery_bot_setup");
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

		const FString searchRoot = FSimulationSetupJson::ResolveProjectPath(relativeDirectory);
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
		if (!FFileHelper::LoadFileToString(jsonString, *FSimulationSetupJson::ResolveProjectPath(jsonFile)))
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

	FString JoinDiagnostics(const TArray<FScenarioCompileDiagnostic>& diagnostics)
	{
		TArray<FString> lines;
		lines.Reserve(diagnostics.Num());
		for (const FScenarioCompileDiagnostic& diagnostic : diagnostics)
		{
			lines.Add(FString::Printf(TEXT("%s: %s"), *diagnostic.Code, *diagnostic.Message));
		}

		return FString::Join(lines, TEXT("\n"));
	}

	FString JoinStringDiagnostics(const TArray<FString>& diagnostics)
	{
		return FString::Join(diagnostics, TEXT("\n"));
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

	bool IsSixDigitRunDirectoryName(const FString& runDirectoryName)
	{
		if (runDirectoryName.Len() != 6)
		{
			return false;
		}

		for (const TCHAR character : runDirectoryName)
		{
			if (!FChar::IsDigit(character))
			{
				return false;
			}
		}

		return true;
	}

	FString MakeNextProjectRunId(const FString& projectPath)
	{
		const FString runsPath = FPaths::Combine(projectPath, TEXT("runs"));
		TArray<FString> runDirectoryNames;
		IFileManager::Get().FindFiles(runDirectoryNames, *FPaths::Combine(runsPath, TEXT("*")), false, true);

		int32 maxRunNumber = 0;
		for (const FString& runDirectoryName : runDirectoryNames)
		{
			if (!IsSixDigitRunDirectoryName(runDirectoryName))
			{
				continue;
			}

			maxRunNumber = FMath::Max(maxRunNumber, FCString::Atoi(*runDirectoryName));
		}

		if (maxRunNumber >= 999999)
		{
			return FString();
		}

		return FString::Printf(TEXT("%06d"), maxRunNumber + 1);
	}

	void AddLaunchDiagnostic(TArray<FString>& diagnostics, const FString& message)
	{
		diagnostics.Add(message);
	}

	bool CopyProjectRunFileSnapshot(
		const FString& sourcePath,
		const FString& destinationPath,
		const TCHAR* label,
		TArray<FString>& diagnostics)
	{
		if (!FPaths::FileExists(sourcePath))
		{
			AddLaunchDiagnostic(
				diagnostics,
				FString::Printf(TEXT("%s source file is required: %s"), label, *sourcePath));
			return false;
		}

		const FString destinationDirectory = FPaths::GetPath(destinationPath);
		if (!IFileManager::Get().MakeDirectory(*destinationDirectory, true))
		{
			AddLaunchDiagnostic(
				diagnostics,
				FString::Printf(TEXT("%s snapshot directory create failed: %s"), label, *destinationDirectory));
			return false;
		}

		TArray<uint8> fileBytes;
		if (!FFileHelper::LoadFileToArray(fileBytes, *sourcePath))
		{
			AddLaunchDiagnostic(
				diagnostics,
				FString::Printf(TEXT("%s source file read failed: %s"), label, *sourcePath));
			return false;
		}

		if (!FFileHelper::SaveArrayToFile(fileBytes, *destinationPath))
		{
			AddLaunchDiagnostic(
				diagnostics,
				FString::Printf(TEXT("%s snapshot file write failed: %s"), label, *destinationPath));
			return false;
		}

		return true;
	}

	bool ShouldSkipPolicySnapshotFile(FString policyFilePath)
	{
		policyFilePath.ReplaceInline(TEXT("\\"), TEXT("/"));
		const FString extension = FPaths::GetExtension(policyFilePath);
		return policyFilePath.Contains(TEXT("/__pycache__/"))
			|| extension.Equals(TEXT("pyc"), ESearchCase::IgnoreCase)
			|| extension.Equals(TEXT("pyo"), ESearchCase::IgnoreCase);
	}

	bool CopyProjectRunPolicySnapshot(
		const FString& sourcePolicyPath,
		const FString& destinationPolicyPath,
		TArray<FString>& diagnostics)
	{
		if (!FPaths::DirectoryExists(sourcePolicyPath))
		{
			AddLaunchDiagnostic(
				diagnostics,
				FString::Printf(TEXT("policy source directory is required: %s"), *sourcePolicyPath));
			return false;
		}

		if (!IFileManager::Get().MakeDirectory(*destinationPolicyPath, true))
		{
			AddLaunchDiagnostic(
				diagnostics,
				FString::Printf(TEXT("policy snapshot directory create failed: %s"), *destinationPolicyPath));
			return false;
		}

		FString sourcePolicyRoot = sourcePolicyPath;
		sourcePolicyRoot.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (!sourcePolicyRoot.EndsWith(TEXT("/")))
		{
			sourcePolicyRoot += TEXT("/");
		}

		TArray<FString> policyFiles;
		IFileManager::Get().FindFilesRecursive(policyFiles, *sourcePolicyPath, TEXT("*"), true, false);
		for (FString policyFile : policyFiles)
		{
			policyFile.ReplaceInline(TEXT("\\"), TEXT("/"));
			if (ShouldSkipPolicySnapshotFile(policyFile))
			{
				continue;
			}

			FString relativePolicyFile = policyFile;
			FPaths::MakePathRelativeTo(relativePolicyFile, *sourcePolicyRoot);
			const FString destinationFile = FPaths::Combine(destinationPolicyPath, relativePolicyFile);
			if (!CopyProjectRunFileSnapshot(policyFile, destinationFile, TEXT("policy"), diagnostics))
			{
				return false;
			}
		}

		return true;
	}

	bool IsUnrealEditorExecutable()
	{
		// Editor preview에서만 Task-RunPreview.bat fallback을 쓴다. Packaged game은 자기 executable을 다시 실행한다.
		const FString executableName = FPaths::GetBaseFilename(FPlatformProcess::ExecutablePath());
		return executableName.StartsWith(TEXT("UnrealEditor"), ESearchCase::IgnoreCase);
	}

	bool IsScenarioSetupFile(const FString& jsonFile)
	{
		if (FScenarioSampleWorldSpecAdapter::IsScenarioSampleFile(jsonFile))
		{
			return FScenarioSampleWorldSpecAdapter::CompileScenarioWorldSpecFromSampleFile(jsonFile).bSuccess;
		}

		if (!HasJsonSchema(jsonFile, LaunchScenarioSetupSchema))
		{
			return false;
		}

		UScenarioCompiler* compiler = NewObject<UScenarioCompiler>();
		return compiler && compiler->CompileScenarioWorldSpecFromJsonFile(jsonFile).bSuccess;
	}

	bool IsDeliveryBotSetupFile(const FString& jsonFile)
	{
		if (!HasJsonSchema(jsonFile, LaunchDeliveryBotSetupSchema))
		{
			return false;
		}

		UDeliveryBotSetupCompiler* compiler = NewObject<UDeliveryBotSetupCompiler>();
		return compiler && compiler->CompileDeliveryBotSetupFromJsonFile(jsonFile).bSuccess;
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

TArray<FString> USimulatorLaunchSubsystem::ListScenarioSetupFiles() const
{
	TArray<FString> jsonFiles;
	FindProjectJsonFiles(SimulationSetupInputDirectory, jsonFiles);

	TArray<FString> setupFiles;
	for (const FString& jsonFile : jsonFiles)
	{
		if (IsReferenceSampleJsonFile(jsonFile))
		{
			continue;
		}

		if (IsScenarioSetupFile(jsonFile))
		{
			setupFiles.Add(jsonFile);
		}
	}

	return setupFiles;
}

TArray<FString> USimulatorLaunchSubsystem::ListDeliveryBotSetupFiles() const
{
	TArray<FString> jsonFiles;
	FindProjectJsonFiles(SimulationSetupInputDirectory, jsonFiles);

	TArray<FString> setupFiles;
	for (const FString& jsonFile : jsonFiles)
	{
		if (IsReferenceSampleJsonFile(jsonFile))
		{
			continue;
		}

		if (IsDeliveryBotSetupFile(jsonFile))
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

TArray<FString> USimulatorLaunchSubsystem::ListSimulationRunResultDirectories() const
{
	TArray<FString> candidateFiles;
	FindProjectFiles(SimulationRunStatusDirectory, TEXT("*.json"), candidateFiles);

	TArray<FString> resultDirectories;
	for (const FString& candidateFile : candidateFiles)
	{
		FString schema;
		if (!TryReadJsonSchema(candidateFile, schema))
		{
			continue;
		}

		if (!schema.Equals(TEXT("simulation_run_status"), ESearchCase::CaseSensitive))
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

bool USimulatorLaunchSubsystem::PrepareProjectRunSnapshot(
	const FString& projectPath,
	const FString& requestedRunId,
	FString& outRunId,
	TArray<FString>& outDiagnostics) const
{
	outRunId.Reset();
	outDiagnostics.Reset();

	FString normalizedProjectPath = projectPath.TrimStartAndEnd();
	normalizedProjectPath.ReplaceInline(TEXT("\\"), TEXT("/"));
	if (normalizedProjectPath.IsEmpty())
	{
		AddLaunchDiagnostic(outDiagnostics, TEXT("Project path must not be empty."));
		return false;
	}

	if (FPaths::IsRelative(normalizedProjectPath))
	{
		normalizedProjectPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), normalizedProjectPath));
		normalizedProjectPath.ReplaceInline(TEXT("\\"), TEXT("/"));
	}

	if (!FPaths::DirectoryExists(normalizedProjectPath))
	{
		AddLaunchDiagnostic(
			outDiagnostics,
			FString::Printf(TEXT("Project directory is required: %s"), *normalizedProjectPath));
		return false;
	}

	const FString runId = requestedRunId.TrimStartAndEnd().IsEmpty()
		? MakeNextProjectRunId(normalizedProjectPath)
		: requestedRunId.TrimStartAndEnd();
	if (runId.IsEmpty())
	{
		AddLaunchDiagnostic(outDiagnostics, TEXT("No available 6-digit project run id."));
		return false;
	}
	if (!FUserProjectRunSnapshot::IsValidRunId(runId))
	{
		AddLaunchDiagnostic(outDiagnostics, TEXT("Project run id must be a 6-digit decimal string."));
		return false;
	}

	const FUserProjectRunSnapshotPaths paths = FUserProjectRunSnapshot::BuildPaths(normalizedProjectPath, runId);
	if (FPaths::DirectoryExists(paths.RunPath))
	{
		AddLaunchDiagnostic(
			outDiagnostics,
			FString::Printf(TEXT("Project run directory already exists: %s"), *paths.RunPath));
		return false;
	}

	if (!IFileManager::Get().MakeDirectory(*paths.ReviewPath, true)
		|| !IFileManager::Get().MakeDirectory(*paths.EpisodesPath, true)
		|| !IFileManager::Get().MakeDirectory(*paths.SnapshotPath, true))
	{
		AddLaunchDiagnostic(
			outDiagnostics,
			FString::Printf(TEXT("Project run directories could not be created: %s"), *paths.RunPath));
		return false;
	}

	if (!CopyProjectRunFileSnapshot(
			FPaths::Combine(paths.ProjectPath, TEXT("setting.json")),
			paths.SettingPath,
			TEXT("setting"),
			outDiagnostics)
		|| !CopyProjectRunFileSnapshot(
			FPaths::Combine(paths.ProjectPath, TEXT("profile.json")),
			paths.ProfilePath,
			TEXT("profile"),
			outDiagnostics)
		|| !CopyProjectRunFileSnapshot(
			FPaths::Combine(paths.ProjectPath, TEXT("scenario.json")),
			paths.ScenarioPath,
			TEXT("scenario"),
			outDiagnostics)
		|| !CopyProjectRunPolicySnapshot(
			FPaths::Combine(paths.ProjectPath, TEXT("policy")),
			paths.PolicyPath,
			outDiagnostics))
	{
		return false;
	}

	const FUserProjectRunSnapshotParseResult parseResult = FUserProjectRunSnapshot::Parse(paths.ProjectPath, paths.RunId);
	for (const FScenarioCompileDiagnostic& diagnostic : parseResult.Diagnostics)
	{
		AddLaunchDiagnostic(outDiagnostics, FString::Printf(TEXT("%s: %s"), *diagnostic.Code, *diagnostic.Message));
	}
	if (!parseResult.bSuccess)
	{
		return false;
	}

	outRunId = paths.RunId;
	return true;
}

bool USimulatorLaunchSubsystem::StartProjectRun(const FString& projectPath, const FString& runId)
{
	if (ActiveProcessHandle.IsValid() && FPlatformProcess::IsProcRunning(ActiveProcessHandle))
	{
		ActiveRunInfo.LastError = TEXT("A simulator process is already running.");
		BroadcastRunInfoChanged();
		return false;
	}

	CloseActiveProcessHandle();

	const FUserProjectRunSnapshotParseResult snapshotParseResult =
		FUserProjectRunSnapshot::Parse(projectPath, runId);
	if (!snapshotParseResult.bSuccess)
	{
		ActiveRunInfo = FSimulatorRunInfo{};
		ActiveRunInfo.RunId = runId;
		ActiveRunInfo.ProjectPath = projectPath;
		ActiveRunInfo.bProjectRun = true;
		ActiveRunInfo.Status.State = ESimulationRunState::Failed;
		ActiveRunInfo.LastError = JoinDiagnostics(snapshotParseResult.Diagnostics);
		BroadcastRunInfoChanged();
		return false;
	}

	FString executable;
	FString arguments;
	bool bUsesPreviewLauncher = false;
	if (!BuildProjectRunLaunchCommand(
			snapshotParseResult.Paths.ProjectPath,
			snapshotParseResult.Paths.RunId,
			executable,
			arguments,
			bUsesPreviewLauncher))
	{
		ActiveRunInfo = FSimulatorRunInfo{};
		ActiveRunInfo.RunId = snapshotParseResult.Paths.RunId;
		ActiveRunInfo.ProjectPath = snapshotParseResult.Paths.ProjectPath;
		ActiveRunInfo.StatusPath = snapshotParseResult.Paths.StatusPath;
		ActiveRunInfo.bProjectRun = true;
		MarkActiveRunFailed(TEXT("Project run launch command could not be built."));
		return false;
	}

	ActiveRunInfo = FSimulatorRunInfo{};
	ActiveRunInfo.RunId = snapshotParseResult.Paths.RunId;
	ActiveRunInfo.ProjectPath = snapshotParseResult.Paths.ProjectPath;
	ActiveRunInfo.StatusPath = snapshotParseResult.Paths.StatusPath;
	ActiveRunInfo.LaunchExecutable = executable;
	ActiveRunInfo.LaunchArguments = arguments;
	ActiveRunInfo.bUsedPreviewLauncher = bUsesPreviewLauncher;
	ActiveRunInfo.bProjectRun = true;
	ActiveRunInfo.Status.RunId = snapshotParseResult.Paths.RunId;
	ActiveRunInfo.Status.SetupPath = snapshotParseResult.Paths.SnapshotPath;
	ActiveRunInfo.Status.State = ESimulationRunState::Pending;

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
		MarkActiveRunFailed(FString::Printf(TEXT("Project run process start failed: %s"), *executable));
		return false;
	}

	ActiveRunInfo.bProcessStarted = true;
	ActiveRunInfo.bProcessRunning = true;
	ActiveRunInfo.Status.State = ESimulationRunState::Running;
	StartPolling();
	BroadcastRunInfoChanged();

	UE_LOG(
		LogSimulatorLaunch,
		Log,
		TEXT("Project run process started | Project: %s, RunId: %s, Executable: %s, Arguments: %s"),
		*snapshotParseResult.Paths.ProjectPath,
		*snapshotParseResult.Paths.RunId,
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

	if (ActiveRunInfo.bProjectRun)
	{
		RefreshActiveProcessState();

		if (ActiveRunInfo.bProcessStarted && !ActiveRunInfo.bProcessRunning)
		{
			ActiveRunInfo.Status.State = ActiveRunInfo.ProcessReturnCode == 0
				? ESimulationRunState::Completed
				: ESimulationRunState::Failed;
			if (ActiveRunInfo.Status.State == ESimulationRunState::Failed && ActiveRunInfo.LastError.IsEmpty())
			{
				ActiveRunInfo.LastError = FString::Printf(
					TEXT("Project run process exited with code %d."),
					ActiveRunInfo.ProcessReturnCode);
			}
			StopPolling();
			CloseActiveProcessHandle();
		}
		else if (ActiveRunInfo.bProcessStarted)
		{
			ActiveRunInfo.Status.State = ESimulationRunState::Running;
		}

		BroadcastRunInfoChanged();
		return ActiveRunInfo.Status.State != ESimulationRunState::Failed;
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

FString USimulatorLaunchSubsystem::BuildProjectRunSimulatorArgumentString(const FString& projectPath, const FString& runId)
{
	return FString::Printf(
		TEXT("%s %s %s"),
		*QuoteCommandLineArgument(FString::Printf(TEXT("-OdiroProject=%s"), *projectPath)),
		*QuoteCommandLineArgument(FString::Printf(TEXT("-RunId=%s"), *runId)),
		SimulatorProcessFlags);
}

FString USimulatorLaunchSubsystem::BuildProjectRunPreviewLauncherArgumentString(
	const FString& previewBatPath,
	const FString& projectPath,
	const FString& runId)
{
	// cmd.exe quoting is intentionally centralized here; CreateProc receives cmd.exe as executable.
	return FString::Printf(
		TEXT("/d /s /c \"\"%s\" %s %s %s\""),
		*previewBatPath,
		*QuoteCommandLineArgument(FString::Printf(TEXT("-OdiroProject=%s"), *projectPath)),
		*QuoteCommandLineArgument(FString::Printf(TEXT("-RunId=%s"), *runId)),
		SimulatorProcessFlags);
}

bool USimulatorLaunchSubsystem::BuildProjectRunLaunchCommand(
	const FString& projectPath,
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
		outArguments = BuildProjectRunPreviewLauncherArgumentString(previewBatPath, projectPath, runId);
		bOutUsesPreviewLauncher = true;
		return true;
	}

	outExecutable = FPlatformProcess::ExecutablePath();
	outArguments = BuildProjectRunSimulatorArgumentString(projectPath, runId);
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
