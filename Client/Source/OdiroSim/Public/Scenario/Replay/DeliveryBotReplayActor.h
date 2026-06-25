#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Shared/EpisodeReplayDataTypes.h"
#include "DeliveryBotReplayActor.generated.h"

class UStaticMeshComponent;

// Visual-only robot actor used by embedded replay playback.
UCLASS()
class ODIROSIM_API ADeliveryBotReplayActor : public AActor
{
	GENERATED_BODY()

public:
	// Creates replay-only components with collision and physics disabled.
	ADeliveryBotReplayActor();

	// Applies one recorded frame at the replay sandbox offset.
	void ApplyReplayFrame(
		const FEpisodeReplayRobotFrame& Frame,
		const FVector& ReplayWorldOffset);

private:
	// Root component that owns the visual replay transform.
	UPROPERTY(VisibleAnywhere, Category = "Scenario|Replay")
	TObjectPtr<USceneComponent> SceneRoot;

	// Temporary V1 robot body mesh used until real wheel/body visuals are wired.
	UPROPERTY(VisibleAnywhere, Category = "Scenario|Replay")
	TObjectPtr<UStaticMeshComponent> BodyMesh;
};
