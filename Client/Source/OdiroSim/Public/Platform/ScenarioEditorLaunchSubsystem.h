#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ScenarioEditorLaunchSubsystem.generated.h"

class UWorld;
class UObject;
struct FStreamableHandle;

DECLARE_MULTICAST_DELEGATE_OneParam(FScenarioEditorAutoStartCompletedNative, bool /*bLoadedExistingScenario*/);

enum class EScenarioEditorAutoStartMode : uint8
{
	None,
	LoadFromPath,
	NewDraft,
};

// GameInstance lifetime keeps the requested scenario editor startup mode alive while OpenLevel replaces the world.
UCLASS(BlueprintType)
class ODIROSIM_API UScenarioEditorLaunchSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Seeds the default editor visual preload asset list.
	UScenarioEditorLaunchSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Platform|ScenarioEditor")
	bool OpenScenarioEditor(const FString& scenarioSetupPath);

	// Scenario editor map으로만 전환한다. 자동 draft/load 시작은 요청하지 않는다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ScenarioEditor")
	bool OpenScenarioEditorMap();

	UFUNCTION(BlueprintCallable, Category = "Platform|ScenarioEditor")
	bool OpenNewScenarioEditor();

	// Opens a new draft and saves its initial project scenario.json to the requested path.
	UFUNCTION(BlueprintCallable, Category = "Platform|ScenarioEditor")
	bool OpenNewScenarioEditorAtPath(const FString& scenarioJsonPath);

	UFUNCTION(BlueprintPure, Category = "Platform|ScenarioEditor")
	FString GetPendingScenarioSetupPath() const { return PendingScenarioSetupPath; }

	UFUNCTION(BlueprintCallable, Category = "Platform|ScenarioEditor")
	void ClearPendingScenarioSetupPath();

	UFUNCTION(BlueprintPure, Category = "Platform|ScenarioEditor")
	bool HasAutoStartedScenarioEditorSession() const { return bAutoStartedScenarioEditorSession; }

	UFUNCTION(BlueprintPure, Category = "Platform|ScenarioEditor")
	bool WasAutoStartedScenarioEditorSessionLoadedExistingScenario() const
	{
		return bAutoStartedScenarioEditorSessionLoadedExistingScenario;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ScenarioEditor")
	FString ScenarioEditorMapId = TEXT("ScenarioEditorMap");

	FScenarioEditorAutoStartCompletedNative& OnAutoStartCompleted() { return AutoStartCompletedEvent; }

private:
	// Receives ScenarioEditorMap load completion before applying deferred scenario startup work.
	void HandlePostLoadMapWithWorld(UWorld* loadedWorld);
	// Applies the pending new/load request after ScenarioEditorMap has created its controller.
	void TryApplyPendingEditorStartup(UWorld* loadedWorld);
	// Clears auto-start request state that should not survive another explicit launch request.
	void ResetPendingAutoStartState();
	// Stores an auto-start request and gates ScenarioEditorMap travel behind editor visual preload.
	bool OpenScenarioEditorInternal(EScenarioEditorAutoStartMode launchMode, const FString& scenarioSetupPath);
	// Performs the actual ScenarioEditorMap travel after editor visual assets are resident.
	bool OpenScenarioEditorMapAfterPreload(const FString& openLevelOptions);
	// Starts or reuses the async editor visual preload request for an upcoming ScenarioEditorMap travel.
	void RequestScenarioEditorPreload(const FString& openLevelOptions);
	// Opens ScenarioEditorMap after the preload request has completed and caches loaded assets.
	void HandleScenarioEditorPreloadComplete();
	// Resolves preloaded objects into strong references that survive OpenLevel.
	void CacheLoadedScenarioEditorPreloadAssets();
	// Compares PIE-prefixed world names against the configured editor map id.
	static bool DoesWorldMatchMapId(const UWorld* world, const FString& mapId);

	UPROPERTY(Transient)
	FString PendingScenarioSetupPath;

	// Editor viewport assets that should be resident before ScenarioEditorMap becomes visible.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ScenarioEditor", meta = (AllowPrivateAccess = "true"))
	TArray<TSoftObjectPtr<UObject>> ScenarioEditorPreloadAssets;

	// Strong references keeping preloaded editor assets resident across the map transition.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> LoadedScenarioEditorPreloadAssets;

	// Active async load request that gates ScenarioEditorMap OpenLevel.
	TSharedPtr<FStreamableHandle> ScenarioEditorPreloadHandle;

	// OpenLevel options waiting for editor visual asset preload completion.
	FString PendingScenarioEditorOpenLevelOptions;

	// Pending auto-start mode to apply after ScenarioEditorMap has loaded.
	EScenarioEditorAutoStartMode PendingAutoStartMode = EScenarioEditorAutoStartMode::None;
	// Whether the latest scenario editor map session completed its requested startup.
	bool bAutoStartedScenarioEditorSession = false;
	// Whether the latest completed scenario editor auto-start loaded an existing scenario file.
	bool bAutoStartedScenarioEditorSessionLoadedExistingScenario = false;

	// Native notification for observers waiting for deferred ScenarioEditorMap startup completion.
	FScenarioEditorAutoStartCompletedNative AutoStartCompletedEvent;
	// Global map-load delegate handle owned by this subsystem lifetime.
	FDelegateHandle PostLoadMapHandle;
};
