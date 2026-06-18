
#include "Platform/SimulatorLaunchSubsystem.h"
#include "DeliveryBot/DeliveryBotSetupCompiler.h"
#include "Scenario/ScenarioCompiler.h"
#include "Scenario/ScenarioSampleWorldSpecAdapter.h"
#include "Shared/UserProjectDataTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogSimulatorLaunch, Log, All);

namespace
{
	const TCHAR* SimulationSetupInputDirectory = TEXT("Json/Input");
	const TCHAR* SimulatorLaunchPolicySpecInputDirectory = TEXT("Json/Input/PolicySpecs");
	const TCHAR* EvaluationReportOutputDirectory = TEXT("Json/Output");
	const TCHAR* SimulationRunStatusDirectory = TEXT("Saved/SimulationRuns");
	const TCHAR* PreviewLauncherFileName = TEXT("Task-RunPreview.bat");
	const TCHAR* LaunchSimulationSetupSchema = TEXT("simulation_setup");
	const TCHAR* LaunchScenarioSetupSchema = TEXT("scenario_actor_spawn_mvp");
	const TCHAR* LaunchDeliveryBotSetupSchema = TEXT("delivery_bot_setup");
	const TCHAR* LaunchScenarioRunQueueSchema = TEXT("episode_run_queue");
	const TCHAR* LaunchEvaluationReportSchema = TEXT("episode_evaluation_report");
	const TCHAR* LaunchDefaultPolicySpecJsonPath = TEXT("Json/Input/PolicySpecs/PolicySpec_DefaultDelivery.json");
	const TCHAR* SimulatorProcessFlags = TEXT("-nosound -unattended -NoLoadingScreen");
	const int32 ProjectRunDefaultPolicyPort = 18145;
	const TCHAR* UserProjectStaticDirectory = TEXT("static");
	const TCHAR* UserProjectResourcesDirectory = TEXT("resources");
	const TCHAR* UserProjectTemplateDirectory = TEXT("project-templates");
	const TCHAR* UserProjectRunDefaultsDirectory = TEXT("run-defaults");
	const TCHAR* UserProjectSettingFileName = TEXT("setting.json");
	const TCHAR* UserProjectProfileFileName = TEXT("profile.json");
	const TCHAR* UserProjectScenarioFileName = TEXT("scenario.json");
	const TCHAR* UserProjectPolicyDirectory = TEXT("policy");
	const TCHAR* UserProjectRunsDirectory = TEXT("runs");
	const TCHAR* UserProjectSnapshotDirectory = TEXT("snapshot");
	const TCHAR* UserProjectPolicyEntrypointFileName = TEXT("__init__.py");
	const TCHAR* UserProjectSettingSchema = TEXT("project_setting");
	const TCHAR* UserProjectProfileSchema = TEXT("simulation_profile");
	const TCHAR* UserProjectScenarioSchema = TEXT("scenario");

	FString ToProjectRelativePath(FString filePath)
	{
		// UI에는 machine-specific absolute path 대신 packaged args와 같은 project-relative path를 노출한다.
		const FString projectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		FPaths::MakePathRelativeTo(filePath, *projectDir);
		return filePath.Replace(TEXT("\\"), TEXT("/"));
	}

	FString NormalizeAbsolutePath(FString path)
	{
		path = path.TrimStartAndEnd();
		if (path.IsEmpty())
		{
			return FString();
		}

		path = FPaths::IsRelative(path)
			? FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), path))
			: FPaths::ConvertRelativePathToFull(path);
		FPaths::NormalizeFilename(path);
		return path;
	}

	bool IsDirectory(const FString& path)
	{
		return !path.IsEmpty() && IFileManager::Get().DirectoryExists(*path);
	}

	bool IsFile(const FString& path)
	{
		return !path.IsEmpty() && FPaths::FileExists(path);
	}

	bool IsSafeUserProjectSegment(const FString& value)
	{
		if (value.IsEmpty() || value.Equals(TEXT(".")) || value.Equals(TEXT("..")) || FPaths::IsRelative(value) == false)
		{
			return false;
		}
		if (value.Contains(TEXT("/")) || value.Contains(TEXT("\\")))
		{
			return false;
		}
		if (!FChar::IsAlnum(value[0]))
		{
			return false;
		}

		for (int32 characterIndex = 0; characterIndex < value.Len(); ++characterIndex)
		{
			const TCHAR character = value[characterIndex];
			if (!FChar::IsAlnum(character) && character != TEXT('.') && character != TEXT('_') && character != TEXT('-'))
			{
				return false;
			}
		}
		return true;
	}

	bool TryFindUserProjectResourcesFromRoot(
		const FString& root,
		FString& outProjectTemplatesPath,
		FString& outRunDefaultsPath)
	{
		const FString staticTemplatesPath =
			NormalizeAbsolutePath(FPaths::Combine(root, UserProjectStaticDirectory, UserProjectTemplateDirectory));
		const FString staticRunDefaultsPath =
			NormalizeAbsolutePath(FPaths::Combine(root, UserProjectStaticDirectory, UserProjectRunDefaultsDirectory));
		if (IsDirectory(staticTemplatesPath) && IsDirectory(staticRunDefaultsPath))
		{
			outProjectTemplatesPath = staticTemplatesPath;
			outRunDefaultsPath = staticRunDefaultsPath;
			return true;
		}

		const FString resourceTemplatesPath =
			NormalizeAbsolutePath(FPaths::Combine(root, UserProjectResourcesDirectory, UserProjectTemplateDirectory));
		const FString resourceRunDefaultsPath =
			NormalizeAbsolutePath(FPaths::Combine(root, UserProjectResourcesDirectory, UserProjectRunDefaultsDirectory));
		if (IsDirectory(resourceTemplatesPath) && IsDirectory(resourceRunDefaultsPath))
		{
			outProjectTemplatesPath = resourceTemplatesPath;
			outRunDefaultsPath = resourceRunDefaultsPath;
			return true;
		}

		return false;
	}

	bool SearchUserProjectResourceRoot(
		FString startPath,
		FString& outProjectTemplatesPath,
		FString& outRunDefaultsPath)
	{
		FString current = NormalizeAbsolutePath(startPath);
		if (IsFile(current))
		{
			current = FPaths::GetPath(current);
		}

		while (!current.IsEmpty())
		{
			if (TryFindUserProjectResourcesFromRoot(current, outProjectTemplatesPath, outRunDefaultsPath))
			{
				return true;
			}

			const FString parent = FPaths::GetPath(current);
			if (parent.IsEmpty() || parent.Equals(current, ESearchCase::IgnoreCase))
			{
				break;
			}
			current = parent;
		}

		return false;
	}

	bool LocateUserProjectResourceDirectories(
		FString& outProjectTemplatesPath,
		FString& outRunDefaultsPath,
		TArray<FString>* outDiagnostics)
	{
		TArray<FString> startPaths;
		startPaths.Add(FPaths::ProjectDir());
		startPaths.Add(FPlatformProcess::BaseDir());
		startPaths.Add(FPlatformProcess::ExecutablePath());

		for (const FString& startPath : startPaths)
		{
			if (SearchUserProjectResourceRoot(startPath, outProjectTemplatesPath, outRunDefaultsPath))
			{
				return true;
			}
		}

		if (outDiagnostics)
		{
			outDiagnostics->Add(TEXT("Project template resources not found. Expected static/project-templates or resources/project-templates."));
		}
		return false;
	}

	void AddUserProjectJsonDiagnostics(const FString& label, const TArray<FScenarioCompileDiagnostic>& diagnostics, TArray<FString>& outDiagnostics)
	{
		for (const FScenarioCompileDiagnostic& diagnostic : diagnostics)
		{
			outDiagnostics.Add(FString::Printf(
				TEXT("%s: %s: %s"),
				*label,
				*diagnostic.Code,
				*diagnostic.Message));
		}
	}

	bool ValidateUserProjectRootJsonFile(
		const FString& jsonPath,
		const FString& expectedSchema,
		const FString& label,
		TArray<FString>& outDiagnostics)
	{
		if (!IsFile(jsonPath))
		{
			outDiagnostics.Add(FString::Printf(TEXT("%s missing: %s"), *label, *jsonPath));
			return false;
		}

		const FUserProjectJsonParseResult parseResult =
			FUserProjectDataJson::ValidateRootJsonFile(jsonPath, expectedSchema);
		if (!parseResult.bSuccess)
		{
			AddUserProjectJsonDiagnostics(label, parseResult.Diagnostics, outDiagnostics);
			return false;
		}

		return true;
	}

	bool ValidateUserProjectPolicyDirectory(const FString& policyPath, TArray<FString>& outDiagnostics)
	{
		if (!IsDirectory(policyPath))
		{
			outDiagnostics.Add(FString::Printf(TEXT("policy directory missing: %s"), *policyPath));
			return false;
		}

		const FString entrypointPath = NormalizeAbsolutePath(FPaths::Combine(policyPath, UserProjectPolicyEntrypointFileName));
		if (!IsFile(entrypointPath))
		{
			outDiagnostics.Add(FString::Printf(TEXT("policy entrypoint missing: %s"), *entrypointPath));
			return false;
		}

		FString entrypointSource;
		if (!FFileHelper::LoadFileToString(entrypointSource, *entrypointPath))
		{
			outDiagnostics.Add(FString::Printf(TEXT("policy entrypoint read failed: %s"), *entrypointPath));
			return false;
		}
		if (!entrypointSource.Contains(TEXT("create_policy")))
		{
			outDiagnostics.Add(TEXT("policy/__init__.py must expose create_policy."));
			return false;
		}

		return true;
	}

	bool ValidateUserProjectDirectory(const FString& projectPath, const bool bRequireRunsDirectory, TArray<FString>& outDiagnostics)
	{
		const FString resolvedProjectPath = NormalizeAbsolutePath(projectPath);
		if (!IsDirectory(resolvedProjectPath))
		{
			outDiagnostics.Add(FString::Printf(TEXT("project directory missing: %s"), *resolvedProjectPath));
			return false;
		}

		bool bValid = true;
		bValid &= ValidateUserProjectRootJsonFile(
			NormalizeAbsolutePath(FPaths::Combine(resolvedProjectPath, UserProjectSettingFileName)),
			UserProjectSettingSchema,
			TEXT("setting.json"),
			outDiagnostics);
		bValid &= ValidateUserProjectRootJsonFile(
			NormalizeAbsolutePath(FPaths::Combine(resolvedProjectPath, UserProjectProfileFileName)),
			UserProjectProfileSchema,
			TEXT("profile.json"),
			outDiagnostics);
		bValid &= ValidateUserProjectRootJsonFile(
			NormalizeAbsolutePath(FPaths::Combine(resolvedProjectPath, UserProjectScenarioFileName)),
			UserProjectScenarioSchema,
			TEXT("scenario.json"),
			outDiagnostics);
		bValid &= ValidateUserProjectPolicyDirectory(
			NormalizeAbsolutePath(FPaths::Combine(resolvedProjectPath, UserProjectPolicyDirectory)),
			outDiagnostics);

		if (bRequireRunsDirectory)
		{
			const FString runsPath = NormalizeAbsolutePath(FPaths::Combine(resolvedProjectPath, UserProjectRunsDirectory));
			if (!IsDirectory(runsPath))
			{
				outDiagnostics.Add(FString::Printf(TEXT("runs directory missing: %s"), *runsPath));
				bValid = false;
			}
		}

		return bValid;
	}

	bool IsGeneratedPythonCachePath(const FString& path)
	{
		const FString normalizedPath = path.Replace(TEXT("\\"), TEXT("/"));
		const FString cleanName = FPaths::GetCleanFilename(normalizedPath);
		return normalizedPath.Contains(TEXT("/__pycache__/"))
			|| cleanName.Equals(TEXT("__pycache__"), ESearchCase::CaseSensitive)
			|| cleanName.EndsWith(TEXT(".pyc"), ESearchCase::IgnoreCase)
			|| cleanName.EndsWith(TEXT(".pyo"), ESearchCase::IgnoreCase);
	}

	bool MakeRelativeToDirectory(const FString& path, const FString& root, FString& outRelativePath)
	{
		outRelativePath = path;
		FString normalizedRoot = root;
		if (!normalizedRoot.EndsWith(TEXT("/")))
		{
			normalizedRoot += TEXT("/");
		}
		const bool bRelative = FPaths::MakePathRelativeTo(outRelativePath, *normalizedRoot);
		outRelativePath.ReplaceInline(TEXT("\\"), TEXT("/"));
		return bRelative && !outRelativePath.StartsWith(TEXT(".."));
	}

	bool CopyUserProjectFile(const FString& sourcePath, const FString& targetPath, TArray<FString>& outDiagnostics)
	{
		TArray<uint8> payload;
		if (!FFileHelper::LoadFileToArray(payload, *sourcePath))
		{
			outDiagnostics.Add(FString::Printf(TEXT("file read failed: %s"), *sourcePath));
			return false;
		}

		const FString targetDirectory = FPaths::GetPath(targetPath);
		if (!IFileManager::Get().MakeDirectory(*targetDirectory, true))
		{
			outDiagnostics.Add(FString::Printf(TEXT("directory create failed: %s"), *targetDirectory));
			return false;
		}

		if (!FFileHelper::SaveArrayToFile(payload, *targetPath))
		{
			outDiagnostics.Add(FString::Printf(TEXT("file write failed: %s"), *targetPath));
			return false;
		}

		return true;
	}

	bool CopyUserProjectTree(
		const FString& sourceRoot,
		const FString& targetRoot,
		const bool bSkipGitKeep,
		TArray<FString>& outDiagnostics)
	{
		const FString normalizedSourceRoot = NormalizeAbsolutePath(sourceRoot);
		const FString normalizedTargetRoot = NormalizeAbsolutePath(targetRoot);
		if (!IsDirectory(normalizedSourceRoot))
		{
			outDiagnostics.Add(FString::Printf(TEXT("source directory missing: %s"), *normalizedSourceRoot));
			return false;
		}

		if (!IFileManager::Get().MakeDirectory(*normalizedTargetRoot, true))
		{
			outDiagnostics.Add(FString::Printf(TEXT("target directory create failed: %s"), *normalizedTargetRoot));
			return false;
		}

		TArray<FString> directories;
		IFileManager::Get().FindFilesRecursive(directories, *normalizedSourceRoot, TEXT("*"), false, true);
		directories.Sort();
		for (const FString& directory : directories)
		{
			if (IsGeneratedPythonCachePath(directory))
			{
				continue;
			}

			FString relativePath;
			if (!MakeRelativeToDirectory(NormalizeAbsolutePath(directory), normalizedSourceRoot, relativePath))
			{
				outDiagnostics.Add(FString::Printf(TEXT("relative directory resolve failed: %s"), *directory));
				return false;
			}

			const FString targetDirectory = NormalizeAbsolutePath(FPaths::Combine(normalizedTargetRoot, relativePath));
			if (!IFileManager::Get().MakeDirectory(*targetDirectory, true))
			{
				outDiagnostics.Add(FString::Printf(TEXT("directory create failed: %s"), *targetDirectory));
				return false;
			}
		}

		TArray<FString> files;
		IFileManager::Get().FindFilesRecursive(files, *normalizedSourceRoot, TEXT("*"), true, false);
		files.Sort();
		for (const FString& file : files)
		{
			const FString cleanName = FPaths::GetCleanFilename(file);
			if (IsGeneratedPythonCachePath(file) || (bSkipGitKeep && cleanName.Equals(TEXT(".gitkeep"), ESearchCase::CaseSensitive)))
			{
				continue;
			}

			FString relativePath;
			if (!MakeRelativeToDirectory(NormalizeAbsolutePath(file), normalizedSourceRoot, relativePath))
			{
				outDiagnostics.Add(FString::Printf(TEXT("relative file resolve failed: %s"), *file));
				return false;
			}

			const FString targetPath = NormalizeAbsolutePath(FPaths::Combine(normalizedTargetRoot, relativePath));
			if (!CopyUserProjectFile(NormalizeAbsolutePath(file), targetPath, outDiagnostics))
			{
				return false;
			}
		}

		return true;
	}

	bool IsDirectoryEmptyOrMissing(const FString& directoryPath, TArray<FString>& outDiagnostics)
	{
		const FString resolvedDirectoryPath = NormalizeAbsolutePath(directoryPath);
		if (resolvedDirectoryPath.IsEmpty())
		{
			outDiagnostics.Add(TEXT("project path must not be empty."));
			return false;
		}

		if (FPaths::FileExists(resolvedDirectoryPath) && !IsDirectory(resolvedDirectoryPath))
		{
			outDiagnostics.Add(FString::Printf(TEXT("target project path is not a directory: %s"), *resolvedDirectoryPath));
			return false;
		}

		if (!IsDirectory(resolvedDirectoryPath))
		{
			return true;
		}

		TArray<FString> children;
		IFileManager::Get().FindFiles(children, *FPaths::Combine(resolvedDirectoryPath, TEXT("*")), true, true);
		if (!children.IsEmpty())
		{
			outDiagnostics.Add(FString::Printf(TEXT("target project directory is not empty: %s"), *resolvedDirectoryPath));
			return false;
		}

		return true;
	}

	bool FindNextUserProjectRunId(const FString& runsPath, FString& outRunId, TArray<FString>& outDiagnostics)
	{
		outRunId.Reset();
		if (!IsDirectory(runsPath))
		{
			outDiagnostics.Add(FString::Printf(TEXT("runs directory missing: %s"), *runsPath));
			return false;
		}

		TArray<FString> runDirectoryNames;
		IFileManager::Get().FindFiles(runDirectoryNames, *FPaths::Combine(runsPath, TEXT("*")), false, true);

		int32 maxRunId = 0;
		for (const FString& runDirectoryName : runDirectoryNames)
		{
			if (!FUserProjectRunSnapshot::IsValidRunId(runDirectoryName))
			{
				continue;
			}

			maxRunId = FMath::Max(maxRunId, FCString::Atoi(*runDirectoryName));
		}

		if (maxRunId >= 999999)
		{
			outDiagnostics.Add(TEXT("run id overflow."));
			return false;
		}

		outRunId = FString::Printf(TEXT("%06d"), maxRunId + 1);
		return true;
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

	FString NormalizeRunQueueReferencePath(FString jsonPath)
	{
		jsonPath = jsonPath.TrimStartAndEnd();
		jsonPath.ReplaceInline(TEXT("\\"), TEXT("/"));
		return jsonPath;
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

	TSharedRef<FJsonObject> MakeScenarioRunQueueObject(const TArray<FScenarioRunInput>& runInputs)
	{
		TSharedRef<FJsonObject> rootObject = MakeShared<FJsonObject>();
		rootObject->SetStringField(TEXT("schema"), LaunchScenarioRunQueueSchema);
		rootObject->SetNumberField(TEXT("version"), 1);

		TArray<TSharedPtr<FJsonValue>> runValues;
		runValues.Reserve(runInputs.Num());
		for (const FScenarioRunInput& runInput : runInputs)
		{
			TSharedRef<FJsonObject> runObject = MakeShared<FJsonObject>();
			if (!runInput.PairId.IsEmpty())
			{
				runObject->SetStringField(TEXT("pair_id"), runInput.PairId);
			}
			runObject->SetStringField(TEXT("scenario_setup"), runInput.ScenarioSetupJsonPath);
			runObject->SetStringField(TEXT("delivery_bot_setup"), runInput.DeliveryBotSetupJsonPath);
			if (!runInput.PolicySpecJsonPath.IsEmpty())
			{
				runObject->SetStringField(TEXT("policy_spec"), runInput.PolicySpecJsonPath);
			}
			runValues.Add(MakeShared<FJsonValueObject>(runObject));
		}

		rootObject->SetArrayField(TEXT("runs"), runValues);
		return rootObject;
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
		if (IsReferenceSampleJsonFile(jsonFile))
		{
			continue;
		}

		if (HasJsonSchema(jsonFile, LaunchSimulationSetupSchema) && FSimulationSetupJson::ParseFromFile(jsonFile).bSuccess)
		{
			setupFiles.Add(jsonFile);
		}
	}

	return setupFiles;
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

TArray<FString> USimulatorLaunchSubsystem::ListScenarioRunQueueFiles() const
{
	TArray<FString> jsonFiles;
	FindProjectJsonFiles(SimulationSetupInputDirectory, jsonFiles);

	TArray<FString> runQueueFiles;
	for (const FString& jsonFile : jsonFiles)
	{
		if (IsReferenceSampleJsonFile(jsonFile))
		{
			continue;
		}

		if (!HasJsonSchema(jsonFile, LaunchScenarioRunQueueSchema))
		{
			continue;
		}

		FString jsonString;
		if (!FFileHelper::LoadFileToString(jsonString, *FSimulationSetupJson::ResolveProjectPath(jsonFile)))
		{
			continue;
		}

		TArray<FScenarioRunInput> runInputs;
		TArray<FString> diagnostics;
		if (TryReadScenarioRunQueueJson(jsonString, runInputs, diagnostics))
		{
			runQueueFiles.Add(jsonFile);
		}
	}

	return runQueueFiles;
}

TArray<FString> USimulatorLaunchSubsystem::ListEvaluationReportFiles() const
{
	TArray<FString> reportFiles;
	FindJsonFilesWithSchema(EvaluationReportOutputDirectory, LaunchEvaluationReportSchema, reportFiles);
	FindJsonFilesWithSchema(SimulationRunStatusDirectory, LaunchEvaluationReportSchema, reportFiles);
	reportFiles.Sort();
	return reportFiles;
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

		if (!schema.Equals(TEXT("simulation_run_status"), ESearchCase::CaseSensitive)
			&& !schema.Equals(LaunchEvaluationReportSchema, ESearchCase::CaseSensitive))
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

TArray<FString> USimulatorLaunchSubsystem::ListEvaluationReportFilesInDirectory(const FString& runDirectory) const
{
	TArray<FString> candidateFiles;
	FindProjectFiles(runDirectory, TEXT("*.json"), candidateFiles);

	TArray<FString> reportFiles;
	for (const FString& candidateFile : candidateFiles)
	{
		if (HasJsonSchema(candidateFile, LaunchEvaluationReportSchema))
		{
			reportFiles.Add(candidateFile);
		}
	}

	reportFiles.Sort();
	return reportFiles;
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

bool USimulatorLaunchSubsystem::SaveSimulationSetupFile(
	const FString& setupPath,
	const FSimulationSetup& setup,
	TArray<FString>& outDiagnostics) const
{
	if (!FSimulationSetupJson::SaveToFile(setup, setupPath, outDiagnostics))
	{
		return false;
	}

	const FSimulationSetupParseResult parseResult = FSimulationSetupJson::ParseFromFile(setupPath);
	for (const FScenarioCompileDiagnostic& diagnostic : parseResult.Diagnostics)
	{
		outDiagnostics.Add(FString::Printf(TEXT("%s: %s"), *diagnostic.Code, *diagnostic.Message));
	}
	return parseResult.bSuccess;
}

bool USimulatorLaunchSubsystem::LoadScenarioRunQueueFile(
	const FString& runQueuePath,
	TArray<FScenarioRunInput>& outRunInputs,
	TArray<FString>& outDiagnostics) const
{
	outRunInputs.Reset();
	outDiagnostics.Reset();
	if (runQueuePath.TrimStartAndEnd().IsEmpty())
	{
		outDiagnostics.Add(TEXT("ScenarioRunQueue path must not be empty."));
		return false;
	}

	FString jsonString;
	const FString resolvedRunQueuePath = FSimulationSetupJson::ResolveProjectPath(runQueuePath);
	if (!FFileHelper::LoadFileToString(jsonString, *resolvedRunQueuePath))
	{
		outDiagnostics.Add(FString::Printf(TEXT("ScenarioRunQueue read failed: %s"), *resolvedRunQueuePath));
		return false;
	}

	return TryReadScenarioRunQueueJson(jsonString, outRunInputs, outDiagnostics);
}

bool USimulatorLaunchSubsystem::SaveScenarioRunQueueFile(
	const FString& runQueuePath,
	const TArray<FScenarioRunInput>& runInputs,
	TArray<FString>& outDiagnostics) const
{
	outDiagnostics.Reset();
	if (runQueuePath.TrimStartAndEnd().IsEmpty())
	{
		outDiagnostics.Add(TEXT("ScenarioRunQueue path must not be empty."));
		return false;
	}

	FString jsonString;
	if (!TryWriteScenarioRunQueueJson(runInputs, jsonString, outDiagnostics))
	{
		return false;
	}

	const FString resolvedRunQueuePath = FSimulationSetupJson::ResolveProjectPath(runQueuePath);
	const FString outputDirectory = FPaths::GetPath(resolvedRunQueuePath);
	if (!IFileManager::Get().MakeDirectory(*outputDirectory, true))
	{
		outDiagnostics.Add(FString::Printf(TEXT("ScenarioRunQueue directory create failed: %s"), *outputDirectory));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(
			jsonString,
			*resolvedRunQueuePath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		outDiagnostics.Add(FString::Printf(TEXT("ScenarioRunQueue write failed: %s"), *resolvedRunQueuePath));
		return false;
	}

	return true;
}

bool USimulatorLaunchSubsystem::AppendRunQueuePair(
	const FString& runQueuePath,
	const FString& pairId,
	const FString& scenarioSetupPath,
	const FString& deliveryBotSetupPath,
	TArray<FString>& outDiagnostics) const
{
	outDiagnostics.Reset();

	TArray<FScenarioRunInput> runInputs;
	const FString resolvedRunQueuePath = FSimulationSetupJson::ResolveProjectPath(runQueuePath);
	if (FPaths::FileExists(resolvedRunQueuePath))
	{
		if (!LoadScenarioRunQueueFile(runQueuePath, runInputs, outDiagnostics))
		{
			return false;
		}
	}

	FScenarioRunInput runInput;
	runInput.PairId = pairId.TrimStartAndEnd();
	runInput.ScenarioSetupJsonPath = scenarioSetupPath.TrimStartAndEnd();
	runInput.DeliveryBotSetupJsonPath = deliveryBotSetupPath.TrimStartAndEnd();
	runInput.PolicySpecJsonPath = LaunchDefaultPolicySpecJsonPath;
	runInputs.Add(runInput);
	return SaveScenarioRunQueueFile(runQueuePath, runInputs, outDiagnostics);
}

bool USimulatorLaunchSubsystem::RemoveRunQueuePair(
	const FString& runQueuePath,
	int32 runIndex,
	TArray<FString>& outDiagnostics) const
{
	TArray<FScenarioRunInput> runInputs;
	if (!LoadScenarioRunQueueFile(runQueuePath, runInputs, outDiagnostics))
	{
		return false;
	}

	if (!runInputs.IsValidIndex(runIndex))
	{
		outDiagnostics.Add(FString::Printf(TEXT("ScenarioRunQueue index is out of range: %d"), runIndex));
		return false;
	}

	runInputs.RemoveAt(runIndex);
	return SaveScenarioRunQueueFile(runQueuePath, runInputs, outDiagnostics);
}

bool USimulatorLaunchSubsystem::MoveRunQueuePair(
	const FString& runQueuePath,
	int32 runIndex,
	int32 direction,
	TArray<FString>& outDiagnostics) const
{
	TArray<FScenarioRunInput> runInputs;
	if (!LoadScenarioRunQueueFile(runQueuePath, runInputs, outDiagnostics))
	{
		return false;
	}

	if (!runInputs.IsValidIndex(runIndex))
	{
		outDiagnostics.Add(FString::Printf(TEXT("ScenarioRunQueue index is out of range: %d"), runIndex));
		return false;
	}

	const int32 step = direction < 0 ? -1 : 1;
	const int32 targetIndex = runIndex + step;
	if (!runInputs.IsValidIndex(targetIndex))
	{
		outDiagnostics.Add(FString::Printf(TEXT("ScenarioRunQueue target index is out of range: %d"), targetIndex));
		return false;
	}

	runInputs.Swap(runIndex, targetIndex);
	return SaveScenarioRunQueueFile(runQueuePath, runInputs, outDiagnostics);
}

bool USimulatorLaunchSubsystem::ReplaceScenarioSetupReferencesInRunQueues(
	const FString& oldScenarioSetupPath,
	const FString& newScenarioSetupPath,
	TArray<FString>& outDiagnostics) const
{
	return ReplaceRunQueueReferences(oldScenarioSetupPath, newScenarioSetupPath, true, outDiagnostics);
}

bool USimulatorLaunchSubsystem::ReplaceDeliveryBotSetupReferencesInRunQueues(
	const FString& oldDeliveryBotSetupPath,
	const FString& newDeliveryBotSetupPath,
	TArray<FString>& outDiagnostics) const
{
	return ReplaceRunQueueReferences(oldDeliveryBotSetupPath, newDeliveryBotSetupPath, false, outDiagnostics);
}

bool USimulatorLaunchSubsystem::ReplaceRunQueueReferences(
	const FString& oldPath,
	const FString& newPath,
	const bool bReplaceScenarioSetupReference,
	TArray<FString>& outDiagnostics) const
{
	outDiagnostics.Reset();

	const FString normalizedOldPath = NormalizeRunQueueReferencePath(oldPath);
	const FString normalizedNewPath = NormalizeRunQueueReferencePath(newPath);
	const TCHAR* referenceFieldName = bReplaceScenarioSetupReference
		? TEXT("scenario_setup")
		: TEXT("delivery_bot_setup");

	if (normalizedOldPath.IsEmpty() || normalizedNewPath.IsEmpty())
	{
		outDiagnostics.Add(FString::Printf(TEXT("ScenarioRunQueue %s reference paths must not be empty."), referenceFieldName));
		return false;
	}

	if (normalizedOldPath.Equals(normalizedNewPath, ESearchCase::IgnoreCase))
	{
		return true;
	}

	TArray<FString> changedRunQueueFiles;
	TArray<FString> changedRunQueueJsonStrings;
	int32 changedReferenceCount = 0;

	for (const FString& runQueuePath : ListScenarioRunQueueFiles())
	{
		TArray<FScenarioRunInput> runInputs;
		TArray<FString> loadDiagnostics;
		if (!LoadScenarioRunQueueFile(runQueuePath, runInputs, loadDiagnostics))
		{
			outDiagnostics.Add(FString::Printf(TEXT("ScenarioRunQueue load failed before reference update: %s"), *runQueuePath));
			outDiagnostics.Append(loadDiagnostics);
			return false;
		}

		int32 runQueueChangedReferenceCount = 0;
		for (FScenarioRunInput& runInput : runInputs)
		{
			FString& referencePath = bReplaceScenarioSetupReference
				? runInput.ScenarioSetupJsonPath
				: runInput.DeliveryBotSetupJsonPath;
			if (NormalizeRunQueueReferencePath(referencePath).Equals(normalizedOldPath, ESearchCase::IgnoreCase))
			{
				referencePath = normalizedNewPath;
				++runQueueChangedReferenceCount;
			}
		}

		if (runQueueChangedReferenceCount <= 0)
		{
			continue;
		}

		FString jsonString;
		TArray<FString> writeDiagnostics;
		if (!TryWriteScenarioRunQueueJson(runInputs, jsonString, writeDiagnostics))
		{
			outDiagnostics.Add(FString::Printf(TEXT("ScenarioRunQueue reference update validation failed: %s"), *runQueuePath));
			outDiagnostics.Append(writeDiagnostics);
			return false;
		}

		changedRunQueueFiles.Add(runQueuePath);
		changedRunQueueJsonStrings.Add(jsonString);
		changedReferenceCount += runQueueChangedReferenceCount;
	}

	for (int32 index = 0; index < changedRunQueueFiles.Num(); ++index)
	{
		const FString& runQueuePath = changedRunQueueFiles[index];
		const FString resolvedRunQueuePath = FSimulationSetupJson::ResolveProjectPath(runQueuePath);
		if (!FFileHelper::SaveStringToFile(
				changedRunQueueJsonStrings[index],
				*resolvedRunQueuePath,
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			outDiagnostics.Add(FString::Printf(TEXT("ScenarioRunQueue reference update write failed: %s"), *resolvedRunQueuePath));
			return false;
		}
	}

	if (changedReferenceCount > 0)
	{
		outDiagnostics.Add(FString::Printf(
			TEXT("Updated %d %s reference(s) in %d ScenarioRunQueue file(s)."),
			changedReferenceCount,
			referenceFieldName,
			changedRunQueueFiles.Num()));
		UE_LOG(
			LogSimulatorLaunch,
			Log,
			TEXT("ScenarioRunQueue references updated | Field: %s, Old: %s, New: %s, References: %d, Files: %d"),
			referenceFieldName,
			*normalizedOldPath,
			*normalizedNewPath,
			changedReferenceCount,
			changedRunQueueFiles.Num());
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
	FString runtimeSetupPath;
	FSimulationSetup runtimeSetup;
	TArray<FString> runtimeSetupDiagnostics;
	if (!CreateRuntimeSimulationSetupFile(
			setupParseResult.Setup,
			runId,
			runtimeSetupPath,
			runtimeSetup,
			runtimeSetupDiagnostics))
	{
		ActiveRunInfo = FSimulatorRunInfo{};
		ActiveRunInfo.RunId = runId;
		ActiveRunInfo.SetupPath = setupPath;
		ActiveRunInfo.StatusPath = runtimeSetup.Status.OutputPath;
		ActiveRunInfo.Diagnostics = runtimeSetupDiagnostics;
		MarkActiveRunFailed(JoinStringDiagnostics(runtimeSetupDiagnostics));
		return false;
	}

	FString executable;
	FString arguments;
	bool bUsesPreviewLauncher = false;
	if (!BuildLaunchCommand(runtimeSetupPath, runId, executable, arguments, bUsesPreviewLauncher))
	{
		ActiveRunInfo = FSimulatorRunInfo{};
		ActiveRunInfo.RunId = runId;
		ActiveRunInfo.SetupPath = setupPath;
		ActiveRunInfo.StatusPath = runtimeSetup.Status.OutputPath;
		MarkActiveRunFailed(TEXT("Simulator launch command could not be built."));
		return false;
	}

	ActiveRunInfo = FSimulatorRunInfo{};
	ActiveRunInfo.RunId = runId;
	ActiveRunInfo.SetupPath = setupPath;
	ActiveRunInfo.StatusPath = runtimeSetup.Status.OutputPath;
	ActiveRunInfo.LaunchExecutable = executable;
	ActiveRunInfo.LaunchArguments = arguments;
	ActiveRunInfo.bUsedPreviewLauncher = bUsesPreviewLauncher;
	ActiveRunInfo.Status.RunId = runId;
	ActiveRunInfo.Status.SetupPath = runtimeSetupPath;
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

TArray<FString> USimulatorLaunchSubsystem::ListProjectTemplates() const
{
	TArray<FString> diagnostics;
	FString projectTemplatesPath;
	FString runDefaultsPath;
	if (!LocateUserProjectResourceDirectories(projectTemplatesPath, runDefaultsPath, &diagnostics))
	{
		return {};
	}

	TArray<FString> templateIds;
	IFileManager::Get().FindFiles(templateIds, *FPaths::Combine(projectTemplatesPath, TEXT("*")), false, true);

	TArray<FString> safeTemplateIds;
	for (const FString& templateId : templateIds)
	{
		if (IsSafeUserProjectSegment(templateId))
		{
			safeTemplateIds.Add(templateId);
		}
	}
	safeTemplateIds.Sort();
	return safeTemplateIds;
}

bool USimulatorLaunchSubsystem::ValidateUserProject(const FString& projectPath, TArray<FString>& outDiagnostics) const
{
	outDiagnostics.Reset();
	return ValidateUserProjectDirectory(projectPath, true, outDiagnostics);
}

bool USimulatorLaunchSubsystem::CreateProjectFromTemplate(
	const FString& projectPath,
	const FString& templateId,
	TArray<FString>& outDiagnostics) const
{
	outDiagnostics.Reset();

	const FString trimmedTemplateId = templateId.TrimStartAndEnd();
	if (!IsSafeUserProjectSegment(trimmedTemplateId))
	{
		outDiagnostics.Add(TEXT("template id must be a safe path segment."));
		return false;
	}

	FString projectTemplatesPath;
	FString runDefaultsPath;
	if (!LocateUserProjectResourceDirectories(projectTemplatesPath, runDefaultsPath, &outDiagnostics))
	{
		return false;
	}

	const FString templatePath = NormalizeAbsolutePath(FPaths::Combine(projectTemplatesPath, trimmedTemplateId));
	if (!ValidateUserProjectDirectory(templatePath, false, outDiagnostics))
	{
		return false;
	}

	const FString resolvedProjectPath = NormalizeAbsolutePath(projectPath);
	if (!IsDirectoryEmptyOrMissing(resolvedProjectPath, outDiagnostics))
	{
		return false;
	}

	if (!IFileManager::Get().MakeDirectory(*resolvedProjectPath, true))
	{
		outDiagnostics.Add(FString::Printf(TEXT("project directory create failed: %s"), *resolvedProjectPath));
		return false;
	}

	if (!CopyUserProjectTree(templatePath, resolvedProjectPath, false, outDiagnostics))
	{
		return false;
	}

	const FString runsPath = NormalizeAbsolutePath(FPaths::Combine(resolvedProjectPath, UserProjectRunsDirectory));
	if (!IFileManager::Get().MakeDirectory(*runsPath, true))
	{
		outDiagnostics.Add(FString::Printf(TEXT("runs directory create failed: %s"), *runsPath));
		return false;
	}

	return ValidateUserProjectDirectory(resolvedProjectPath, true, outDiagnostics);
}

bool USimulatorLaunchSubsystem::CreateProjectRun(
	const FString& projectPath,
	FString& outRunId,
	TArray<FString>& outDiagnostics) const
{
	outRunId.Reset();
	outDiagnostics.Reset();

	const FString resolvedProjectPath = NormalizeAbsolutePath(projectPath);
	if (!ValidateUserProjectDirectory(resolvedProjectPath, true, outDiagnostics))
	{
		return false;
	}

	FString projectTemplatesPath;
	FString runDefaultsPath;
	if (!LocateUserProjectResourceDirectories(projectTemplatesPath, runDefaultsPath, &outDiagnostics))
	{
		return false;
	}

	const FString runsPath = NormalizeAbsolutePath(FPaths::Combine(resolvedProjectPath, UserProjectRunsDirectory));
	if (!FindNextUserProjectRunId(runsPath, outRunId, outDiagnostics))
	{
		return false;
	}

	const FUserProjectRunSnapshotPaths paths = FUserProjectRunSnapshot::BuildPaths(resolvedProjectPath, outRunId);
	if (IsDirectory(paths.RunPath) || FPaths::FileExists(paths.RunPath))
	{
		outDiagnostics.Add(FString::Printf(TEXT("run path already exists: %s"), *paths.RunPath));
		return false;
	}
	if (!IFileManager::Get().MakeDirectory(*paths.RunPath, true))
	{
		outDiagnostics.Add(FString::Printf(TEXT("run directory create failed: %s"), *paths.RunPath));
		return false;
	}

	if (!CopyUserProjectTree(runDefaultsPath, paths.RunPath, true, outDiagnostics))
	{
		return false;
	}
	if (!IFileManager::Get().MakeDirectory(*paths.SnapshotPath, true))
	{
		outDiagnostics.Add(FString::Printf(TEXT("snapshot directory create failed: %s"), *paths.SnapshotPath));
		return false;
	}

	const TPair<FString, FString> snapshotFiles[] = {
		TPair<FString, FString>(
			NormalizeAbsolutePath(FPaths::Combine(resolvedProjectPath, UserProjectSettingFileName)),
			paths.SettingPath),
		TPair<FString, FString>(
			NormalizeAbsolutePath(FPaths::Combine(resolvedProjectPath, UserProjectProfileFileName)),
			paths.ProfilePath),
		TPair<FString, FString>(
			NormalizeAbsolutePath(FPaths::Combine(resolvedProjectPath, UserProjectScenarioFileName)),
			paths.ScenarioPath),
	};
	for (const TPair<FString, FString>& snapshotFile : snapshotFiles)
	{
		if (!CopyUserProjectFile(snapshotFile.Key, snapshotFile.Value, outDiagnostics))
		{
			return false;
		}
	}

	const FString projectPolicyPath = NormalizeAbsolutePath(FPaths::Combine(resolvedProjectPath, UserProjectPolicyDirectory));
	if (!CopyUserProjectTree(projectPolicyPath, paths.PolicyPath, false, outDiagnostics))
	{
		return false;
	}

	const FUserProjectRunSnapshotParseResult snapshotParseResult =
		FUserProjectRunSnapshot::Parse(resolvedProjectPath, outRunId);
	if (!snapshotParseResult.bSuccess)
	{
		AddUserProjectJsonDiagnostics(TEXT("run snapshot"), snapshotParseResult.Diagnostics, outDiagnostics);
		return false;
	}

	return true;
}

TArray<FString> USimulatorLaunchSubsystem::ListProjectRunDirectories(const FString& projectPath) const
{
	const FString runsPath = NormalizeAbsolutePath(FPaths::Combine(projectPath, UserProjectRunsDirectory));
	TArray<FString> runDirectoryNames;
	IFileManager::Get().FindFiles(runDirectoryNames, *FPaths::Combine(runsPath, TEXT("*")), false, true);

	TArray<FString> runDirectories;
	for (const FString& runDirectoryName : runDirectoryNames)
	{
		if (FUserProjectRunSnapshot::IsValidRunId(runDirectoryName))
		{
			runDirectories.Add(NormalizeAbsolutePath(FPaths::Combine(runsPath, runDirectoryName)));
		}
	}
	runDirectories.Sort();
	return runDirectories;
}

TArray<FString> USimulatorLaunchSubsystem::ListProjectEpisodeResultFiles(const FString& runDirectory) const
{
	TArray<FString> resultFiles;
	const FString resolvedRunDirectory = NormalizeAbsolutePath(runDirectory);
	IFileManager::Get().FindFilesRecursive(resultFiles, *resolvedRunDirectory, TEXT("result.json"), true, false);
	for (FString& resultFile : resultFiles)
	{
		resultFile = NormalizeAbsolutePath(resultFile);
	}
	resultFiles.Sort();
	return resultFiles;
}

TArray<FString> USimulatorLaunchSubsystem::ListProjectRunLogFiles(const FString& runDirectory) const
{
	TArray<FString> logFiles;
	const FString resolvedRunDirectory = NormalizeAbsolutePath(runDirectory);
	IFileManager::Get().FindFilesRecursive(logFiles, *resolvedRunDirectory, TEXT("*.jsonl"), true, false);
	for (FString& logFile : logFiles)
	{
		logFile = NormalizeAbsolutePath(logFile);
	}
	logFiles.Sort();
	return logFiles;
}

bool USimulatorLaunchSubsystem::CreateRuntimeSimulationSetupFile(
	const FSimulationSetup& sourceSetup,
	const FString& runId,
	FString& outRuntimeSetupPath,
	FSimulationSetup& outRuntimeSetup,
	TArray<FString>& outDiagnostics) const
{
	outDiagnostics.Reset();
	outRuntimeSetup = sourceSetup;
	FSimulationSetupJson::ApplyRunOutputPaths(outRuntimeSetup, runId);
	outRuntimeSetupPath = FSimulationSetupJson::BuildRunSetupPath(runId);

	if (!FSimulationSetupJson::SaveToFile(outRuntimeSetup, outRuntimeSetupPath, outDiagnostics))
	{
		return false;
	}

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

FString USimulatorLaunchSubsystem::BuildSimulatorArgumentString(const FString& setupPath, const FString& runId)
{
	return FString::Printf(
		TEXT("%s %s %s"),
		*QuoteCommandLineArgument(FString::Printf(TEXT("-Simulate=%s"), *setupPath)),
		*QuoteCommandLineArgument(FString::Printf(TEXT("-RunId=%s"), *runId)),
		SimulatorProcessFlags);
}

FString USimulatorLaunchSubsystem::BuildProjectRunSimulatorArgumentString(const FString& projectPath, const FString& runId)
{
	return FString::Printf(
		TEXT("%s %s %s %s"),
		*QuoteCommandLineArgument(FString::Printf(TEXT("-OdiroProject=%s"), *projectPath)),
		*QuoteCommandLineArgument(FString::Printf(TEXT("-RunId=%s"), *runId)),
		*QuoteCommandLineArgument(FString::Printf(TEXT("-PolicyPort=%d"), ProjectRunDefaultPolicyPort)),
		SimulatorProcessFlags);
}

FString USimulatorLaunchSubsystem::BuildPreviewLauncherArgumentString(
	const FString& previewBatPath,
	const FString& setupPath,
	const FString& runId)
{
	// cmd.exe quoting is intentionally centralized here; CreateProc receives cmd.exe as executable.
	return FString::Printf(
		TEXT("/d /s /c \"\"%s\" %s %s %s\""),
		*previewBatPath,
		*QuoteCommandLineArgument(FString::Printf(TEXT("-Simulate=%s"), *setupPath)),
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
		TEXT("/d /s /c \"\"%s\" %s %s %s %s\""),
		*previewBatPath,
		*QuoteCommandLineArgument(FString::Printf(TEXT("-OdiroProject=%s"), *projectPath)),
		*QuoteCommandLineArgument(FString::Printf(TEXT("-RunId=%s"), *runId)),
		*QuoteCommandLineArgument(FString::Printf(TEXT("-PolicyPort=%d"), ProjectRunDefaultPolicyPort)),
		SimulatorProcessFlags);
}

bool USimulatorLaunchSubsystem::TryReadScenarioRunQueueJson(
	const FString& jsonString,
	TArray<FScenarioRunInput>& outRunInputs,
	TArray<FString>& outDiagnostics)
{
	outRunInputs.Reset();
	outDiagnostics.Reset();

	TSharedPtr<FJsonObject> rootObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(jsonString);
	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		outDiagnostics.Add(TEXT("ScenarioRunQueue JSON parse failed."));
		return false;
	}

	FString schema;
	if (!rootObject->TryGetStringField(TEXT("schema"), schema) || !schema.Equals(LaunchScenarioRunQueueSchema, ESearchCase::CaseSensitive))
	{
		outDiagnostics.Add(FString::Printf(TEXT("ScenarioRunQueue schema must be '%s'."), LaunchScenarioRunQueueSchema));
	}

	double version = 0.0;
	if (!rootObject->TryGetNumberField(TEXT("version"), version) || version <= 0.0)
	{
		outDiagnostics.Add(TEXT("ScenarioRunQueue version must be > 0."));
	}

	const TSharedPtr<FJsonValue> runsValue = rootObject->TryGetField(TEXT("runs"));
	if (!runsValue.IsValid() || runsValue->Type != EJson::Array)
	{
		outDiagnostics.Add(TEXT("ScenarioRunQueue runs must be an array."));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>> runValues = runsValue->AsArray();
	if (runValues.IsEmpty())
	{
		outDiagnostics.Add(TEXT("ScenarioRunQueue runs must contain at least one pair."));
	}

	for (int32 index = 0; index < runValues.Num(); ++index)
	{
		const TSharedPtr<FJsonValue>& runValue = runValues[index];
		if (!runValue.IsValid() || runValue->Type != EJson::Object)
		{
			outDiagnostics.Add(FString::Printf(TEXT("ScenarioRunQueue runs[%d] must be an object."), index));
			continue;
		}

		const TSharedPtr<FJsonObject> runObject = runValue->AsObject();
		FScenarioRunInput runInput;
		runObject->TryGetStringField(TEXT("pair_id"), runInput.PairId);
		runObject->TryGetStringField(TEXT("scenario_setup"), runInput.ScenarioSetupJsonPath);
		runObject->TryGetStringField(TEXT("delivery_bot_setup"), runInput.DeliveryBotSetupJsonPath);
		runObject->TryGetStringField(TEXT("policy_spec"), runInput.PolicySpecJsonPath);

		runInput.PairId = runInput.PairId.TrimStartAndEnd();
		runInput.ScenarioSetupJsonPath = NormalizeRunQueueReferencePath(runInput.ScenarioSetupJsonPath);
		runInput.DeliveryBotSetupJsonPath = NormalizeRunQueueReferencePath(runInput.DeliveryBotSetupJsonPath);
		runInput.PolicySpecJsonPath = NormalizeRunQueueReferencePath(runInput.PolicySpecJsonPath);

		if (runInput.ScenarioSetupJsonPath.TrimStartAndEnd().IsEmpty())
		{
			outDiagnostics.Add(FString::Printf(TEXT("ScenarioRunQueue runs[%d].scenario_setup must not be empty."), index));
		}

		if (runInput.DeliveryBotSetupJsonPath.TrimStartAndEnd().IsEmpty())
		{
			outDiagnostics.Add(FString::Printf(TEXT("ScenarioRunQueue runs[%d].delivery_bot_setup must not be empty."), index));
		}

		outRunInputs.Add(runInput);
	}

	return outDiagnostics.IsEmpty();
}

bool USimulatorLaunchSubsystem::TryWriteScenarioRunQueueJson(
	const TArray<FScenarioRunInput>& runInputs,
	FString& outJson,
	TArray<FString>& outDiagnostics)
{
	outJson.Reset();
	outDiagnostics.Reset();

	if (runInputs.IsEmpty())
	{
		outDiagnostics.Add(TEXT("ScenarioRunQueue requires at least one pair."));
	}

	for (int32 index = 0; index < runInputs.Num(); ++index)
	{
		const FScenarioRunInput& runInput = runInputs[index];
		if (runInput.ScenarioSetupJsonPath.TrimStartAndEnd().IsEmpty())
		{
			outDiagnostics.Add(FString::Printf(TEXT("ScenarioRunQueue pair %d requires scenario_setup."), index));
		}
		else if (!IsScenarioSetupFile(runInput.ScenarioSetupJsonPath))
		{
			outDiagnostics.Add(FString::Printf(TEXT("Scenario source validation failed: %s"), *runInput.ScenarioSetupJsonPath));
		}

		if (runInput.DeliveryBotSetupJsonPath.TrimStartAndEnd().IsEmpty())
		{
			outDiagnostics.Add(FString::Printf(TEXT("ScenarioRunQueue pair %d requires delivery_bot_setup."), index));
		}
		else if (!IsDeliveryBotSetupFile(runInput.DeliveryBotSetupJsonPath))
		{
			outDiagnostics.Add(FString::Printf(TEXT("DeliveryBotSetup validation failed: %s"), *runInput.DeliveryBotSetupJsonPath));
		}

		if (!runInput.PolicySpecJsonPath.TrimStartAndEnd().IsEmpty()
			&& !IsPolicySpecFile(runInput.PolicySpecJsonPath))
		{
			outDiagnostics.Add(FString::Printf(TEXT("PolicySpec validation failed: %s"), *runInput.PolicySpecJsonPath));
		}
	}

	if (!outDiagnostics.IsEmpty())
	{
		return false;
	}

	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&outJson);
	if (!FJsonSerializer::Serialize(MakeScenarioRunQueueObject(runInputs), writer))
	{
		outDiagnostics.Add(TEXT("ScenarioRunQueue JSON serialization failed."));
		return false;
	}

	return true;
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
