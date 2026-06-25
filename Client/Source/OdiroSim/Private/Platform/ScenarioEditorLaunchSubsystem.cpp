
#include "Platform/ScenarioEditorLaunchSubsystem.h"
#include "Scenario/Editor/ScenarioEditorController.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Kismet/GameplayStatics.h"
#include "Shared/ScenarioViewportPresentation.h"

DEFINE_LOG_CATEGORY_STATIC(LogScenarioEditorLaunch, Log, All);

namespace
{
	FString NormalizeMapIdForOpenLevel(const FString& mapId)
	{
		FString normalizedMapId = mapId.TrimStartAndEnd();
		if (normalizedMapId.IsEmpty())
		{
			normalizedMapId = TEXT("ScenarioEditorMap");
		}

		int32 objectNameSeparatorIndex = INDEX_NONE;
		if (normalizedMapId.FindChar(TEXT('.'), objectNameSeparatorIndex))
		{
			normalizedMapId = normalizedMapId.Left(objectNameSeparatorIndex);
		}

		return normalizedMapId;
	}
}

UScenarioEditorLaunchSubsystem::UScenarioEditorLaunchSubsystem()
{
	ScenarioEditorPreloadAssets = FScenarioViewportPresentation::MakeScenarioMapPreloadAssets();
}

void UScenarioEditorLaunchSubsystem::Initialize(FSubsystemCollectionBase& collection)
{
	Super::Initialize(collection);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this,
		&UScenarioEditorLaunchSubsystem::HandlePostLoadMapWithWorld);
}

void UScenarioEditorLaunchSubsystem::Deinitialize()
{
	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}

	if (ScenarioEditorPreloadHandle.IsValid() && ScenarioEditorPreloadHandle->IsLoadingInProgress())
	{
		ScenarioEditorPreloadHandle->CancelHandle();
	}
	ScenarioEditorPreloadHandle.Reset();
	PendingScenarioEditorOpenLevelOptions.Reset();
	LoadedScenarioEditorPreloadAssets.Reset();

	Super::Deinitialize();
}

bool UScenarioEditorLaunchSubsystem::OpenScenarioEditor(const FString& scenarioSetupPath)
{
	const FString trimmedScenarioSetupPath = scenarioSetupPath.TrimStartAndEnd();
	if (trimmedScenarioSetupPath.IsEmpty())
	{
		return OpenNewScenarioEditor();
	}

	return OpenScenarioEditorInternal(EScenarioEditorAutoStartMode::LoadFromPath, trimmedScenarioSetupPath);
}

bool UScenarioEditorLaunchSubsystem::OpenScenarioEditorMap()
{
	UWorld* world = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!world)
	{
		UE_LOG(LogScenarioEditorLaunch, Warning, TEXT("ScenarioEditorMap 열기 실패: World 없음"));
		return false;
	}

	ResetPendingAutoStartState();
	bAutoStartedScenarioEditorSession = false;
	bAutoStartedScenarioEditorSessionLoadedExistingScenario = false;

	RequestScenarioEditorPreload(FString());
	return true;
}

bool UScenarioEditorLaunchSubsystem::OpenNewScenarioEditor()
{
	return OpenScenarioEditorInternal(EScenarioEditorAutoStartMode::NewDraft, FString());
}

bool UScenarioEditorLaunchSubsystem::OpenNewScenarioEditorAtPath(const FString& scenarioJsonPath)
{
	return OpenScenarioEditorInternal(EScenarioEditorAutoStartMode::NewDraft, scenarioJsonPath.TrimStartAndEnd());
}

bool UScenarioEditorLaunchSubsystem::OpenScenarioEditorInternal(
	const EScenarioEditorAutoStartMode launchMode,
	const FString& scenarioSetupPath)
{
	UWorld* world = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!world)
	{
		UE_LOG(LogScenarioEditorLaunch, Warning, TEXT("ScenarioEditorMap 열기 실패: World 없음"));
		return false;
	}

	PendingAutoStartMode = launchMode;
	PendingScenarioSetupPath = scenarioSetupPath.TrimStartAndEnd();
	bAutoStartedScenarioEditorSession = false;
	bAutoStartedScenarioEditorSessionLoadedExistingScenario = false;

	const FString openLevelName = NormalizeMapIdForOpenLevel(ScenarioEditorMapId);
	const FString openLevelOptions = launchMode == EScenarioEditorAutoStartMode::LoadFromPath
		? FString::Printf(TEXT("Scenario=%s"), *PendingScenarioSetupPath)
		: TEXT("NewScenario=1");

	// The URL options keep the transition inspectable in logs/console, while the subsystem state is the authoritative
	// startup request that survives the world replacement.
	UE_LOG(
		LogScenarioEditorLaunch,
		Log,
		TEXT("ScenarioEditorMap 열기 요청 | Map: %s, Options: %s"),
		*openLevelName,
		*openLevelOptions);
	RequestScenarioEditorPreload(openLevelOptions);
	return true;
}

void UScenarioEditorLaunchSubsystem::ClearPendingScenarioSetupPath()
{
	ResetPendingAutoStartState();
}

void UScenarioEditorLaunchSubsystem::ResetPendingAutoStartState()
{
	PendingScenarioSetupPath.Reset();
	PendingAutoStartMode = EScenarioEditorAutoStartMode::None;
}

bool UScenarioEditorLaunchSubsystem::OpenScenarioEditorMapAfterPreload(const FString& openLevelOptions)
{
	UWorld* world = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!world)
	{
		UE_LOG(LogScenarioEditorLaunch, Warning, TEXT("ScenarioEditorMap 열기 실패: World 없음"));
		return false;
	}

	const FString openLevelName = NormalizeMapIdForOpenLevel(ScenarioEditorMapId);
	UE_LOG(
		LogScenarioEditorLaunch,
		Log,
		TEXT("ScenarioEditorMap 프리로드 완료 후 OpenLevel | Map: %s, Options: %s"),
		*openLevelName,
		openLevelOptions.IsEmpty() ? TEXT("<none>") : *openLevelOptions);
	UGameplayStatics::OpenLevel(world, FName(*openLevelName), true, openLevelOptions);
	return true;
}

void UScenarioEditorLaunchSubsystem::RequestScenarioEditorPreload(const FString& openLevelOptions)
{
	PendingScenarioEditorOpenLevelOptions = openLevelOptions;

	TArray<FSoftObjectPath> preloadAssetPaths;
	for (const TSoftObjectPtr<UObject>& preloadAsset : ScenarioEditorPreloadAssets)
	{
		const FSoftObjectPath preloadAssetPath = preloadAsset.ToSoftObjectPath();
		if (!preloadAssetPath.IsNull())
		{
			preloadAssetPaths.AddUnique(preloadAssetPath);
		}
	}

	if (preloadAssetPaths.IsEmpty())
	{
		LoadedScenarioEditorPreloadAssets.Reset();
		OpenScenarioEditorMapAfterPreload(PendingScenarioEditorOpenLevelOptions);
		PendingScenarioEditorOpenLevelOptions.Reset();
		return;
	}

	if (ScenarioEditorPreloadHandle.IsValid() && ScenarioEditorPreloadHandle->IsLoadingInProgress())
	{
		UE_LOG(
			LogScenarioEditorLaunch,
			Log,
			TEXT("ScenarioEditor visual asset preload already in progress | Count: %d"),
			preloadAssetPaths.Num());
		return;
	}

	FStreamableManager& streamableManager = UAssetManager::GetStreamableManager();
	ScenarioEditorPreloadHandle = streamableManager.RequestAsyncLoad(
		MoveTemp(preloadAssetPaths),
		FStreamableDelegate::CreateUObject(
			this,
			&UScenarioEditorLaunchSubsystem::HandleScenarioEditorPreloadComplete),
		FStreamableManager::DefaultAsyncLoadPriority,
		false,
		false,
		TEXT("ScenarioEditorVisualPreload"));

	if (!ScenarioEditorPreloadHandle.IsValid())
	{
		UE_LOG(LogScenarioEditorLaunch, Warning, TEXT("ScenarioEditor visual asset preload request failed."));
		OpenScenarioEditorMapAfterPreload(PendingScenarioEditorOpenLevelOptions);
		PendingScenarioEditorOpenLevelOptions.Reset();
	}
}

void UScenarioEditorLaunchSubsystem::HandleScenarioEditorPreloadComplete()
{
	CacheLoadedScenarioEditorPreloadAssets();
	ScenarioEditorPreloadHandle.Reset();

	const FString openLevelOptions = PendingScenarioEditorOpenLevelOptions;
	PendingScenarioEditorOpenLevelOptions.Reset();
	OpenScenarioEditorMapAfterPreload(openLevelOptions);
}

void UScenarioEditorLaunchSubsystem::CacheLoadedScenarioEditorPreloadAssets()
{
	LoadedScenarioEditorPreloadAssets.Reset();
	for (const TSoftObjectPtr<UObject>& preloadAsset : ScenarioEditorPreloadAssets)
	{
		if (UObject* loadedAsset = preloadAsset.Get())
		{
			LoadedScenarioEditorPreloadAssets.Add(loadedAsset);
			continue;
		}

		const FSoftObjectPath preloadAssetPath = preloadAsset.ToSoftObjectPath();
		if (!preloadAssetPath.IsNull())
		{
			UE_LOG(
				LogScenarioEditorLaunch,
				Warning,
				TEXT("ScenarioEditor visual asset preload missing asset | Path: %s"),
				*preloadAssetPath.ToString());
		}
	}

	UE_LOG(
		LogScenarioEditorLaunch,
		Log,
		TEXT("ScenarioEditor visual asset preload complete | Loaded: %d / Configured: %d"),
		LoadedScenarioEditorPreloadAssets.Num(),
		ScenarioEditorPreloadAssets.Num());
}

void UScenarioEditorLaunchSubsystem::HandlePostLoadMapWithWorld(UWorld* loadedWorld)
{
	if (!loadedWorld || !DoesWorldMatchMapId(loadedWorld, ScenarioEditorMapId))
	{
		return;
	}

	// Defer one tick so the scenario editor controller and its startup widgets have completed their own construction.
	loadedWorld->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(
			this,
			&UScenarioEditorLaunchSubsystem::TryApplyPendingEditorStartup,
			loadedWorld));
}

void UScenarioEditorLaunchSubsystem::TryApplyPendingEditorStartup(UWorld* loadedWorld)
{
	if (!loadedWorld || PendingAutoStartMode == EScenarioEditorAutoStartMode::None)
	{
		return;
	}

	AScenarioEditorController* editorController = Cast<AScenarioEditorController>(
		UGameplayStatics::GetPlayerController(loadedWorld, 0));
	if (!editorController)
	{
		UE_LOG(
			LogScenarioEditorLaunch,
			Warning,
			TEXT("ScenarioEditor 자동 시작 보류: ScenarioEditorController 없음 | Mode: %d, Path: %s"),
			static_cast<int32>(PendingAutoStartMode),
			PendingScenarioSetupPath.IsEmpty() ? TEXT("<new draft>") : *PendingScenarioSetupPath);
		return;
	}

	bool bLoadedExistingScenario = false;
	if (PendingAutoStartMode == EScenarioEditorAutoStartMode::NewDraft)
	{
		editorController->NewScenarioDraft();
		if (!PendingScenarioSetupPath.IsEmpty())
		{
			FString resolvedPath;
			TArray<FString> diagnostics;
			if (!editorController->SaveProjectScenarioJsonFile(PendingScenarioSetupPath, resolvedPath, diagnostics))
			{
				UE_LOG(
					LogScenarioEditorLaunch,
					Warning,
					TEXT("Project scenario 새 draft 저장 실패 | Path: %s, Diagnostics: %s"),
					*PendingScenarioSetupPath,
					*FString::Join(diagnostics, TEXT(" | ")));
				return;
			}

			UE_LOG(LogScenarioEditorLaunch, Log, TEXT("Project scenario 새 draft 저장 완료 | Path: %s"), *resolvedPath);
		}
		UE_LOG(LogScenarioEditorLaunch, Log, TEXT("ScenarioEditor 새 draft 자동 시작 완료"));
	}
	else
	{
		FString resolvedPath;
		TArray<FString> diagnostics;
		if (!editorController->LoadProjectScenarioJsonFile(PendingScenarioSetupPath, resolvedPath, diagnostics))
		{
			UE_LOG(
				LogScenarioEditorLaunch,
				Warning,
				TEXT("Project scenario 자동 로드 실패 | Path: %s, Diagnostics: %s"),
				*PendingScenarioSetupPath,
				*FString::Join(diagnostics, TEXT(" | ")));
			return;
		}

		bLoadedExistingScenario = true;
		UE_LOG(
			LogScenarioEditorLaunch,
			Log,
			TEXT("Project scenario 자동 로드 완료 | Path: %s"),
			*resolvedPath);
	}

	bAutoStartedScenarioEditorSession = true;
	bAutoStartedScenarioEditorSessionLoadedExistingScenario = bLoadedExistingScenario;
	AutoStartCompletedEvent.Broadcast(bLoadedExistingScenario);
	ResetPendingAutoStartState();
}

bool UScenarioEditorLaunchSubsystem::DoesWorldMatchMapId(const UWorld* world, const FString& mapId)
{
	if (!world)
	{
		return false;
	}

	const FString targetMapName = FPackageName::GetShortName(NormalizeMapIdForOpenLevel(mapId));
	FString worldMapName = world->GetMapName();
	if (!world->StreamingLevelsPrefix.IsEmpty())
	{
		worldMapName.RemoveFromStart(world->StreamingLevelsPrefix);
	}

	return worldMapName.Equals(targetMapName, ESearchCase::IgnoreCase);
}
