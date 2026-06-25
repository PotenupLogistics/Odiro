#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodeReplayDataTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "ScenarioReplaySubsystem.generated.h"

class ADeliveryBotReplayActor;
class AActor;
class ASceneCapture2D;
class UTextureRenderTarget2D;
class USceneCaptureComponent2D;
struct FScenarioPlaceableInstanceSpec;
struct FScenarioStaticObstaclePropEntry;
struct FScenarioWorldSpec;

// Owns embedded replay loading, playback state, replay actors, and render target capture.
UCLASS(BlueprintType)
class ODIROSIM_API UScenarioReplaySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// Loads replay artifacts from an episode directory and prepares the render target.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Replay")
	bool LoadEpisodeReplay(
		const FString& EpisodeDirectory,
		TArray<FString>& OutDiagnostics);

	// Unloads transient replay actors, scenario actors, and render resources.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Replay")
	void UnloadReplay();

	// Starts playback from the current replay time.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Replay")
	void Play();

	// Pauses playback without unloading replay artifacts.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Replay")
	void Pause();

	// Stops playback and seeks to the first frame when loaded.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Replay")
	void Stop();

	// Seeks to the nearest replay frame for the requested time.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Replay")
	bool Seek(double TimeSeconds);

	// Returns the render target that UMG can display.
	UFUNCTION(BlueprintPure, Category = "Scenario|Replay")
	UTextureRenderTarget2D* GetReplayRenderTarget() const { return ReplayRenderTarget; }

	// Returns the current playback state.
	UFUNCTION(BlueprintPure, Category = "Scenario|Replay")
	EScenarioReplayPlaybackState GetPlaybackState() const { return PlaybackState; }

	// Returns the current replay time in seconds.
	UFUNCTION(BlueprintPure, Category = "Scenario|Replay")
	double GetCurrentReplayTimeSeconds() const { return CurrentReplayTimeSeconds; }

	// Returns the loaded replay duration in seconds.
	UFUNCTION(BlueprintPure, Category = "Scenario|Replay")
	double GetDurationSeconds() const { return Manifest.DurationSeconds; }

	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;

private:
	// Destroys replay-only actors and releases transient render resources.
	void CleanupReplayWorld();

	// Loads and materializes the episode scenario.json map when present.
	bool LoadEpisodeScenarioWorld(
		const FString& EpisodeDirectory,
		TArray<FString>& OutDiagnostics);

	// Spawns replay-only scenario actors from a compiled scenario world spec.
	bool SpawnReplayScenarioWorld(
		const FScenarioWorldSpec& WorldSpec,
		TArray<FString>& OutDiagnostics);

	// Spawns one replay-only static obstacle from the compiled scenario placeable.
	bool SpawnReplayStaticObstacle(
		const FScenarioPlaceableInstanceSpec& PlaceableSpec,
		TArray<FString>& OutDiagnostics);

	// Resolves one static obstacle prop entry from the default scenario catalog.
	bool TryFindStaticObstacleProp(
		FName PropId,
		FScenarioStaticObstaclePropEntry& OutPropEntry,
		TArray<FString>& OutDiagnostics) const;

	// Applies the replay world offset to a compiled scenario transform.
	FTransform MakeReplayWorldTransform(const FTransform& SourceTransform) const;

	// Adds one transient replay actor to capture visibility and cleanup ownership.
	void RegisterReplayScenarioActor(AActor* Actor);

	// Applies the nearest loaded frame and captures the scene into the render target.
	bool ApplyFrameAtTime(double TimeSeconds);

	// Creates the transient render target used by the replay panel.
	UTextureRenderTarget2D* CreateReplayRenderTarget();

	// Spawns and configures the top-down replay capture actor.
	ASceneCapture2D* SpawnReplayCaptureActor();

	// Returns the capture component for the current capture actor.
	USceneCaptureComponent2D* GetReplayCaptureComponent() const;

	// Episode directory currently loaded by the replay subsystem.
	UPROPERTY(Transient)
	FString LoadedEpisodeDirectory;

	// Parsed manifest for the loaded replay.
	UPROPERTY(Transient)
	FEpisodeReplayManifest Manifest;

	// Loaded V1 robot replay frames.
	UPROPERTY(Transient)
	TArray<FEpisodeReplayRobotFrame> Frames;

	// Visual-only robot actor driven by replay frames.
	UPROPERTY(Transient)
	TObjectPtr<ADeliveryBotReplayActor> ReplayRobotActor;

	// SceneCapture2D actor rendering replay-only actors.
	UPROPERTY(Transient)
	TObjectPtr<ASceneCapture2D> ReplayCaptureActor;

	// Render target shown by debug or formal replay UI.
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> ReplayRenderTarget;

	// Replay-only static scenario actors generated from episode scenario.json.
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> ReplayScenarioActors;

	// Current playback state.
	UPROPERTY(Transient)
	EScenarioReplayPlaybackState PlaybackState = EScenarioReplayPlaybackState::Stopped;

	// Current replay time in seconds.
	double CurrentReplayTimeSeconds = 0.0;

	// Playback speed multiplier.
	double PlaybackSpeed = 1.0;

	// Far-away offset that isolates replay actors from the active scenario world.
	FVector ReplayWorldOffset = FVector(500000.0, 0.0, 0.0);

	// Orthographic camera height above the replay robot.
	double CaptureHeightCm = 3000.0;

	// Orthographic width used by the V1 robot-only debug replay view.
	double CaptureOrthoWidthCm = 1800.0;
};
