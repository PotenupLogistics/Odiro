#include "Scenario/Replay/DeliveryBotReplayActor.h"

#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ADeliveryBotReplayActor::ADeliveryBotReplayActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	Tags.Add(TEXT("ReplayOnly"));

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(SceneRoot);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetGenerateOverlapEvents(false);
	BodyMesh->SetSimulatePhysics(false);
	BodyMesh->SetRelativeScale3D(FVector(0.8, 1.2, 0.35));
	BodyMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		BodyMesh->SetStaticMesh(CubeMeshFinder.Object);
	}
}

void ADeliveryBotReplayActor::ApplyReplayFrame(
	const FEpisodeReplayRobotFrame& Frame,
	const FVector& ReplayWorldOffset)
{
	SetActorLocation(Frame.PositionCm + ReplayWorldOffset);
	SetActorRotation(Frame.Rotation);
}
