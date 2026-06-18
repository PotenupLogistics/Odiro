
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

bool UScenarioEditorLaunchSubsystem::OpenScenarioEditor(const FString& scenarioSetupPath)
{
	const FString trimmedScenarioSetupPath = scenarioSetupPath.TrimStartAndEnd();
	if (trimmedScenarioSetupPath.IsEmpty())
	{
		return OpenNewScenarioEditor();
	}

	return OpenScenarioEditorInternal(EScenarioEditorAutoStartMode::LoadFromPath, trimmedScenarioSetupPath);
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
	UGameplayStatics::OpenLevel(world, FName(*openLevelName), true, openLevelOptions);
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
