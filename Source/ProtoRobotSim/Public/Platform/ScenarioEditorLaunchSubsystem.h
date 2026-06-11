#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ScenarioEditorLaunchSubsystem.generated.h"

class UWorld;

DECLARE_MULTICAST_DELEGATE_OneParam(FScenarioEditorAutoStartCompletedNative, bool /*bLoadedExistingEpisode*/);

enum class EScenarioEditorAutoStartMode : uint8
{
	None,
	LoadFromPath,
	NewDraft,
};

// GameInstance lifetime keeps the requested EpisodeEditor startup mode alive while OpenLevel replaces the world.
UCLASS(BlueprintType)
class PROTOROBOTSIM_API UScenarioEditorLaunchSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Platform|ScenarioEditor")
	bool OpenEpisodeEditor(const FString& episodeSetupPath);

	UFUNCTION(BlueprintCallable, Category = "Platform|ScenarioEditor")
	bool OpenNewEpisodeEditor();

	UFUNCTION(BlueprintPure, Category = "Platform|ScenarioEditor")
	FString GetPendingEpisodeSetupPath() const { return PendingEpisodeSetupPath; }

	// Legacy Blueprint-facing reset name. New C++ code should call ResetPendingAutoStartState().
	UFUNCTION(BlueprintCallable, Category = "Platform|ScenarioEditor")
	void ClearPendingEpisodeSetupPath();

	UFUNCTION(BlueprintPure, Category = "Platform|ScenarioEditor")
	bool HasAutoStartedEpisodeEditorSession() const { return bAutoStartedEpisodeEditorSession; }

	UFUNCTION(BlueprintPure, Category = "Platform|ScenarioEditor")
	bool WasAutoStartedEpisodeEditorSessionLoadedExistingEpisode() const
	{
		return bAutoStartedEpisodeEditorSessionLoadedExistingEpisode;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ScenarioEditor")
	FString EpisodeEditorMapId = TEXT("EpisodeEditorMap");

	FScenarioEditorAutoStartCompletedNative& OnAutoStartCompleted() { return AutoStartCompletedEvent; }

private:
	void HandlePostLoadMapWithWorld(UWorld* loadedWorld);
	void TryApplyPendingEditorStartup(UWorld* loadedWorld);
	void ResetPendingAutoStartState();
	bool OpenEpisodeEditorInternal(EScenarioEditorAutoStartMode launchMode, const FString& episodeSetupPath);
	static bool DoesWorldMatchMapId(const UWorld* world, const FString& mapId);

	UPROPERTY(Transient)
	FString PendingEpisodeSetupPath;

	EScenarioEditorAutoStartMode PendingAutoStartMode = EScenarioEditorAutoStartMode::None;
	bool bAutoStartedEpisodeEditorSession = false;
	bool bAutoStartedEpisodeEditorSessionLoadedExistingEpisode = false;

	FScenarioEditorAutoStartCompletedNative AutoStartCompletedEvent;
	FDelegateHandle PostLoadMapHandle;
};
