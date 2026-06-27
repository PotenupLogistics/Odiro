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
	ApplyReplayWheelFrames(Frame);
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

void ADeliveryBotReplayActor::ApplyReplayWheelFrames(const FEpisodeReplayRobotFrame& Frame) const
{
	if (Frame.Wheels.Num() != EpisodeReplayV2::WheelCount)
	{
		return;
	}

	static const FName PivotNames[EpisodeReplayV2::WheelCount] =
	{
		TEXT("Wheel_FL_Pivot"),
		TEXT("Wheel_FR_Pivot"),
		TEXT("Wheel_RL_Pivot"),
		TEXT("Wheel_RR_Pivot")
	};
	static const FName MeshNames[EpisodeReplayV2::WheelCount] =
	{
		TEXT("Wheel_FL_Mesh"),
		TEXT("Wheel_FR_Mesh"),
		TEXT("Wheel_RL_Mesh"),
		TEXT("Wheel_RR_Mesh")
	};

	for (int32 WheelIndex = 0; WheelIndex < EpisodeReplayV2::WheelCount; ++WheelIndex)
	{
		const FEpisodeReplayWheelFrame& WheelFrame = Frame.Wheels[WheelIndex];
		if (!WheelFrame.bHasVisualPose)
		{
			continue;
		}

		if (USceneComponent* PivotComponent = FindReplayVisualComponent(PivotNames[WheelIndex]))
		{
			if (!WheelFrame.LocalLocationCm.IsNearlyZero(KINDA_SMALL_NUMBER))
			{
				PivotComponent->SetRelativeLocation(WheelFrame.LocalLocationCm);
			}
		}

		if (USceneComponent* MeshComponent = FindReplayVisualComponent(MeshNames[WheelIndex]))
		{
			MeshComponent->SetRelativeRotation(WheelFrame.LocalRotation);
		}
	}
}

USceneComponent* ADeliveryBotReplayActor::FindReplayVisualComponent(const FName ComponentName) const
{
	AActor* VisualActor = GetReplayVisualActor();
	if (!IsValid(VisualActor))
	{
		return nullptr;
	}

	TArray<USceneComponent*> SceneComponents;
	const FString TargetComponentName = ComponentName.ToString();
	VisualActor->GetComponents<USceneComponent>(SceneComponents);
	for (USceneComponent* SceneComponent : SceneComponents)
	{
		if (!IsValid(SceneComponent))
		{
			continue;
		}

		const FString SceneComponentName = SceneComponent->GetName();
		if (SceneComponent->GetFName() == ComponentName
			|| SceneComponentName.StartsWith(TargetComponentName + TEXT("_"), ESearchCase::IgnoreCase))
		{
			return SceneComponent;
		}
	}

	return nullptr;
}
