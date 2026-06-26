#include "Scenario/Replay/DeliveryBotReplayActor.h"

#include "Components/ChildActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "UObject/ConstructorHelpers.h"

ADeliveryBotReplayActor::ADeliveryBotReplayActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	Tags.Add(TEXT("ReplayOnly"));

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	VisualActorComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("ReplayVisualActor"));
	VisualActorComponent->SetupAttachment(SceneRoot);
	VisualActorComponent->SetRelativeScale3D(ReplayVisualScale);

	static ConstructorHelpers::FClassFinder<AActor> ReplayVisualActorFinder(
		TEXT("/Game/Blueprints/Vehicle/BP_DeliveryBotReplayVisual"));
	if (ReplayVisualActorFinder.Succeeded())
	{
		ReplayVisualActorClass = ReplayVisualActorFinder.Class;
		VisualActorComponent->SetChildActorClass(ReplayVisualActorClass);
	}
}

void ADeliveryBotReplayActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	ConfigureReplayVisualActor();
}

void ADeliveryBotReplayActor::ApplyReplayFrame(
	const FEpisodeReplayRobotFrame& Frame,
	const FVector& ReplayWorldOffset)
{
	SetActorLocation(Frame.PositionCm + ReplayWorldOffset);
	SetActorRotation(Frame.Rotation);
}

AActor* ADeliveryBotReplayActor::GetReplayVisualActor() const
{
	return IsValid(VisualActorComponent)
		? VisualActorComponent->GetChildActor()
		: nullptr;
}

void ADeliveryBotReplayActor::ConfigureReplayVisualActor() const
{
	AActor* VisualActor = GetReplayVisualActor();
	if (!IsValid(VisualActor))
	{
		return;
	}

	VisualActor->Tags.AddUnique(FName(TEXT("ReplayOnly")));
	VisualActor->SetActorEnableCollision(false);

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	VisualActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!IsValid(PrimitiveComponent))
		{
			continue;
		}

		PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PrimitiveComponent->SetGenerateOverlapEvents(false);
		PrimitiveComponent->SetSimulatePhysics(false);
		PrimitiveComponent->SetComponentTickEnabled(false);
	}
}
