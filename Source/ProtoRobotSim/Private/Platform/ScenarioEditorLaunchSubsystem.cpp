
#include "Platform/ScenarioEditorLaunchSubsystem.h"
#include "Scenario/Editor/ScenarioEditorController.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogScenarioEditorLaunch, Log, All);

namespace
{
	FString NormalizeMapIdForOpenLevel(const FString& mapId)
	{
		FString normalizedMapId = mapId.TrimStartAndEnd();
		if (normalizedMapId.IsEmpty())
		{
			normalizedMapId = TEXT("EpisodeEditorMap");
		}

		int32 objectNameSeparatorIndex = INDEX_NONE;
		if (normalizedMapId.FindChar(TEXT('.'), objectNameSeparatorIndex))
		{
			normalizedMapId = normalizedMapId.Left(objectNameSeparatorIndex);
		}

		return normalizedMapId;
	}
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

	Super::Deinitialize();
}

bool UScenarioEditorLaunchSubsystem::OpenEpisodeEditor(const FString& episodeSetupPath)
{
	const FString trimmedEpisodeSetupPath = episodeSetupPath.TrimStartAndEnd();
	if (trimmedEpisodeSetupPath.IsEmpty())
	{
		return OpenNewEpisodeEditor();
	}

	return OpenEpisodeEditorInternal(EScenarioEditorAutoStartMode::LoadFromPath, trimmedEpisodeSetupPath);
}

bool UScenarioEditorLaunchSubsystem::OpenNewEpisodeEditor()
{
	return OpenEpisodeEditorInternal(EScenarioEditorAutoStartMode::NewDraft, FString());
}

bool UScenarioEditorLaunchSubsystem::OpenEpisodeEditorInternal(
	const EScenarioEditorAutoStartMode launchMode,
	const FString& episodeSetupPath)
{
	UWorld* world = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!world)
	{
		UE_LOG(LogScenarioEditorLaunch, Warning, TEXT("EpisodeEditorMap 열기 실패: World 없음"));
		return false;
	}

	PendingAutoStartMode = launchMode;
	PendingEpisodeSetupPath = episodeSetupPath.TrimStartAndEnd();
	bAutoStartedEpisodeEditorSession = false;
	bAutoStartedEpisodeEditorSessionLoadedExistingEpisode = false;

	const FString openLevelName = NormalizeMapIdForOpenLevel(EpisodeEditorMapId);
	const FString openLevelOptions = launchMode == EScenarioEditorAutoStartMode::LoadFromPath
		? FString::Printf(TEXT("EpisodeSetup=%s"), *PendingEpisodeSetupPath)
		: TEXT("NewEpisode=1");

	// The URL options keep the transition inspectable in logs/console, while the subsystem state is the authoritative
	// startup request that survives the world replacement.
	UE_LOG(
		LogScenarioEditorLaunch,
		Log,
		TEXT("EpisodeEditorMap 열기 요청 | Map: %s, Options: %s"),
		*openLevelName,
		*openLevelOptions);
	UGameplayStatics::OpenLevel(world, FName(*openLevelName), true, openLevelOptions);
	return true;
}

void UScenarioEditorLaunchSubsystem::ClearPendingEpisodeSetupPath()
{
	ResetPendingAutoStartState();
}

void UScenarioEditorLaunchSubsystem::ResetPendingAutoStartState()
{
	PendingEpisodeSetupPath.Reset();
	PendingAutoStartMode = EScenarioEditorAutoStartMode::None;
}

void UScenarioEditorLaunchSubsystem::HandlePostLoadMapWithWorld(UWorld* loadedWorld)
{
	if (!loadedWorld || !DoesWorldMatchMapId(loadedWorld, EpisodeEditorMapId))
	{
		return;
	}

	// Defer one tick so the EpisodeEditor controller and its startup widgets have completed their own construction.
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
			TEXT("EpisodeEditor 자동 시작 보류: ScenarioEditorController 없음 | Mode: %d, Path: %s"),
			static_cast<int32>(PendingAutoStartMode),
			PendingEpisodeSetupPath.IsEmpty() ? TEXT("<new draft>") : *PendingEpisodeSetupPath);
		return;
	}

	bool bLoadedExistingEpisode = false;
	if (PendingAutoStartMode == EScenarioEditorAutoStartMode::NewDraft)
	{
		editorController->NewEpisodeDraft();
		UE_LOG(LogScenarioEditorLaunch, Log, TEXT("EpisodeEditor 새 draft 자동 시작 완료"));
	}
	else
	{
		FString resolvedPath;
		TArray<FString> diagnostics;
		if (!editorController->LoadEpisodeSetupJsonFile(PendingEpisodeSetupPath, resolvedPath, diagnostics))
		{
			UE_LOG(
				LogScenarioEditorLaunch,
				Warning,
				TEXT("EpisodeSetup 자동 로드 실패 | Path: %s, Diagnostics: %s"),
				*PendingEpisodeSetupPath,
				*FString::Join(diagnostics, TEXT(" | ")));
			return;
		}

		bLoadedExistingEpisode = true;
		UE_LOG(
			LogScenarioEditorLaunch,
			Log,
			TEXT("EpisodeSetup 자동 로드 완료 | Path: %s"),
			*resolvedPath);
	}

	bAutoStartedEpisodeEditorSession = true;
	bAutoStartedEpisodeEditorSessionLoadedExistingEpisode = bLoadedExistingEpisode;
	AutoStartCompletedEvent.Broadcast(bLoadedExistingEpisode);
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
