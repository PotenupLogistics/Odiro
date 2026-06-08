
#include "Episode/Actors/EpisodeGroundRegion.h"

namespace
{
	const FName WalkableRegionCollisionProfileName{ TEXT("Walkable") };
	const FName PenaltyRegionCollisionProfileName{ TEXT("Penalty") };
	const FName BlockedRegionCollisionProfileName{ TEXT("Blocked") };

	FName GetGroundRegionCollisionProfileName(EEpisodeGroundRegionType regionType)
	{
		switch (regionType)
		{
		case EEpisodeGroundRegionType::Penalty:
			return PenaltyRegionCollisionProfileName;
		case EEpisodeGroundRegionType::Blocked:
			return BlockedRegionCollisionProfileName;
		case EEpisodeGroundRegionType::Walkable:
		default:
			return WalkableRegionCollisionProfileName;
		}
	}

	double GetGroundRegionCollisionCenterZCm(EEpisodeGroundRegionType regionType, double collisionHeightCm)
	{
		if (regionType == EEpisodeGroundRegionType::Blocked)
		{
			return collisionHeightCm * 0.5;
		}

		return -collisionHeightCm * 0.5;
	}
}

AEpisodeGroundRegion::AEpisodeGroundRegion()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	RegionBoundsComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RegionBoundsComponent"));
	RegionBoundsComponent->SetupAttachment(SceneRoot);
	RegionBoundsComponent->SetVisibility(false);
	RegionBoundsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RegionBoundsComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	RegionBoundsComponent->SetGenerateOverlapEvents(false);
	RegionBoundsComponent->SetMobility(EComponentMobility::Movable);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> cubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (cubeMeshAsset.Succeeded())
	{
		RegionBoundsComponent->SetStaticMesh(cubeMeshAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> walkableGroundMaterialAsset(TEXT("/Game/Materials/M_EpisodeGroundWalkable.M_EpisodeGroundWalkable"));
	if (walkableGroundMaterialAsset.Succeeded())
	{
		WalkableGroundMaterial = walkableGroundMaterialAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> penaltyGroundMaterialAsset(TEXT("/Game/Materials/M_EpisodeGroundPenalty.M_EpisodeGroundPenalty"));
	if (penaltyGroundMaterialAsset.Succeeded())
	{
		PenaltyGroundMaterial = penaltyGroundMaterialAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> blockedAreaMaterialAsset(TEXT("/Game/Materials/M_EpisodeGroundBlock.M_EpisodeGroundBlock"));
	if (blockedAreaMaterialAsset.Succeeded())
	{
		BlockedAreaMaterial = blockedAreaMaterialAsset.Object;
	}

	ApplyMaterialSettings();
}

void AEpisodeGroundRegion::BeginPlay()
{
	Super::BeginPlay();

	ApplyMaterialSettings();
}

void AEpisodeGroundRegion::ConfigureRegion(const FEpisodeGroundRegionSpec& inRegionSpec)
{
	RegionSpec = inRegionSpec;
	RegionSpec.Size.X = FMath::Max(RegionSpec.Size.X, 1.0);
	RegionSpec.Size.Y = FMath::Max(RegionSpec.Size.Y, 1.0);

	SetActorLocation(RegionSpec.Center);
	SetActorRotation(FRotator(0.0, RegionSpec.YawDegrees, 0.0));

	if (!RegionSpec.CollisionTag.IsEmpty())
	{
		Tags.AddUnique(FName(*RegionSpec.CollisionTag));
	}

	const double collisionHeightCm = RegionSpec.RegionType == EEpisodeGroundRegionType::Blocked
		? BlockedCollisionHeightCm
		: GroundCollisionThicknessCm;
	const double safeCollisionHeightCm = FMath::Max(collisionHeightCm, 1.0);

	RegionBoundsComponent->SetRelativeLocation(FVector(
		0.0,
		0.0,
		GetGroundRegionCollisionCenterZCm(RegionSpec.RegionType, safeCollisionHeightCm)));
	RegionBoundsComponent->SetRelativeScale3D(FVector(
		RegionSpec.Size.X / 100.0,
		RegionSpec.Size.Y / 100.0,
		safeCollisionHeightCm / 100.0));

	ApplyMaterialSettings();
	ApplyCollisionSettings();
}

bool AEpisodeGroundRegion::ContainsWorldLocation2D(const FVector& worldLocation) const
{
	const FVector localLocation = GetActorTransform().InverseTransformPosition(worldLocation);
	const FVector2D halfSize(RegionSpec.Size.X * 0.5, RegionSpec.Size.Y * 0.5);

	return FMath::Abs(localLocation.X) <= halfSize.X
		&& FMath::Abs(localLocation.Y) <= halfSize.Y;
}

void AEpisodeGroundRegion::ApplyCollisionSettings()
{
	if (!RegionBoundsComponent) return;

	RegionBoundsComponent->SetVisibility(true);
	RegionBoundsComponent->SetCollisionProfileName(GetGroundRegionCollisionProfileName(RegionSpec.RegionType));
	RegionBoundsComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	RegionBoundsComponent->SetGenerateOverlapEvents(false);
}

void AEpisodeGroundRegion::ApplyMaterialSettings()
{
	if (!RegionBoundsComponent) return;

	UMaterialInterface* selectedMaterial = nullptr;
	switch (RegionSpec.RegionType)
	{
	case EEpisodeGroundRegionType::Walkable:
		selectedMaterial = WalkableGroundMaterial;
		break;
	case EEpisodeGroundRegionType::Penalty:
		selectedMaterial = PenaltyGroundMaterial;
		break;
	case EEpisodeGroundRegionType::Blocked:
		selectedMaterial = BlockedAreaMaterial;
		break;
	default:
		break;
	}

	if (selectedMaterial)
	{
		RegionBoundsComponent->SetMaterial(0, selectedMaterial);
	}
}
