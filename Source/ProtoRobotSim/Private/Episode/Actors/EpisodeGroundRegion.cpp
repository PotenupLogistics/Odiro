#include "Episode/Actors/EpisodeGroundRegion.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"

AEpisodeGroundRegion::AEpisodeGroundRegion()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	RegionBoundsComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("RegionBoundsComponent"));
	RegionBoundsComponent->SetupAttachment(SceneRoot);
	RegionBoundsComponent->SetBoxExtent(FVector(50.0, 50.0, 50.0));
	RegionBoundsComponent->SetHiddenInGame(true);
	RegionBoundsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RegionBoundsComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	RegionBoundsComponent->SetGenerateOverlapEvents(false);
}

void AEpisodeGroundRegion::BeginPlay()
{
	Super::BeginPlay();

	DrawDebugRegion(2.0);
}

void AEpisodeGroundRegion::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	DrawDebugRegion(DebugDrawLifeTimeSeconds);
}

void AEpisodeGroundRegion::ConfigureRegion(const FEpisodeGroundRegionSpec& InRegionSpec)
{
	RegionSpec = InRegionSpec;
	RegionSpec.Size.X = FMath::Max(RegionSpec.Size.X, 1.0);
	RegionSpec.Size.Y = FMath::Max(RegionSpec.Size.Y, 1.0);

	SetActorLocation(RegionSpec.Center);
	SetActorRotation(FRotator(0.0, RegionSpec.YawDegrees, 0.0));

	if (!RegionSpec.CollisionTag.IsEmpty())
	{
		Tags.AddUnique(FName(*RegionSpec.CollisionTag));
	}

	const FVector BoxExtent(
		RegionSpec.Size.X * 0.5,
		RegionSpec.Size.Y * 0.5,
		BlockedCollisionHeightCm * 0.5);

	RegionBoundsComponent->SetRelativeLocation(FVector(0.0, 0.0, BlockedCollisionHeightCm * 0.5));
	RegionBoundsComponent->SetBoxExtent(BoxExtent, true);

	ApplyCollisionSettings();
	DrawDebugRegion(5.0);
}

bool AEpisodeGroundRegion::ContainsWorldLocation2D(const FVector& WorldLocation) const
{
	const FVector LocalLocation = GetActorTransform().InverseTransformPosition(WorldLocation);
	const FVector2D HalfSize(RegionSpec.Size.X * 0.5, RegionSpec.Size.Y * 0.5);

	return FMath::Abs(LocalLocation.X) <= HalfSize.X
		&& FMath::Abs(LocalLocation.Y) <= HalfSize.Y;
}

void AEpisodeGroundRegion::ApplyCollisionSettings()
{
	if (!RegionBoundsComponent)
	{
		return;
	}

	if (RegionSpec.RegionType == EEpisodeGroundRegionType::Blocked)
	{
		RegionBoundsComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		RegionBoundsComponent->SetCollisionObjectType(ECC_WorldStatic);
		RegionBoundsComponent->SetCollisionResponseToAllChannels(ECR_Block);
		return;
	}

	RegionBoundsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RegionBoundsComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
}

void AEpisodeGroundRegion::DrawDebugRegion(double LifeTimeSeconds) const
{
	UWorld* World = GetWorld();
	if (!World || RegionSpec.ShapeType != EEpisodeGroundShapeType::Rectangle)
	{
		return;
	}

	const FVector DebugCenter = GetActorLocation() + FVector(0.0, 0.0, DebugDrawZOffsetCm);
	const FVector DebugExtent(
		RegionSpec.Size.X * 0.5,
		RegionSpec.Size.Y * 0.5,
		DebugDrawHalfHeightCm);

	const FColor DebugColor = GetDebugColor();
	DrawDebugBox(
		World,
		DebugCenter,
		DebugExtent,
		GetActorQuat(),
		DebugColor,
		false,
		static_cast<float>(LifeTimeSeconds),
		0,
		static_cast<float>(DebugLineThickness));

	const FString RegionTypeLabel = GetRegionTypeLabel();
	const FString Label = FString::Printf(TEXT("%s (%s)"), *RegionSpec.RegionId, *RegionTypeLabel);
	DrawDebugString(
		World,
		DebugCenter + FVector(0.0, 0.0, 24.0),
		Label,
		nullptr,
		DebugColor,
		static_cast<float>(LifeTimeSeconds),
		true);
}

FColor AEpisodeGroundRegion::GetDebugColor() const
{
	switch (RegionSpec.RegionType)
	{
	case EEpisodeGroundRegionType::Walkable:
		return FColor::Green;
	case EEpisodeGroundRegionType::Penalty:
		return FColor(255, 165, 0);
	case EEpisodeGroundRegionType::Blocked:
		return FColor::Red;
	default:
		return FColor::White;
	}
}

FString AEpisodeGroundRegion::GetRegionTypeLabel() const
{
	switch (RegionSpec.RegionType)
	{
	case EEpisodeGroundRegionType::Walkable:
		return TEXT("Walkable");
	case EEpisodeGroundRegionType::Penalty:
		return TEXT("Penalty");
	case EEpisodeGroundRegionType::Blocked:
		return TEXT("Blocked");
	default:
		return TEXT("Unknown");
	}
}
