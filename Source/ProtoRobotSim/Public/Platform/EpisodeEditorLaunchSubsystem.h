#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EpisodeEditorLaunchSubsystem.generated.h"

class UWorld;

DECLARE_MULTICAST_DELEGATE_OneParam(FEpisodeEditorAutoStartCompletedNative, bool /*bLoadedExistingEpisode*/);

enum class EEpisodeEditorAutoStartMode : uint8
{
	None,
	LoadFromPath,
	NewDraft,
};

// GameInstance lifetime keeps the requested EpisodeEditor startup mode alive while OpenLevel replaces the world.
UCLASS(BlueprintType)
class PROTOROBOTSIM_API UEpisodeEditorLaunchSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Platform|EpisodeEditor")
	bool OpenEpisodeEditor(const FString& episodeSetupPath);

	UFUNCTION(BlueprintCallable, Category = "Platform|EpisodeEditor")
	bool OpenNewEpisodeEditor();

	UFUNCTION(BlueprintPure, Category = "Platform|EpisodeEditor")
	FString GetPendingEpisodeSetupPath() const { return PendingEpisodeSetupPath; }

	// Legacy Blueprint-facing reset name. New C++ code should call ResetPendingAutoStartState().
	UFUNCTION(BlueprintCallable, Category = "Platform|EpisodeEditor")
	void ClearPendingEpisodeSetupPath();

	UFUNCTION(BlueprintPure, Category = "Platform|EpisodeEditor")
	bool HasAutoStartedEpisodeEditorSession() const { return bAutoStartedEpisodeEditorSession; }

	UFUNCTION(BlueprintPure, Category = "Platform|EpisodeEditor")
	bool WasAutoStartedEpisodeEditorSessionLoadedExistingEpisode() const
	{
		return bAutoStartedEpisodeEditorSessionLoadedExistingEpisode;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|EpisodeEditor")
	FString EpisodeEditorMapId = TEXT("EpisodeEditorMap");

	FEpisodeEditorAutoStartCompletedNative& OnAutoStartCompleted() { return AutoStartCompletedEvent; }

private:
	void HandlePostLoadMapWithWorld(UWorld* loadedWorld);
	void TryApplyPendingEditorStartup(UWorld* loadedWorld);
	void ResetPendingAutoStartState();
	bool OpenEpisodeEditorInternal(EEpisodeEditorAutoStartMode launchMode, const FString& episodeSetupPath);
	static bool DoesWorldMatchMapId(const UWorld* world, const FString& mapId);

	UPROPERTY(Transient)
	FString PendingEpisodeSetupPath;

	EEpisodeEditorAutoStartMode PendingAutoStartMode = EEpisodeEditorAutoStartMode::None;
	bool bAutoStartedEpisodeEditorSession = false;
	bool bAutoStartedEpisodeEditorSessionLoadedExistingEpisode = false;

	FEpisodeEditorAutoStartCompletedNative AutoStartCompletedEvent;
	FDelegateHandle PostLoadMapHandle;
};
