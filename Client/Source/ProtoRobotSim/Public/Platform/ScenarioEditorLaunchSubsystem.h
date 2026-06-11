#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ScenarioEditorLaunchSubsystem.generated.h"

class UWorld;

DECLARE_MULTICAST_DELEGATE_OneParam(FScenarioEditorAutoStartCompletedNative, bool /*bLoadedExistingScenario*/);

enum class EScenarioEditorAutoStartMode : uint8
{
	None,
	LoadFromPath,
	NewDraft,
};

// GameInstance lifetime keeps the requested scenario editor startup mode alive while OpenLevel replaces the world.
UCLASS(BlueprintType)
class PROTOROBOTSIM_API UScenarioEditorLaunchSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Platform|ScenarioEditor")
	bool OpenScenarioEditor(const FString& scenarioSetupPath);

	UFUNCTION(BlueprintCallable, Category = "Platform|ScenarioEditor")
	bool OpenNewScenarioEditor();

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
	void HandlePostLoadMapWithWorld(UWorld* loadedWorld);
	void TryApplyPendingEditorStartup(UWorld* loadedWorld);
	void ResetPendingAutoStartState();
	bool OpenScenarioEditorInternal(EScenarioEditorAutoStartMode launchMode, const FString& scenarioSetupPath);
	static bool DoesWorldMatchMapId(const UWorld* world, const FString& mapId);

	UPROPERTY(Transient)
	FString PendingScenarioSetupPath;

	EScenarioEditorAutoStartMode PendingAutoStartMode = EScenarioEditorAutoStartMode::None;
	bool bAutoStartedScenarioEditorSession = false;
	bool bAutoStartedScenarioEditorSessionLoadedExistingScenario = false;

	FScenarioEditorAutoStartCompletedNative AutoStartCompletedEvent;
	FDelegateHandle PostLoadMapHandle;
};
