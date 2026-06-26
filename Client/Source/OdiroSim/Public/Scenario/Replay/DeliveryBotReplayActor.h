#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Shared/EpisodeReplayDataTypes.h"
#include "DeliveryBotReplayActor.generated.h"

class UChildActorComponent;

// Visual-only robot actor used by embedded replay playback.
UCLASS()
class ODIROSIM_API ADeliveryBotReplayActor : public AActor
{
	GENERATED_BODY()

public:
	// Creates replay-only components with collision and physics disabled.
	ADeliveryBotReplayActor();

	// Applies replay-only safeguards to the spawned visual child.
	virtual void PostInitializeComponents() override;

	// Applies one recorded frame at the replay sandbox offset.
	void ApplyReplayFrame(
		const FEpisodeReplayRobotFrame& Frame,
		const FVector& ReplayWorldOffset);

	// Returns the visual child actor that owns the replay robot mesh components.
	AActor* GetReplayVisualActor() const;

private:
	// Disables physical interaction on the visual child while keeping it renderable.
	void ConfigureReplayVisualActor() const;

	// Applies Replay V2 wheel visual poses to the child visual rig.
	void ApplyReplayWheelFrames(const FEpisodeReplayRobotFrame& Frame) const;

	// Finds a named scene component on the child visual actor.
	USceneComponent* FindReplayVisualComponent(FName ComponentName) const;

	// Root component that owns the visual replay transform.
	UPROPERTY(VisibleAnywhere, Category = "Scenario|Replay")
	TObjectPtr<USceneComponent> SceneRoot;

	// Child actor component that hosts the visual-only replay robot Blueprint.
	UPROPERTY(VisibleAnywhere, Category = "Scenario|Replay")
	TObjectPtr<UChildActorComponent> VisualActorComponent;

	// Visual-only Blueprint class used for replay robot rendering.
	UPROPERTY(EditDefaultsOnly, Category = "Scenario|Replay")
	TSubclassOf<AActor> ReplayVisualActorClass;

	// Relative scale applied to the replay visual so it matches the runtime DeliveryBot scale.
	UPROPERTY(EditDefaultsOnly, Category = "Scenario|Replay")
	FVector ReplayVisualScale = FVector(0.2);
};
