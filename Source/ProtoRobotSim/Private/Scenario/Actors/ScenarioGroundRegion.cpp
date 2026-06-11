
#include "Scenario/Actors/ScenarioGroundRegion.h"

#include "Scenario/Components/ScenarioPlaceableComponent.h"

namespace
{
	const FName WalkableRegionCollisionProfileName{ TEXT("Walkable") };
	const FName PenaltyRegionCollisionProfileName{ TEXT("Penalty") };
	const FName BlockedRegionCollisionProfileName{ TEXT("Blocked") };

	FName GetGroundRegionCollisionProfileName(EScenarioGroundRegionType regionType)
	{
		switch (regionType)
		{
		case EScenarioGroundRegionType::Penalty:
			return PenaltyRegionCollisionProfileName;
		case EScenarioGroundRegionType::Blocked:
			return BlockedRegionCollisionProfileName;
		case EScenarioGroundRegionType::Walkable:
		default:
			return WalkableRegionCollisionProfileName;
		}
	}

	double GetGroundRegionCollisionCenterZCm(EScenarioGroundRegionType regionType, double collisionHeightCm)
	{
		if (regionType == EScenarioGroundRegionType::Blocked)
		{
			return collisionHeightCm * 0.5;
		}

		return -collisionHeightCm * 0.5;
	}
}

AScenarioGroundRegion::AScenarioGroundRegion()
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

	PlaceableComponent = CreateDefaultSubobject<UScenarioPlaceableComponent>(TEXT("PlaceableComponent"));
	PlaceableComponent->Category = EScenarioActorCategory::GroundRegion;
	PlaceableComponent->AuthoringRole = EScenarioPlaceableAuthoringRole::Generic;
	PlaceableComponent->bAuthoringSelectable = true;
	PlaceableComponent->bAuthoringRenamable = false;
	PlaceableComponent->bAuthoringDeletable = true;
	PlaceableComponent->bAuthoringAllowLocationEdit = true;
	PlaceableComponent->bAuthoringAllowRotationEdit = true;
	PlaceableComponent->bAuthoringAllowScaleEdit = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> cubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (cubeMeshAsset.Succeeded())
	{
		RegionBoundsComponent->SetStaticMesh(cubeMeshAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> walkableGroundMaterialAsset(TEXT("/Game/Materials/M_ScenarioGroundWalkable.M_ScenarioGroundWalkable"));
	if (walkableGroundMaterialAsset.Succeeded())
	{
		WalkableGroundMaterial = walkableGroundMaterialAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> penaltyGroundMaterialAsset(TEXT("/Game/Materials/M_ScenarioGroundPenalty.M_ScenarioGroundPenalty"));
	if (penaltyGroundMaterialAsset.Succeeded())
	{
		PenaltyGroundMaterial = penaltyGroundMaterialAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> blockedAreaMaterialAsset(TEXT("/Game/Materials/M_ScenarioGroundBlock.M_ScenarioGroundBlock"));
	if (blockedAreaMaterialAsset.Succeeded())
	{
		BlockedAreaMaterial = blockedAreaMaterialAsset.Object;
	}

	ApplyMaterialSettings();
}

void AScenarioGroundRegion::BeginPlay()
{
	Super::BeginPlay();

	ApplyMaterialSettings();
}

void AScenarioGroundRegion::ConfigureRegion(const FScenarioGroundRegionSpec& inRegionSpec)
{
	RegionSpec = inRegionSpec;
	RegionSpec.Size.X = FMath::Max(RegionSpec.Size.X, 1.0);
	RegionSpec.Size.Y = FMath::Max(RegionSpec.Size.Y, 1.0);

	if (PlaceableComponent)
	{
		PlaceableComponent->InstanceId = RegionSpec.RegionId;
	}

	SetActorLocation(RegionSpec.Center);
	SetActorRotation(FRotator(0.0, RegionSpec.YawDegrees, 0.0));

	if (!RegionSpec.CollisionTag.IsEmpty())
	{
		Tags.AddUnique(FName(*RegionSpec.CollisionTag));
	}

	const double collisionHeightCm = RegionSpec.RegionType == EScenarioGroundRegionType::Blocked
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

bool AScenarioGroundRegion::ContainsWorldLocation2D(const FVector& worldLocation) const
{
	const FVector localLocation = GetActorTransform().InverseTransformPosition(worldLocation);
	const FVector2D halfSize(RegionSpec.Size.X * 0.5, RegionSpec.Size.Y * 0.5);

	return FMath::Abs(localLocation.X) <= halfSize.X
		&& FMath::Abs(localLocation.Y) <= halfSize.Y;
}

void AScenarioGroundRegion::ApplyCollisionSettings()
{
	if (!RegionBoundsComponent) return;

	RegionBoundsComponent->SetVisibility(true);
	RegionBoundsComponent->SetCollisionProfileName(GetGroundRegionCollisionProfileName(RegionSpec.RegionType));
	RegionBoundsComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	RegionBoundsComponent->SetGenerateOverlapEvents(false);
}

void AScenarioGroundRegion::ApplyMaterialSettings()
{
	if (!RegionBoundsComponent) return;

	UMaterialInterface* selectedMaterial = nullptr;
	switch (RegionSpec.RegionType)
	{
	case EScenarioGroundRegionType::Walkable:
		selectedMaterial = WalkableGroundMaterial;
		break;
	case EScenarioGroundRegionType::Penalty:
		selectedMaterial = PenaltyGroundMaterial;
		break;
	case EScenarioGroundRegionType::Blocked:
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
