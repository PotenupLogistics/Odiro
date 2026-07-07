#include "Scenario/ScenarioEditorUiSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Platform/ScenarioEditorLaunchSubsystem.h"
#include "Platform/SimulatorLaunchSubsystem.h"
#include "Scenario/Editor/ScenarioAuthoringSubsystem.h"
#include "Scenario/Editor/ScenarioEditorController.h"
#include "Scenario/Llm/ScenarioLlmAuthoringSubsystem.h"
#include "Scenario/ViewModel/ScenarioAssetPaletteViewModel.h"
#include "Scenario/ViewModel/ScenarioEditorOutlinerViewModel.h"
#include "Scenario/ViewModel/ScenarioEditorShellViewModel.h"
#include "Scenario/ViewModel/ScenarioEditorToolbarViewModel.h"
#include "Scenario/ViewModel/ScenarioLlmPromptViewModel.h"
#include "Scenario/ViewModel/ScenarioPlaceableDetailsViewModel.h"
#include "Scenario/ViewModel/ScenarioTemplateSidebarViewModel.h"
#include "Components/Widget.h"
#include "Shared/ScenarioCoreTypes.h"

namespace
{
	const TCHAR* ScenarioEditorUiProjectScenarioFileName = TEXT("scenario.json");

	// Project-relative source for the presentation demo scenario.
	const TCHAR* ScenarioEditorUiDemoScenarioJsonPath = TEXT("Json/demo-scenario.json");

	FString ResolveScenarioEditorUiProjectScenarioJsonPath(FString rawPath)
	{
		rawPath.TrimStartAndEndInline();
		rawPath.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (rawPath.IsEmpty())
		{
			return FString();
		}

		if (FPaths::GetExtension(rawPath).IsEmpty())
		{
			rawPath = FPaths::Combine(rawPath, ScenarioEditorUiProjectScenarioFileName);
		}
		if (FPaths::IsRelative(rawPath))
		{
			rawPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), rawPath);
		}
		FPaths::NormalizeFilename(rawPath);
		return rawPath;
	}

	bool IsScenarioEditorUiProjectScenarioJsonPath(const FString& scenarioJsonPath)
	{
		return FPaths::GetCleanFilename(scenarioJsonPath).Equals(
			ScenarioEditorUiProjectScenarioFileName,
			ESearchCase::IgnoreCase);
	}

	// Resolves the bundled demo scenario used by presentation-only LLM load flow.
	FString ResolveScenarioEditorUiDemoScenarioJsonPath()
	{
		FString demoScenarioJsonPath = FPaths::Combine(FPaths::ProjectDir(), ScenarioEditorUiDemoScenarioJsonPath);
		FPaths::NormalizeFilename(demoScenarioJsonPath);
		return demoScenarioJsonPath;
	}

	// 기존 저장 파일을 덮어쓰지 않는 기본 scenario.json 저장 후보를 만든다.
	FString MakeUniqueScenarioEditorUiSavePath(const FString& preferredPath)
	{
		FString directory = FPaths::GetPath(preferredPath);
		if (directory.IsEmpty())
		{
			directory = TEXT("Saved/UserProjects/ScenarioEditor");
		}

		const FString baseName = FPaths::GetBaseFilename(preferredPath).IsEmpty()
			? FString(TEXT("scenario"))
			: FPaths::GetBaseFilename(preferredPath);
		const FString extension = FPaths::GetExtension(preferredPath).IsEmpty()
			? FString(TEXT("json"))
			: FPaths::GetExtension(preferredPath);

		for (int32 index = 0; index < 1000; ++index)
		{
			const FString fileName = index == 0
				? FString::Printf(TEXT("%s.%s"), *baseName, *extension)
				: FString::Printf(TEXT("%s_%d.%s"), *baseName, index, *extension);
			FString candidatePath = FPaths::Combine(directory, fileName);
			candidatePath.ReplaceInline(TEXT("\\"), TEXT("/"));
			const FString resolvedCandidatePath = FPaths::IsRelative(candidatePath)
				? FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), candidatePath)
				: FPaths::ConvertRelativePathToFull(candidatePath);
			if (!FPaths::FileExists(resolvedCandidatePath))
			{
				return candidatePath;
			}
		}

		FString fallbackPath = FPaths::Combine(
			directory,
			FString::Printf(
				TEXT("%s_%s.%s"),
				*baseName,
				*FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8),
				*extension));
		fallbackPath.ReplaceInline(TEXT("\\"), TEXT("/"));
		return fallbackPath;
	}
}

void UScenarioEditorUiSubsystem::Initialize(FSubsystemCollectionBase& collection)
{
	Super::Initialize(collection);

	ShellViewModel = NewObject<UScenarioEditorShellViewModel>(this);
	ToolbarViewModel = NewObject<UScenarioEditorToolbarViewModel>(this);
	OutlinerViewModel = NewObject<UScenarioEditorOutlinerViewModel>(this);
	AssetPaletteViewModel = NewObject<UScenarioAssetPaletteViewModel>(this);
	PlaceableDetailsViewModel = NewObject<UScenarioPlaceableDetailsViewModel>(this);
	LlmPromptViewModel = NewObject<UScenarioLlmPromptViewModel>(this);
	TemplateSidebarViewModel = NewObject<UScenarioTemplateSidebarViewModel>(this);

	if (ShellViewModel)
	{
		ShellViewModel->InitializeForSubsystem(this);
	}
	if (ToolbarViewModel)
	{
		ToolbarViewModel->InitializeForSubsystem(this);
	}
	if (OutlinerViewModel)
	{
		OutlinerViewModel->InitializeForSubsystem(this);
	}
	if (AssetPaletteViewModel)
	{
		AssetPaletteViewModel->InitializeForSubsystem(this);
	}
	if (PlaceableDetailsViewModel)
	{
		PlaceableDetailsViewModel->InitializeForSubsystem(this);
	}
	if (LlmPromptViewModel)
	{
		LlmPromptViewModel->InitializeForSubsystem(this);
	}
	if (TemplateSidebarViewModel)
	{
		TemplateSidebarViewModel->InitializeForSubsystem(this);
	}

	if (UScenarioEditorLaunchSubsystem* launchSubsystem = ResolveScenarioEditorLaunchSubsystem())
	{
		launchSubsystem->OnAutoStartCompleted().RemoveAll(this);
		EditorAutoStartCompletedHandle = launchSubsystem->OnAutoStartCompleted().AddUObject(
			this,
			&UScenarioEditorUiSubsystem::HandleEditorAutoStartCompleted);
	}

	RefreshFromEditorState();
}

void UScenarioEditorUiSubsystem::Deinitialize()
{
	if (EditorAutoStartCompletedHandle.IsValid())
	{
		if (UScenarioEditorLaunchSubsystem* launchSubsystem = ResolveScenarioEditorLaunchSubsystem())
		{
			launchSubsystem->OnAutoStartCompleted().Remove(EditorAutoStartCompletedHandle);
		}
		EditorAutoStartCompletedHandle.Reset();
	}

	ShellViewModel = nullptr;
	ToolbarViewModel = nullptr;
	OutlinerViewModel = nullptr;
	AssetPaletteViewModel = nullptr;
	PlaceableDetailsViewModel = nullptr;
	LlmPromptViewModel = nullptr;
	TemplateSidebarViewModel = nullptr;

	Super::Deinitialize();
}

UScenarioEditorUiSubsystem* UScenarioEditorUiSubsystem::ResolveForWorldContext(const UObject* worldContextObject)
{
	UWorld* world = worldContextObject ? worldContextObject->GetWorld() : nullptr;
	return world ? world->GetSubsystem<UScenarioEditorUiSubsystem>() : nullptr;
}

bool UScenarioEditorUiSubsystem::HasAutoStartedScenarioEditorSession() const
{
	const UScenarioEditorLaunchSubsystem* launchSubsystem = ResolveScenarioEditorLaunchSubsystem();
	return launchSubsystem && launchSubsystem->HasAutoStartedScenarioEditorSession();
}

bool UScenarioEditorUiSubsystem::WasAutoStartedScenarioEditorSessionLoadedExistingScenario() const
{
	const UScenarioEditorLaunchSubsystem* launchSubsystem = ResolveScenarioEditorLaunchSubsystem();
	return launchSubsystem && launchSubsystem->WasAutoStartedScenarioEditorSessionLoadedExistingScenario();
}

void UScenarioEditorUiSubsystem::RefreshFromEditorState()
{
	if (ShellViewModel)
	{
		ShellViewModel->RefreshFromController();
	}
	if (OutlinerViewModel)
	{
		OutlinerViewModel->RefreshTemplatePanels();
	}
	if (TemplateSidebarViewModel && ShellViewModel)
	{
		TemplateSidebarViewModel->SelectPanel(ShellViewModel->GetActiveSidebarPanel());
	}
}

void UScenarioEditorUiSubsystem::RefreshEditorRootInspector()
{
	RefreshFromEditorState();
	if (AScenarioEditorController* editorController = ResolveEditorController())
	{
		if (UScenarioEditorRootWidget* rootWidget = editorController->GetEditorRootWidget())
		{
			rootWidget->RefreshScenarioInspectorWithOutlinerRegistryRebuild();
		}
	}
}

bool UScenarioEditorUiSubsystem::SaveScenario(
	const FString& defaultSavePath,
	FString& outResolvedPath,
	TArray<FString>& outDiagnostics) const
{
	outResolvedPath.Reset();
	outDiagnostics.Reset();

	AScenarioEditorController* editorController = ResolveEditorController();
	if (!editorController)
	{
		outDiagnostics.Add(TEXT("ScenarioEditorController unavailable."));
		return false;
	}

	const FString savePath = ResolveSavePath(defaultSavePath);
	return editorController->SaveProjectScenarioJsonFile(savePath, outResolvedPath, outDiagnostics);
}

bool UScenarioEditorUiSubsystem::RequestScenarioGeneration(
	const FString& prompt,
	const FString& projectScenarioJsonPath,
	const int32 episodeCount,
	FString& outFailureReason) const
{
	outFailureReason.Reset();

	UScenarioLlmAuthoringSubsystem* llmSubsystem = ResolveLlmAuthoringSubsystem();
	if (!llmSubsystem)
	{
		outFailureReason = TEXT("Scenario LLM subsystem unavailable.");
		return false;
	}

	if (!llmSubsystem->GenerateProjectScenarioFromPrompt(prompt, projectScenarioJsonPath, episodeCount))
	{
		outFailureReason = TEXT("Scenario generation request rejected.");
		return false;
	}

	return true;
}

bool UScenarioEditorUiSubsystem::BindScenarioGenerationCompleted(UObject* listener, const FName functionName) const
{
	if (!listener || functionName.IsNone())
	{
		return false;
	}

	UScenarioLlmAuthoringSubsystem* llmSubsystem = ResolveLlmAuthoringSubsystem();
	if (!llmSubsystem)
	{
		return false;
	}

	FScriptDelegate delegate;
	delegate.BindUFunction(listener, functionName);
	llmSubsystem->OnGenerationCompleted.Remove(delegate);
	llmSubsystem->OnGenerationCompleted.Add(delegate);
	return true;
}

void UScenarioEditorUiSubsystem::UnbindScenarioGenerationCompleted(UObject* listener) const
{
	if (listener)
	{
		if (UScenarioLlmAuthoringSubsystem* llmSubsystem = ResolveLlmAuthoringSubsystem())
		{
			llmSubsystem->OnGenerationCompleted.RemoveAll(listener);
		}
	}
}

int32 UScenarioEditorUiSubsystem::GetDefaultScenarioGenerationEpisodeCount() const
{
	const UScenarioLlmAuthoringSubsystem* llmSubsystem = ResolveLlmAuthoringSubsystem();
	return llmSubsystem ? FMath::Max(1, llmSubsystem->DefaultEpisodeCount) : 1;
}

FString UScenarioEditorUiSubsystem::GetLatestGeneratedProjectScenarioPath() const
{
	const UScenarioLlmAuthoringSubsystem* llmSubsystem = ResolveLlmAuthoringSubsystem();
	if (!llmSubsystem)
	{
		return FString();
	}

	const FScenarioLlmGenerationResult result = llmSubsystem->GetLatestResult();
	if (!result.bSuccess || result.ProjectScenarioJsonPath.IsEmpty())
	{
		return FString();
	}

	const FString scenarioJsonPath = ResolveScenarioEditorUiProjectScenarioJsonPath(result.ProjectScenarioJsonPath);
	return IsScenarioEditorUiProjectScenarioJsonPath(scenarioJsonPath) ? scenarioJsonPath : FString();
}

bool UScenarioEditorUiSubsystem::ResolveCurrentProjectScenarioPath(
	FString& outScenarioJsonPath,
	FString& outProjectPath,
	FString& outFailureReason) const
{
	outScenarioJsonPath.Reset();
	outProjectPath.Reset();
	outFailureReason.Reset();

	const AScenarioEditorController* editorController = ResolveEditorController();
	if (!editorController)
	{
		outFailureReason = TEXT("시나리오 에디터 컨트롤러를 찾을 수 없습니다.");
		return false;
	}

	outScenarioJsonPath = ResolveScenarioEditorUiProjectScenarioJsonPath(
		editorController->GetSourceProjectScenarioJsonPath());
	if (!IsScenarioEditorUiProjectScenarioJsonPath(outScenarioJsonPath))
	{
		outFailureReason = TEXT("생성, 불러오기, 실행 기능은 에디터 소스가 <UserProject>/scenario.json일 때 사용할 수 있습니다.");
		return false;
	}

	outProjectPath = FPaths::GetPath(outScenarioJsonPath);
	if (outProjectPath.IsEmpty())
	{
		outFailureReason = TEXT("scenario.json에서 프로젝트 경로를 확인하지 못했습니다.");
		return false;
	}

	return true;
}

bool UScenarioEditorUiSubsystem::LoadLatestGeneratedProjectScenario(FString& outStatusText) const
{
	outStatusText.Reset();

	AScenarioEditorController* editorController = ResolveEditorController();
	if (!editorController)
	{
		outStatusText = TEXT("시나리오 에디터 컨트롤러를 찾을 수 없습니다.");
		return false;
	}

	FString scenarioJsonPath = GetLatestGeneratedProjectScenarioPath();
	FString projectPath;
	if (scenarioJsonPath.IsEmpty())
	{
		FString failureReason;
		if (!ResolveCurrentProjectScenarioPath(scenarioJsonPath, projectPath, failureReason))
		{
			outStatusText = failureReason;
			return false;
		}
	}

	FString resolvedJsonFilePath;
	TArray<FString> diagnostics;
	if (!editorController->LoadProjectScenarioJsonFile(scenarioJsonPath, resolvedJsonFilePath, diagnostics))
	{
		outStatusText = diagnostics.IsEmpty()
			? FString::Printf(TEXT("scenario.json을 불러오지 못했습니다: %s"), *scenarioJsonPath)
			: FString::Printf(TEXT("scenario.json을 불러오지 못했습니다:\n%s"), *FString::Join(diagnostics, TEXT("\n")));
		return false;
	}

	editorController->RequestMoveEditorViewToPlayerStart();
	outStatusText = TEXT("프로젝트 시나리오를 불러왔습니다.");
	return true;
}

bool UScenarioEditorUiSubsystem::LoadDemoProjectScenario(FString& outStatusText) const
{
	outStatusText.Reset();

	AScenarioEditorController* editorController = ResolveEditorController();
	if (!editorController)
	{
		outStatusText = TEXT("시나리오 에디터 컨트롤러를 찾을 수 없습니다.");
		return false;
	}

	FString scenarioJsonPath;
	FString projectPath;
	FString failureReason;
	if (!ResolveCurrentProjectScenarioPath(scenarioJsonPath, projectPath, failureReason))
	{
		outStatusText = failureReason;
		return false;
	}

	const FString demoScenarioJsonPath = ResolveScenarioEditorUiDemoScenarioJsonPath();
	FString demoScenarioJson;
	if (!FFileHelper::LoadFileToString(demoScenarioJson, *demoScenarioJsonPath))
	{
		outStatusText = FString::Printf(TEXT("시연용 scenario.json을 읽지 못했습니다: %s"), *demoScenarioJsonPath);
		return false;
	}

	const FString scenarioJsonDirectory = FPaths::GetPath(scenarioJsonPath);
	if (!scenarioJsonDirectory.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*scenarioJsonDirectory, true);
	}

	if (!FFileHelper::SaveStringToFile(
			demoScenarioJson,
			*scenarioJsonPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		outStatusText = FString::Printf(TEXT("프로젝트 scenario.json에 시나리오를 저장하지 못했습니다: %s"), *scenarioJsonPath);
		return false;
	}

	FString resolvedJsonFilePath;
	TArray<FString> diagnostics;
	if (!editorController->LoadProjectScenarioJsonFile(scenarioJsonPath, resolvedJsonFilePath, diagnostics))
	{
		outStatusText = diagnostics.IsEmpty()
			? FString::Printf(TEXT("scenario.json을 불러오지 못했습니다: %s"), *scenarioJsonPath)
			: FString::Printf(TEXT("scenario.json을 불러오지 못했습니다:\n%s"), *FString::Join(diagnostics, TEXT("\n")));
		return false;
	}

	editorController->RequestMoveEditorViewToPlayerStart();
	outStatusText = TEXT("시나리오를 적용했습니다.");
	return true;
}

bool UScenarioEditorUiSubsystem::RunCurrentProjectScenario(FString& outStatusText) const
{
	outStatusText.Reset();

	USimulatorLaunchSubsystem* launchSubsystem = ResolveSimulatorLaunchSubsystem();
	if (!launchSubsystem)
	{
		outStatusText = TEXT("시뮬레이터 실행 서브시스템을 찾을 수 없습니다.");
		return false;
	}

	FString scenarioJsonPath;
	FString projectPath;
	if (!SaveCurrentProjectScenario(scenarioJsonPath, projectPath, outStatusText))
	{
		return false;
	}

	FString runId;
	TArray<FString> diagnostics;
	if (!launchSubsystem->PrepareProjectRunSnapshot(projectPath, FString(), runId, diagnostics))
	{
		outStatusText = diagnostics.IsEmpty()
			? FString::Printf(TEXT("프로젝트 실행 스냅샷을 준비하지 못했습니다: %s"), *projectPath)
			: FString::Printf(TEXT("프로젝트 실행 스냅샷을 준비하지 못했습니다:\n%s"), *FString::Join(diagnostics, TEXT("\n")));
		return false;
	}

	if (!launchSubsystem->StartProjectRun(projectPath, runId))
	{
		const FString lastError = launchSubsystem->GetLastError();
		outStatusText = lastError.IsEmpty()
			? TEXT("시뮬레이션 실행을 시작하지 못했습니다.")
			: FString::Printf(TEXT("시뮬레이션 실행을 시작하지 못했습니다:\n%s"), *lastError);
		return false;
	}

	outStatusText = TEXT("프로젝트 실행을 요청했습니다.");
	return true;
}

bool UScenarioEditorUiSubsystem::ReturnToStartup(const FString& startupMapId) const
{
	const FString trimmedMapId = startupMapId.TrimStartAndEnd();
	if (trimmedMapId.IsEmpty())
	{
		return false;
	}

	UWorld* world = GetWorld();
	if (!world)
	{
		return false;
	}

	UGameplayStatics::OpenLevel(world, FName(*trimmedMapId));
	return true;
}

void UScenarioEditorUiSubsystem::RequestEditorWidgetInputMode(UWidget* requestingWidget) const
{
	if (!requestingWidget)
	{
		return;
	}

	if (AScenarioEditorController* editorController = ResolveEditorController())
	{
		editorController->RequestEditorWidgetInputMode(requestingWidget);
	}
}

void UScenarioEditorUiSubsystem::ReleaseEditorWidgetInputMode(UWidget* requestingWidget) const
{
	if (!requestingWidget)
	{
		return;
	}

	if (AScenarioEditorController* editorController = ResolveEditorController())
	{
		editorController->ReleaseEditorWidgetInputMode(requestingWidget);
	}
}

bool UScenarioEditorUiSubsystem::SetEditorViewMode(const EScenarioEditorViewMode viewMode) const
{
	AScenarioEditorController* editorController = ResolveEditorController();
	if (!editorController)
	{
		return false;
	}

	editorController->SetEditorViewMode(viewMode);
	return true;
}

EScenarioEditorViewMode UScenarioEditorUiSubsystem::GetEditorViewMode() const
{
	const AScenarioEditorController* editorController = ResolveEditorController();
	return editorController
		? editorController->GetEditorViewMode()
		: EScenarioEditorViewMode::Perspective;
}

bool UScenarioEditorUiSubsystem::SetTransformGizmoMode(const EScenarioTransformGizmoMode mode) const
{
	AScenarioEditorController* editorController = ResolveEditorController();
	if (!editorController)
	{
		return false;
	}

	editorController->SetTransformGizmoMode(mode);
	return true;
}

EScenarioTransformGizmoMode UScenarioEditorUiSubsystem::GetTransformGizmoMode() const
{
	const AScenarioEditorController* editorController = ResolveEditorController();
	return editorController
		? editorController->GetTransformGizmoMode()
		: EScenarioTransformGizmoMode::Translate;
}

bool UScenarioEditorUiSubsystem::SetTransformGizmoOrientationMode(
	const EScenarioTransformGizmoOrientationMode orientationMode) const
{
	AScenarioEditorController* editorController = ResolveEditorController();
	if (!editorController)
	{
		return false;
	}

	editorController->SetTransformGizmoOrientationMode(orientationMode);
	return true;
}

EScenarioTransformGizmoOrientationMode UScenarioEditorUiSubsystem::GetTransformGizmoOrientationMode() const
{
	const AScenarioEditorController* editorController = ResolveEditorController();
	return editorController
		? editorController->GetTransformGizmoOrientationMode()
		: EScenarioTransformGizmoOrientationMode::World;
}

bool UScenarioEditorUiSubsystem::CanEditTransformGizmoOrientationForSelection() const
{
	const AScenarioEditorController* editorController = ResolveEditorController();
	return editorController && editorController->CanEditTransformGizmoOrientationForSelection();
}

EScenarioTransformGizmoOrientationMode UScenarioEditorUiSubsystem::GetEffectiveTransformGizmoOrientationMode() const
{
	const AScenarioEditorController* editorController = ResolveEditorController();
	return editorController
		? editorController->GetEffectiveTransformGizmoOrientationMode()
		: EScenarioTransformGizmoOrientationMode::World;
}

bool UScenarioEditorUiSubsystem::DeleteSelectedPlaceable(FString& outFailureReason) const
{
	outFailureReason.Reset();

	AScenarioEditorController* editorController = ResolveEditorController();
	if (!editorController)
	{
		outFailureReason = TEXT("ScenarioEditorController unavailable.");
		return false;
	}

	return editorController->DeleteSelectedPlaceable(outFailureReason);
}

bool UScenarioEditorUiSubsystem::RenameSelectedPlaceableInstanceId(
	const FString& instanceId,
	FString& outFailureReason) const
{
	outFailureReason.Reset();

	AScenarioEditorController* editorController = ResolveEditorController();
	if (!editorController)
	{
		outFailureReason = TEXT("ScenarioEditorController unavailable.");
		return false;
	}

	return editorController->TryRenameSelectedPlaceableInstanceId(instanceId.TrimStartAndEnd(), outFailureReason);
}

bool UScenarioEditorUiSubsystem::UpdateSelectedPlaceableTransform(
	const FTransform& transform,
	FString& outFailureReason) const
{
	outFailureReason.Reset();

	AScenarioEditorController* editorController = ResolveEditorController();
	if (!editorController)
	{
		outFailureReason = TEXT("ScenarioEditorController unavailable.");
		return false;
	}

	return editorController->TryUpdateSelectedPlaceableTransform(transform, outFailureReason);
}

void UScenarioEditorUiSubsystem::GetStaticObstaclePaletteEntries(
	TArray<FScenarioStaticObstaclePropEntry>& outEntries) const
{
	outEntries.Reset();
	if (const AScenarioEditorController* editorController = ResolveEditorController())
	{
		editorController->GetStaticObstaclePaletteEntries(outEntries);
	}
}

bool UScenarioEditorUiSubsystem::BeginPalettePlacement(
	const EScenarioPaletteItemType itemType,
	const FName assetId) const
{
	AScenarioEditorController* editorController = ResolveEditorController();
	return editorController && editorController->BeginPalettePlacement(itemType, assetId);
}

bool UScenarioEditorUiSubsystem::BeginGroundRegionDraw(
	const EScenarioGroundRegionType regionType) const
{
	AScenarioEditorController* editorController = ResolveEditorController();
	return editorController && editorController->BeginGroundRegionDraw(regionType);
}

AScenarioEditorController* UScenarioEditorUiSubsystem::ResolveEditorController() const
{
	UWorld* world = GetWorld();
	return world ? Cast<AScenarioEditorController>(world->GetFirstPlayerController()) : nullptr;
}

UScenarioAuthoringSubsystem* UScenarioEditorUiSubsystem::ResolveAuthoringSubsystem() const
{
	UWorld* world = GetWorld();
	return world ? world->GetSubsystem<UScenarioAuthoringSubsystem>() : nullptr;
}

FString UScenarioEditorUiSubsystem::ResolveSavePath(const FString& defaultSavePath) const
{
	if (const AScenarioEditorController* editorController = ResolveEditorController())
	{
		const FString sourcePath = editorController->GetSourceProjectScenarioJsonPath();
		if (!sourcePath.IsEmpty())
		{
			return sourcePath;
		}
	}

	return MakeUniqueScenarioEditorUiSavePath(defaultSavePath);
}

UScenarioLlmAuthoringSubsystem* UScenarioEditorUiSubsystem::ResolveLlmAuthoringSubsystem() const
{
	UWorld* world = GetWorld();
	UGameInstance* gameInstance = world ? world->GetGameInstance() : nullptr;
	return gameInstance ? gameInstance->GetSubsystem<UScenarioLlmAuthoringSubsystem>() : nullptr;
}

USimulatorLaunchSubsystem* UScenarioEditorUiSubsystem::ResolveSimulatorLaunchSubsystem() const
{
	UWorld* world = GetWorld();
	UGameInstance* gameInstance = world ? world->GetGameInstance() : nullptr;
	return gameInstance ? gameInstance->GetSubsystem<USimulatorLaunchSubsystem>() : nullptr;
}

UScenarioEditorLaunchSubsystem* UScenarioEditorUiSubsystem::ResolveScenarioEditorLaunchSubsystem() const
{
	UWorld* world = GetWorld();
	UGameInstance* gameInstance = world ? world->GetGameInstance() : nullptr;
	return gameInstance ? gameInstance->GetSubsystem<UScenarioEditorLaunchSubsystem>() : nullptr;
}

void UScenarioEditorUiSubsystem::HandleEditorAutoStartCompleted(const bool bLoadedExistingScenario)
{
	EditorAutoStartCompletedEvent.Broadcast(bLoadedExistingScenario);
}

bool UScenarioEditorUiSubsystem::SaveCurrentProjectScenario(
	FString& outScenarioJsonPath,
	FString& outProjectPath,
	FString& outStatusText) const
{
	outScenarioJsonPath.Reset();
	outProjectPath.Reset();
	outStatusText.Reset();

	AScenarioEditorController* editorController = ResolveEditorController();
	if (!editorController)
	{
		outStatusText = TEXT("시나리오 에디터 컨트롤러를 찾을 수 없습니다.");
		return false;
	}

	FString failureReason;
	if (!ResolveCurrentProjectScenarioPath(outScenarioJsonPath, outProjectPath, failureReason))
	{
		outStatusText = failureReason;
		return false;
	}

	FString resolvedJsonFilePath;
	TArray<FString> diagnostics;
	if (!editorController->SaveProjectScenarioJsonFile(outScenarioJsonPath, resolvedJsonFilePath, diagnostics))
	{
		outStatusText = diagnostics.IsEmpty()
			? FString::Printf(TEXT("scenario.json을 저장하지 못했습니다: %s"), *outScenarioJsonPath)
			: FString::Printf(TEXT("scenario.json을 저장하지 못했습니다:\n%s"), *FString::Join(diagnostics, TEXT("\n")));
		return false;
	}

	outScenarioJsonPath = ResolveScenarioEditorUiProjectScenarioJsonPath(resolvedJsonFilePath);
	outProjectPath = FPaths::GetPath(outScenarioJsonPath);
	return true;
}
