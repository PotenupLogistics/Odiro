#include "Platform/PlatformUiSubsystem.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Platform/PlatformAnalysisAiSubsystem.h"
#include "Platform/ProjectRunResultDashboard.h"
#include "Platform/ProjectSessionSubsystem.h"
#include "Platform/ScenarioEditorLaunchSubsystem.h"
#include "Platform/SimulatorLaunchSubsystem.h"
#include "Platform/ViewModel/ExperimentConfigViewModel.h"
#include "Platform/ViewModel/ExperimentResultViewModel.h"
#include "Platform/ViewModel/OdiroListItemViewModel.h"
#include "Platform/ViewModel/ProjectWorkspaceViewModel.h"
#include "Platform/ViewModel/StartupMenuViewModel.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	const TCHAR* PlatformUiExperimentSettingFileName = TEXT("setting.json");
	const TCHAR* PlatformUiExperimentDefaultMapId = TEXT("ScenarioSimulationMap");

	// user project path 입력을 absolute normalized path로 맞춘다.
	FString NormalizePlatformUiProjectPath(FString path)
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

	// Platform UI가 편집하는 JSON 파일 root object를 읽는다.
	bool LoadPlatformUiJsonRoot(
		const FString& path,
		TSharedPtr<FJsonObject>& outRootObject,
		FString& outError)
	{
		outRootObject.Reset();
		outError.Reset();

		FString jsonString;
		if (!FFileHelper::LoadFileToString(jsonString, *path))
		{
			outError = FString::Printf(TEXT("setting.json 읽기 실패: %s"), *path);
			return false;
		}

		const TSharedRef<TJsonReader<TCHAR>> reader = TJsonReaderFactory<TCHAR>::Create(jsonString);
		if (!FJsonSerializer::Deserialize(reader, outRootObject) || !outRootObject.IsValid())
		{
			outError = FString::Printf(TEXT("setting.json 파싱 실패: %s"), *path);
			return false;
		}

		return true;
	}

	// JSON object field를 찾아 반환하거나 저장을 위해 새 object field를 만든다.
	TSharedRef<FJsonObject> FindOrCreatePlatformUiObjectField(
		const TSharedRef<FJsonObject>& rootObject,
		const TCHAR* fieldName)
	{
		const TSharedPtr<FJsonObject>* existingObject = nullptr;
		if (rootObject->TryGetObjectField(fieldName, existingObject) && existingObject && existingObject->IsValid())
		{
			return existingObject->ToSharedRef();
		}

		TSharedRef<FJsonObject> newObject = MakeShared<FJsonObject>();
		rootObject->SetObjectField(fieldName, newObject);
		return newObject;
	}

	// user project root에서 experiment setting.json 경로를 만든다.
	FString BuildPlatformUiExperimentSettingPath(const FString& projectPath)
	{
		return NormalizePlatformUiProjectPath(FPaths::Combine(projectPath, PlatformUiExperimentSettingFileName));
	}

	// 파일 저장 경계에서 experiment setting subset 유효성을 확인한다.
	bool ValidatePlatformUiExperimentSettings(
		const FExperimentConfigSettings& settings,
		FString& outErrorText)
	{
		TArray<FString> diagnostics;
		if (settings.MapId.TrimStartAndEnd().IsEmpty())
		{
			diagnostics.Add(TEXT("Map ID를 입력하세요."));
		}
		if (settings.FixedFps <= 0)
		{
			diagnostics.Add(TEXT("Fixed FPS는 1 이상이어야 합니다."));
		}
		if (settings.EpisodeCount <= 0)
		{
			diagnostics.Add(TEXT("Episode Count는 1 이상이어야 합니다."));
		}

		outErrorText = FString::Join(diagnostics, TEXT("\n"));
		return diagnostics.IsEmpty();
	}
}

void UPlatformUiSubsystem::Initialize(FSubsystemCollectionBase& collection)
{
	Super::Initialize(collection);

	StartupMenuViewModel = NewObject<UStartupMenuViewModel>(this);
	ProjectWorkspaceViewModel = NewObject<UProjectWorkspaceViewModel>(this);
	ExperimentConfigViewModel = NewObject<UExperimentConfigViewModel>(this);
	ExperimentResultViewModel = NewObject<UExperimentResultViewModel>(this);

	if (StartupMenuViewModel)
	{
		StartupMenuViewModel->InitializeForGameInstance(GetGameInstance());
	}
	if (ProjectWorkspaceViewModel)
	{
		ProjectWorkspaceViewModel->InitializeForGameInstance(GetGameInstance());
	}
	if (ExperimentConfigViewModel)
	{
		ExperimentConfigViewModel->InitializeForGameInstance(GetGameInstance());
	}
	if (ExperimentResultViewModel)
	{
		ExperimentResultViewModel->InitializeForGameInstance(GetGameInstance());
		ExperimentResultViewModel->SetSubsystemOverride(this);
	}

	if (USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem())
	{
		subsystem->OnRunInfoChanged.RemoveAll(this);
		subsystem->OnRunInfoChanged.AddUObject(this, &UPlatformUiSubsystem::HandleRunInfoChanged);
	}
	if (UPlatformAnalysisAiSubsystem* analysisSubsystem = ResolvePlatformAnalysisAiSubsystem())
	{
		analysisSubsystem->OnAnalysisCompleted.RemoveAll(this);
		analysisSubsystem->OnAnalysisCompleted.AddUObject(this, &UPlatformUiSubsystem::HandleAnalysisCompleted);
	}
}

void UPlatformUiSubsystem::Deinitialize()
{
	if (USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem())
	{
		subsystem->OnRunInfoChanged.RemoveAll(this);
	}
	if (UPlatformAnalysisAiSubsystem* analysisSubsystem = ResolvePlatformAnalysisAiSubsystem())
	{
		analysisSubsystem->OnAnalysisCompleted.RemoveAll(this);
	}

	StartupMenuViewModel = nullptr;
	ProjectWorkspaceViewModel = nullptr;
	ExperimentConfigViewModel = nullptr;
	ExperimentResultViewModel = nullptr;

	Super::Deinitialize();
}

UPlatformUiSubsystem* UPlatformUiSubsystem::ResolveForWorldContext(const UObject* worldContextObject)
{
	UWorld* world = worldContextObject ? worldContextObject->GetWorld() : nullptr;
	UGameInstance* gameInstance = world ? world->GetGameInstance() : nullptr;
	return gameInstance ? gameInstance->GetSubsystem<UPlatformUiSubsystem>() : nullptr;
}

void UPlatformUiSubsystem::RefreshFromProjectSession()
{
	if (ProjectWorkspaceViewModel)
	{
		ProjectWorkspaceViewModel->RefreshFromProjectSession();
	}
	if (ExperimentConfigViewModel)
	{
		ExperimentConfigViewModel->LoadFromActiveProject();
	}
}

FProjectPresetCatalog UPlatformUiSubsystem::ListProjectPresets() const
{
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	return subsystem ? subsystem->ListProjectPresets() : FProjectPresetCatalog();
}

bool UPlatformUiSubsystem::ValidateUserProject(const FString& projectPath, TArray<FString>& outDiagnostics) const
{
	outDiagnostics.Reset();

	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		outDiagnostics.Add(TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다."));
		return false;
	}

	return subsystem->ValidateUserProject(projectPath, outDiagnostics);
}

bool UPlatformUiSubsystem::HasActiveProject() const
{
	const UProjectSessionSubsystem* projectSession = ResolveProjectSessionSubsystem();
	return projectSession && projectSession->HasActiveProject();
}

FString UPlatformUiSubsystem::GetActiveProjectPath() const
{
	const UProjectSessionSubsystem* projectSession = ResolveProjectSessionSubsystem();
	return projectSession && projectSession->HasActiveProject()
		? projectSession->GetActiveProjectPath()
		: FString();
}

FString UPlatformUiSubsystem::GetActiveProjectScenarioPath() const
{
	const UProjectSessionSubsystem* projectSession = ResolveProjectSessionSubsystem();
	return projectSession && projectSession->HasActiveProject()
		? projectSession->GetActiveProjectScenarioPath()
		: FString();
}

bool UPlatformUiSubsystem::ReturnToStartupMap(FString& outErrorText) const
{
	outErrorText.Reset();

	if (UProjectSessionSubsystem* projectSession = ResolveProjectSessionSubsystem())
	{
		projectSession->ClearActiveProject();
	}

	UWorld* world = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!world)
	{
		outErrorText = TEXT("StartupMap으로 돌아갈 World가 없습니다.");
		return false;
	}

	const FString mapId = StartupMapId.TrimStartAndEnd();
	if (mapId.IsEmpty())
	{
		outErrorText = TEXT("StartupMap id가 없습니다.");
		return false;
	}

	UGameplayStatics::OpenLevel(world, FName(*mapId));
	return true;
}

bool UPlatformUiSubsystem::StartLegacySimulationRun(
	const FString& setupPath,
	const FString& requestedRunId,
	FString& outErrorText) const
{
	outErrorText.Reset();

	USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		outErrorText = TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다.");
		return false;
	}

	if (!subsystem->StartSimulationRun(setupPath, requestedRunId))
	{
		outErrorText = subsystem->GetLastError();
		return false;
	}

	return true;
}

bool UPlatformUiSubsystem::OpenScenarioEditorPath(const FString& scenarioPath, FString& outErrorText) const
{
	outErrorText.Reset();

	const UGameInstance* gameInstance = GetGameInstance();
	UScenarioEditorLaunchSubsystem* subsystem = gameInstance ? gameInstance->GetSubsystem<UScenarioEditorLaunchSubsystem>() : nullptr;
	if (!subsystem)
	{
		outErrorText = TEXT("ScenarioEditorLaunchSubsystem을 사용할 수 없습니다.");
		return false;
	}

	if (!subsystem->OpenScenarioEditor(scenarioPath))
	{
		outErrorText = TEXT("ScenarioEditorMap 열기 실패.");
		return false;
	}

	return true;
}

TArray<FString> UPlatformUiSubsystem::ListProjectEpisodeResultFiles(const FString& runDirectory) const
{
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	return subsystem ? subsystem->ListProjectEpisodeResultFiles(runDirectory) : TArray<FString>();
}

bool UPlatformUiSubsystem::LoadProjectRunDashboard(
	const FString& runDirectory,
	FProjectRunResultDashboardData& outDashboardData)
{
	outDashboardData = FProjectRunResultDashboardData{};

	const FString normalizedRunDirectory = NormalizePlatformUiProjectPath(runDirectory);
	if (normalizedRunDirectory.IsEmpty())
	{
		outDashboardData.Diagnostics.Add(TEXT("Run directory가 없습니다."));
		return false;
	}

	const bool bLoaded = FProjectRunResultDashboardJson::BuildFromRunDirectory(
		normalizedRunDirectory,
		outDashboardData);
	if (outDashboardData.RunId.IsEmpty())
	{
		outDashboardData.RunId = FPaths::GetCleanFilename(normalizedRunDirectory);
	}
	return bLoaded;
}

bool UPlatformUiSubsystem::RequestProjectRunAnalysis(
	const FString& projectPath,
	const FString& runId,
	FString& outErrorText) const
{
	outErrorText.Reset();

	UPlatformAnalysisAiSubsystem* subsystem = ResolvePlatformAnalysisAiSubsystem();
	if (!subsystem)
	{
		outErrorText = TEXT("PlatformAnalysisAiSubsystem을 사용할 수 없습니다.");
		return false;
	}

	const FString normalizedProjectPath = NormalizePlatformUiProjectPath(projectPath);
	const FString normalizedRunId = runId.TrimStartAndEnd();
	if (normalizedProjectPath.IsEmpty() || normalizedRunId.IsEmpty())
	{
		outErrorText = TEXT("AI 분석을 요청할 project/run 선택이 필요합니다.");
		return false;
	}

	if (!subsystem->RequestAnalysisForProjectRun(normalizedProjectPath, normalizedRunId))
	{
		outErrorText = TEXT("AI 분석 요청 실패.");
		return false;
	}

	return true;
}

TArray<FString> UPlatformUiSubsystem::ListLegacySimulationSetupFiles() const
{
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	return subsystem ? subsystem->ListSimulationSetupFiles() : TArray<FString>();
}

TArray<FString> UPlatformUiSubsystem::ListLegacyScenarioSetupFiles() const
{
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	return subsystem ? subsystem->ListScenarioSetupFiles() : TArray<FString>();
}

TArray<FString> UPlatformUiSubsystem::ListLegacyDeliveryBotSetupFiles() const
{
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	return subsystem ? subsystem->ListDeliveryBotSetupFiles() : TArray<FString>();
}

TArray<FString> UPlatformUiSubsystem::ListLegacyPolicySpecFiles() const
{
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	return subsystem ? subsystem->ListPolicySpecFiles() : TArray<FString>();
}

TArray<FString> UPlatformUiSubsystem::ListLegacySimulationRunResultDirectories() const
{
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	return subsystem ? subsystem->ListSimulationRunResultDirectories() : TArray<FString>();
}

TArray<FString> UPlatformUiSubsystem::ListLegacySimulationRunStatusFiles() const
{
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	return subsystem ? subsystem->ListSimulationRunStatusFiles() : TArray<FString>();
}

TArray<FString> UPlatformUiSubsystem::ListLegacyEvaluationReportFilesInDirectory(const FString& runDirectory) const
{
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	return subsystem ? subsystem->ListEvaluationReportFilesInDirectory(runDirectory) : TArray<FString>();
}

TArray<FString> UPlatformUiSubsystem::ListLegacyMeasurementLogFilesInDirectory(const FString& runDirectory) const
{
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	return subsystem ? subsystem->ListMeasurementLogFilesInDirectory(runDirectory) : TArray<FString>();
}

TArray<UOdiroListItemViewModel*> UPlatformUiSubsystem::CreatePathItemViewModels(
	const TArray<FString>& itemPaths,
	const bool bUseBaseFilenameAsTitle)
{
	TArray<UOdiroListItemViewModel*> items;
	items.Reserve(itemPaths.Num());
	for (const FString& itemPath : itemPaths)
	{
		UOdiroListItemViewModel* item = NewObject<UOdiroListItemViewModel>(this);
		if (!item)
		{
			continue;
		}

		const FString normalizedPath = itemPath.TrimStartAndEnd();
		const FString title = bUseBaseFilenameAsTitle
			? FPaths::GetBaseFilename(normalizedPath)
			: normalizedPath;
		const FString subtitle = bUseBaseFilenameAsTitle ? normalizedPath : FPaths::GetPath(normalizedPath);
		item->InitializeItem(normalizedPath, title, subtitle, normalizedPath);
		items.Add(item);
	}
	return items;
}

FSimulationSetupParseResult UPlatformUiSubsystem::LoadLegacySimulationSetupFile(const FString& setupPath) const
{
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	return subsystem ? subsystem->LoadSimulationSetupFile(setupPath) : FSimulationSetupParseResult();
}

bool UPlatformUiSubsystem::SaveLegacyFixedStepFpsToSetupFile(
	const FString& setupPath,
	const int32 fps,
	TArray<FString>& outDiagnostics) const
{
	outDiagnostics.Reset();
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		outDiagnostics.Add(TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다."));
		return false;
	}
	return subsystem->SaveFixedStepFpsToSetupFile(setupPath, fps, outDiagnostics);
}

bool UPlatformUiSubsystem::SaveLegacySimulationSetupFile(
	const FString& setupPath,
	const FSimulationSetup& setup,
	TArray<FString>& outDiagnostics) const
{
	outDiagnostics.Reset();
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		outDiagnostics.Add(TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다."));
		return false;
	}
	return subsystem->SaveSimulationSetupFile(setupPath, setup, outDiagnostics);
}

bool UPlatformUiSubsystem::LoadLegacyScenarioRunQueueFile(
	const FString& runQueuePath,
	TArray<FScenarioRunInput>& outRunInputs,
	TArray<FString>& outDiagnostics) const
{
	outRunInputs.Reset();
	outDiagnostics.Reset();
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		outDiagnostics.Add(TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다."));
		return false;
	}
	return subsystem->LoadScenarioRunQueueFile(runQueuePath, outRunInputs, outDiagnostics);
}

bool UPlatformUiSubsystem::SaveLegacyScenarioRunQueueFile(
	const FString& runQueuePath,
	const TArray<FScenarioRunInput>& runInputs,
	TArray<FString>& outDiagnostics) const
{
	outDiagnostics.Reset();
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		outDiagnostics.Add(TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다."));
		return false;
	}
	return subsystem->SaveScenarioRunQueueFile(runQueuePath, runInputs, outDiagnostics);
}

bool UPlatformUiSubsystem::LoadExperimentSettingsForProject(
	const FString& projectPath,
	FExperimentConfigSettings& outSettings,
	FString& outErrorText)
{
	outSettings = FExperimentConfigSettings{};
	outErrorText.Reset();

	const FString normalizedProjectPath = NormalizePlatformUiProjectPath(projectPath);
	if (normalizedProjectPath.IsEmpty())
	{
		outErrorText = TEXT("Active project가 없습니다.");
		return false;
	}

	TSharedPtr<FJsonObject> rootObject;
	const FString settingPath = BuildPlatformUiExperimentSettingPath(normalizedProjectPath);
	if (!LoadPlatformUiJsonRoot(settingPath, rootObject, outErrorText))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* runtimeObject = nullptr;
	const TSharedPtr<FJsonObject>* samplingObject = nullptr;
	if (!rootObject->TryGetObjectField(TEXT("runtime"), runtimeObject) || !runtimeObject || !runtimeObject->IsValid())
	{
		outErrorText = TEXT("setting.json에 runtime object가 없습니다.");
		return false;
	}
	if (!rootObject->TryGetObjectField(TEXT("sampling"), samplingObject) || !samplingObject || !samplingObject->IsValid())
	{
		outErrorText = TEXT("setting.json에 sampling object가 없습니다.");
		return false;
	}

	FString mapId;
	if (!(*runtimeObject)->TryGetStringField(TEXT("map_id"), mapId) || mapId.TrimStartAndEnd().IsEmpty())
	{
		mapId = PlatformUiExperimentDefaultMapId;
	}

	double fixedFps = 60.0;
	(*runtimeObject)->TryGetNumberField(TEXT("fixed_fps"), fixedFps);
	double episodeCount = 1.0;
	(*samplingObject)->TryGetNumberField(TEXT("episode_count"), episodeCount);
	double baseSeed = 0.0;
	(*samplingObject)->TryGetNumberField(TEXT("base_seed"), baseSeed);

	outSettings.MapId = mapId.TrimStartAndEnd();
	outSettings.FixedFps = FMath::Max(1, FMath::RoundToInt(fixedFps));
	outSettings.EpisodeCount = FMath::Max(1, FMath::RoundToInt(episodeCount));
	outSettings.BaseSeed = static_cast<int64>(baseSeed);
	return true;
}

bool UPlatformUiSubsystem::SaveExperimentSettingsForProject(
	const FString& projectPath,
	const FExperimentConfigSettings& settings,
	FString& outStatusText)
{
	outStatusText.Reset();
	if (!ValidatePlatformUiExperimentSettings(settings, outStatusText))
	{
		return false;
	}

	const FString normalizedProjectPath = NormalizePlatformUiProjectPath(projectPath);
	if (normalizedProjectPath.IsEmpty())
	{
		outStatusText = TEXT("Active project가 없습니다.");
		return false;
	}

	TSharedPtr<FJsonObject> rootObject;
	const FString settingPath = BuildPlatformUiExperimentSettingPath(normalizedProjectPath);
	if (!LoadPlatformUiJsonRoot(settingPath, rootObject, outStatusText))
	{
		return false;
	}

	const TSharedRef<FJsonObject> runtimeObject =
		FindOrCreatePlatformUiObjectField(rootObject.ToSharedRef(), TEXT("runtime"));
	const TSharedRef<FJsonObject> samplingObject =
		FindOrCreatePlatformUiObjectField(rootObject.ToSharedRef(), TEXT("sampling"));
	runtimeObject->SetStringField(TEXT("map_id"), settings.MapId.TrimStartAndEnd());
	runtimeObject->SetNumberField(TEXT("fixed_fps"), settings.FixedFps);
	samplingObject->SetNumberField(TEXT("episode_count"), settings.EpisodeCount);
	samplingObject->SetNumberField(TEXT("base_seed"), static_cast<double>(settings.BaseSeed));

	FString updatedJson;
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&updatedJson);
	if (!FJsonSerializer::Serialize(rootObject.ToSharedRef(), writer))
	{
		outStatusText = TEXT("setting.json 직렬화 실패.");
		return false;
	}

	if (!FFileHelper::SaveStringToFile(
		updatedJson,
		*settingPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		outStatusText = FString::Printf(TEXT("setting.json 저장 실패: %s"), *settingPath);
		return false;
	}

	outStatusText = FString::Printf(TEXT("Experiment settings saved: %s"), *settingPath);
	return true;
}

bool UPlatformUiSubsystem::ReplaceLegacyScenarioSetupReferencesInRunQueues(
	const FString& oldScenarioSetupPath,
	const FString& newScenarioSetupPath,
	TArray<FString>& outDiagnostics) const
{
	outDiagnostics.Reset();
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		outDiagnostics.Add(TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다."));
		return false;
	}
	return subsystem->ReplaceScenarioSetupReferencesInRunQueues(
		oldScenarioSetupPath,
		newScenarioSetupPath,
		outDiagnostics);
}

bool UPlatformUiSubsystem::ReplaceLegacyDeliveryBotSetupReferencesInRunQueues(
	const FString& oldDeliveryBotSetupPath,
	const FString& newDeliveryBotSetupPath,
	TArray<FString>& outDiagnostics) const
{
	outDiagnostics.Reset();
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		outDiagnostics.Add(TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다."));
		return false;
	}
	return subsystem->ReplaceDeliveryBotSetupReferencesInRunQueues(
		oldDeliveryBotSetupPath,
		newDeliveryBotSetupPath,
		outDiagnostics);
}

bool UPlatformUiSubsystem::RefreshActiveRunStatus() const
{
	USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	return subsystem ? subsystem->RefreshActiveRunStatus() : false;
}

FSimulatorRunInfo UPlatformUiSubsystem::GetActiveRunInfo() const
{
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	return subsystem ? subsystem->GetActiveRunInfo() : FSimulatorRunInfo();
}

bool UPlatformUiSubsystem::IsAnalysisRequestPending() const
{
	const UPlatformAnalysisAiSubsystem* subsystem = ResolvePlatformAnalysisAiSubsystem();
	return subsystem && subsystem->IsAnalysisRequestPending();
}

bool UPlatformUiSubsystem::TryReadBridgeRunStatusState(
	const FString& statusPath,
	ESimulationRunState& outState)
{
	outState = ESimulationRunState::Pending;
	if (!FPaths::FileExists(statusPath))
	{
		return false;
	}

	FString statusJson;
	if (!FFileHelper::LoadFileToString(statusJson, *statusPath))
	{
		return false;
	}

	TSharedPtr<FJsonObject> rootObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(statusJson);
	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		return false;
	}

	FString schema;
	if (!rootObject->TryGetStringField(TEXT("schema"), schema)
		|| !schema.Equals(TEXT("run_status"), ESearchCase::CaseSensitive))
	{
		return false;
	}

	FString stateText;
	if (!rootObject->TryGetStringField(TEXT("state"), stateText))
	{
		return false;
	}

	stateText = stateText.TrimStartAndEnd().ToLower();
	if (stateText == TEXT("starting")
		|| stateText == TEXT("running")
		|| stateText == TEXT("stopping"))
	{
		outState = ESimulationRunState::Running;
		return true;
	}

	if (stateText == TEXT("exited") || stateText == TEXT("completed"))
	{
		outState = ESimulationRunState::Completed;
		return true;
	}

	if (stateText == TEXT("failed"))
	{
		outState = ESimulationRunState::Failed;
		return true;
	}

	if (stateText == TEXT("canceled") || stateText == TEXT("cancelled"))
	{
		outState = ESimulationRunState::Canceled;
		return true;
	}

	return false;
}

FString UPlatformUiSubsystem::BuildLogPreview(const FString& logPath, const int32 edgeLineCount)
{
	TArray<FString> lines;
	if (!FFileHelper::LoadFileToStringArray(lines, *FSimulationSetupJson::ResolveProjectPath(logPath)))
	{
		return FString::Printf(TEXT("Log read failed: %s"), *logPath);
	}

	const int32 clampedEdgeLineCount = FMath::Max(0, edgeLineCount);
	TArray<FString> previewLines;
	for (int32 lineIndex = 0; lineIndex < FMath::Min(clampedEdgeLineCount, lines.Num()); ++lineIndex)
	{
		previewLines.Add(lines[lineIndex]);
	}

	if (lines.Num() > clampedEdgeLineCount * 2)
	{
		previewLines.Add(TEXT("..."));
	}

	const int32 tailStartIndex = FMath::Max(clampedEdgeLineCount, lines.Num() - clampedEdgeLineCount);
	for (int32 lineIndex = tailStartIndex; lineIndex < lines.Num(); ++lineIndex)
	{
		previewLines.Add(lines[lineIndex]);
	}

	return FString::Join(previewLines, TEXT("\n"));
}

bool UPlatformUiSubsystem::TryReadExperimentResultReportItem(
	const FString& reportPath,
	FExperimentResultReportItem& outItem)
{
	outItem = FExperimentResultReportItem{};
	outItem.ReportPath = reportPath;

	FString reportJson;
	if (!ReadResolvedTextFile(reportPath, reportJson))
	{
		return false;
	}

	TSharedPtr<FJsonObject> rootObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(reportJson);
	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		return false;
	}

	FString schema;
	if (!rootObject->TryGetStringField(TEXT("schema"), schema)
		|| !schema.Equals(TEXT("episode_evaluation_report"), ESearchCase::CaseSensitive))
	{
		return false;
	}

	const TSharedPtr<FJsonValue> runValue = rootObject->TryGetField(TEXT("run"));
	if (!runValue.IsValid() || runValue->Type != EJson::Object)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> runObject = runValue->AsObject();
	if (!runObject.IsValid())
	{
		return false;
	}

	double runIndex = 0.0;
	if (runObject->TryGetNumberField(TEXT("run_index"), runIndex))
	{
		outItem.RunIndex = FMath::RoundToInt(runIndex);
	}

	return true;
}

TArray<FExperimentResultReportItem> UPlatformUiSubsystem::BuildExperimentResultReportItems(
	const TArray<FString>& reportPaths)
{
	TArray<FExperimentResultReportItem> items;
	for (const FString& reportPath : reportPaths)
	{
		FExperimentResultReportItem item;
		if (TryReadExperimentResultReportItem(reportPath, item))
		{
			items.Add(item);
		}
	}

	items.Sort([](const FExperimentResultReportItem& left, const FExperimentResultReportItem& right)
	{
		if (left.RunIndex != right.RunIndex)
		{
			if (left.RunIndex == INDEX_NONE)
			{
				return false;
			}
			if (right.RunIndex == INDEX_NONE)
			{
				return true;
			}
			return left.RunIndex < right.RunIndex;
		}
		return left.ReportPath < right.ReportPath;
	});
	return items;
}

bool UPlatformUiSubsystem::CreateTextFileFromTemplate(
	const FString& templatePath,
	const FString& outputPath,
	FString& outErrorText)
{
	outErrorText.Reset();

	const FString resolvedTemplatePath = FSimulationSetupJson::ResolveProjectPath(templatePath);
	FString templateText;
	if (!FFileHelper::LoadFileToString(templateText, *resolvedTemplatePath))
	{
		outErrorText = FString::Printf(TEXT("Template read failed: %s"), *templatePath);
		return false;
	}

	const FString resolvedOutputPath = FSimulationSetupJson::ResolveProjectPath(outputPath);
	const FString outputDirectory = FPaths::GetPath(resolvedOutputPath);
	if (!IFileManager::Get().MakeDirectory(*outputDirectory, true)
		|| !FFileHelper::SaveStringToFile(
			templateText,
			*resolvedOutputPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		outErrorText = FString::Printf(TEXT("File create failed: %s"), *resolvedOutputPath);
		return false;
	}

	return true;
}

bool UPlatformUiSubsystem::MoveProjectRelativeFile(
	const FString& sourcePath,
	const FString& targetPath,
	const FString& itemLabel,
	FString& outErrorText)
{
	outErrorText.Reset();
	if (sourcePath.Equals(targetPath, ESearchCase::IgnoreCase))
	{
		return true;
	}

	const FString resolvedSourcePath = FSimulationSetupJson::ResolveProjectPath(sourcePath);
	const FString resolvedTargetPath = FSimulationSetupJson::ResolveProjectPath(targetPath);
	if (!FPaths::FileExists(resolvedSourcePath))
	{
		outErrorText = FString::Printf(TEXT("%s 파일을 찾을 수 없습니다: %s"), *itemLabel, *sourcePath);
		return false;
	}
	if (FPaths::FileExists(resolvedTargetPath))
	{
		outErrorText = FString::Printf(TEXT("%s 파일이 이미 존재합니다: %s"), *itemLabel, *targetPath);
		return false;
	}

	const FString targetDirectory = FPaths::GetPath(resolvedTargetPath);
	if (!IFileManager::Get().MakeDirectory(*targetDirectory, true)
		|| !IFileManager::Get().Move(*resolvedTargetPath, *resolvedSourcePath, false, false))
	{
		outErrorText = FString::Printf(TEXT("%s 이름 변경 실패: %s -> %s"), *itemLabel, *sourcePath, *targetPath);
		return false;
	}

	return true;
}

FString UPlatformUiSubsystem::MakeUniqueInputJsonPath(const FString& baseFileName)
{
	for (int32 index = 0; index < 1000; ++index)
	{
		const FString fileName = index == 0
			? FString::Printf(TEXT("%s.json"), *baseFileName)
			: FString::Printf(TEXT("%s_%d.json"), *baseFileName, index);
		FString relativePath = FPaths::Combine(TEXT("Json/Input"), fileName);
		relativePath.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (!DoesResolvedFileExist(relativePath))
		{
			return relativePath;
		}
	}

	FString fallbackPath = FPaths::Combine(
		TEXT("Json/Input"),
		FString::Printf(TEXT("%s_%s.json"), *baseFileName, *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8)));
	fallbackPath.ReplaceInline(TEXT("\\"), TEXT("/"));
	return fallbackPath;
}

bool UPlatformUiSubsystem::DoesResolvedFileExist(const FString& path)
{
	return FPaths::FileExists(FSimulationSetupJson::ResolveProjectPath(path));
}

bool UPlatformUiSubsystem::DoesResolvedDirectoryExist(const FString& path)
{
	return IFileManager::Get().DirectoryExists(*FSimulationSetupJson::ResolveProjectPath(path));
}

bool UPlatformUiSubsystem::ReadResolvedTextFile(const FString& path, FString& outText)
{
	outText.Reset();
	return FFileHelper::LoadFileToString(outText, *FSimulationSetupJson::ResolveProjectPath(path));
}

USimulatorLaunchSubsystem* UPlatformUiSubsystem::ResolveSimulatorLaunchSubsystem() const
{
	const UGameInstance* gameInstance = GetGameInstance();
	return gameInstance ? gameInstance->GetSubsystem<USimulatorLaunchSubsystem>() : nullptr;
}

UProjectSessionSubsystem* UPlatformUiSubsystem::ResolveProjectSessionSubsystem() const
{
	const UGameInstance* gameInstance = GetGameInstance();
	return gameInstance ? gameInstance->GetSubsystem<UProjectSessionSubsystem>() : nullptr;
}

UPlatformAnalysisAiSubsystem* UPlatformUiSubsystem::ResolvePlatformAnalysisAiSubsystem() const
{
	const UGameInstance* gameInstance = GetGameInstance();
	return gameInstance ? gameInstance->GetSubsystem<UPlatformAnalysisAiSubsystem>() : nullptr;
}

void UPlatformUiSubsystem::HandleRunInfoChanged(const FSimulatorRunInfo& runInfo)
{
	OnRunInfoChanged.Broadcast(runInfo);
}

void UPlatformUiSubsystem::HandleAnalysisCompleted(const FPlatformAnalysisAiResponse& response)
{
	OnAnalysisCompleted.Broadcast(response);
}
