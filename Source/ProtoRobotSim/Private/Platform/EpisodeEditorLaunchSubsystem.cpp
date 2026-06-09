
#include "Platform/EpisodeEditorLaunchSubsystem.h"
#include "Episode/Editor/EpisodeEditorController.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogEpisodeEditorLaunch, Log, All);

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

void UEpisodeEditorLaunchSubsystem::Initialize(FSubsystemCollectionBase& collection)
{
	Super::Initialize(collection);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this,
		&UEpisodeEditorLaunchSubsystem::HandlePostLoadMapWithWorld);
}

void UEpisodeEditorLaunchSubsystem::Deinitialize()
{
	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}

	Super::Deinitialize();
}

bool UEpisodeEditorLaunchSubsystem::OpenEpisodeEditor(const FString& episodeSetupPath)
{
	const FString trimmedEpisodeSetupPath = episodeSetupPath.TrimStartAndEnd();
	if (trimmedEpisodeSetupPath.IsEmpty())
	{
		return OpenNewEpisodeEditor();
	}

	return OpenEpisodeEditorInternal(EEpisodeEditorAutoStartMode::LoadFromPath, trimmedEpisodeSetupPath);
}

bool UEpisodeEditorLaunchSubsystem::OpenNewEpisodeEditor()
{
	return OpenEpisodeEditorInternal(EEpisodeEditorAutoStartMode::NewDraft, FString());
}

bool UEpisodeEditorLaunchSubsystem::OpenEpisodeEditorInternal(
	const EEpisodeEditorAutoStartMode launchMode,
	const FString& episodeSetupPath)
{
	UWorld* world = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!world)
	{
		UE_LOG(LogEpisodeEditorLaunch, Warning, TEXT("EpisodeEditorMap 열기 실패: World 없음"));
		return false;
	}

	PendingAutoStartMode = launchMode;
	PendingEpisodeSetupPath = episodeSetupPath.TrimStartAndEnd();
	bAutoStartedEpisodeEditorSession = false;
	bAutoStartedEpisodeEditorSessionLoadedExistingEpisode = false;

	const FString openLevelName = NormalizeMapIdForOpenLevel(EpisodeEditorMapId);
	const FString openLevelOptions = launchMode == EEpisodeEditorAutoStartMode::LoadFromPath
		? FString::Printf(TEXT("EpisodeSetup=%s"), *PendingEpisodeSetupPath)
		: TEXT("NewEpisode=1");

	// The URL options keep the transition inspectable in logs/console, while the subsystem state is the authoritative
	// startup request that survives the world replacement.
	UE_LOG(
		LogEpisodeEditorLaunch,
		Log,
		TEXT("EpisodeEditorMap 열기 요청 | Map: %s, Options: %s"),
		*openLevelName,
		*openLevelOptions);
	UGameplayStatics::OpenLevel(world, FName(*openLevelName), true, openLevelOptions);
	return true;
}

void UEpisodeEditorLaunchSubsystem::ClearPendingEpisodeSetupPath()
{
	ResetPendingAutoStartState();
}

void UEpisodeEditorLaunchSubsystem::ResetPendingAutoStartState()
{
	PendingEpisodeSetupPath.Reset();
	PendingAutoStartMode = EEpisodeEditorAutoStartMode::None;
}

void UEpisodeEditorLaunchSubsystem::HandlePostLoadMapWithWorld(UWorld* loadedWorld)
{
	if (!loadedWorld || !DoesWorldMatchMapId(loadedWorld, EpisodeEditorMapId))
	{
		return;
	}

	// Defer one tick so the EpisodeEditor controller and its startup widgets have completed their own construction.
	loadedWorld->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(
			this,
			&UEpisodeEditorLaunchSubsystem::TryApplyPendingEditorStartup,
			loadedWorld));
}

void UEpisodeEditorLaunchSubsystem::TryApplyPendingEditorStartup(UWorld* loadedWorld)
{
	if (!loadedWorld || PendingAutoStartMode == EEpisodeEditorAutoStartMode::None)
	{
		return;
	}

	AEpisodeEditorController* editorController = Cast<AEpisodeEditorController>(
		UGameplayStatics::GetPlayerController(loadedWorld, 0));
	if (!editorController)
	{
		UE_LOG(
			LogEpisodeEditorLaunch,
			Warning,
			TEXT("EpisodeEditor 자동 시작 보류: EpisodeEditorController 없음 | Mode: %d, Path: %s"),
			static_cast<int32>(PendingAutoStartMode),
			PendingEpisodeSetupPath.IsEmpty() ? TEXT("<new draft>") : *PendingEpisodeSetupPath);
		return;
	}

	bool bLoadedExistingEpisode = false;
	if (PendingAutoStartMode == EEpisodeEditorAutoStartMode::NewDraft)
	{
		editorController->NewEpisodeDraft();
		UE_LOG(LogEpisodeEditorLaunch, Log, TEXT("EpisodeEditor 새 draft 자동 시작 완료"));
	}
	else
	{
		FString resolvedPath;
		TArray<FString> diagnostics;
		if (!editorController->LoadEpisodeSetupJsonFile(PendingEpisodeSetupPath, resolvedPath, diagnostics))
		{
			UE_LOG(
				LogEpisodeEditorLaunch,
				Warning,
				TEXT("EpisodeSetup 자동 로드 실패 | Path: %s, Diagnostics: %s"),
				*PendingEpisodeSetupPath,
				*FString::Join(diagnostics, TEXT(" | ")));
			return;
		}

		bLoadedExistingEpisode = true;
		UE_LOG(
			LogEpisodeEditorLaunch,
			Log,
			TEXT("EpisodeSetup 자동 로드 완료 | Path: %s"),
			*resolvedPath);
	}

	bAutoStartedEpisodeEditorSession = true;
	bAutoStartedEpisodeEditorSessionLoadedExistingEpisode = bLoadedExistingEpisode;
	AutoStartCompletedEvent.Broadcast(bLoadedExistingEpisode);
	ResetPendingAutoStartState();
}

bool UEpisodeEditorLaunchSubsystem::DoesWorldMatchMapId(const UWorld* world, const FString& mapId)
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
