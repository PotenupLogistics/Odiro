#include "Episode/Actors/EpisodeGroundRegion.h"

#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AEpisodeGroundRegion::AEpisodeGroundRegion()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	RegionBoundsComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("RegionBoundsComponent"));
	RegionBoundsComponent->SetupAttachment(SceneRoot);
	RegionBoundsComponent->SetBoxExtent(FVector(50.0, 50.0, 50.0));
	RegionBoundsComponent->SetHiddenInGame(true);
	RegionBoundsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RegionBoundsComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	RegionBoundsComponent->SetGenerateOverlapEvents(false);

	RegionDecalComponent = CreateDefaultSubobject<UDecalComponent>(TEXT("RegionDecalComponent"));
	RegionDecalComponent->SetupAttachment(SceneRoot);
	RegionDecalComponent->SetVisibility(false);
	RegionDecalComponent->SetFadeScreenSize(0.0f);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GroundDecalMaterialAsset(TEXT("/Game/Episode/Material/M_EpisodeGroundDecal.M_EpisodeGroundDecal"));
	if (GroundDecalMaterialAsset.Succeeded())
	{
		GroundDecalMaterial = GroundDecalMaterialAsset.Object;
	}
}

void AEpisodeGroundRegion::BeginPlay()
{
	Super::BeginPlay();

	UpdateDecalVisualization();
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
	UpdateDecalVisualization();
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

void AEpisodeGroundRegion::UpdateDecalVisualization()
{
	if (!RegionDecalComponent)
	{
		return;
	}

	if (!bUseDecalVisualization || RegionSpec.ShapeType != EEpisodeGroundShapeType::Rectangle)
	{
		RegionDecalComponent->SetVisibility(false);
		return;
	}

	CreateOrUpdateDecalMaterialInstance();
	if (!GroundDecalMaterialInstance)
	{
		RegionDecalComponent->SetVisibility(false);
		return;
	}

	// UDecalComponent는 local X축 방향으로 투사하므로, local Y/Z를 지면의 Y/X 크기에 대응시킨다.
	RegionDecalComponent->DecalSize = FVector(
		DecalProjectionDepthCm,
		RegionSpec.Size.Y,
		RegionSpec.Size.X);
	RegionDecalComponent->SetRelativeLocation(FVector(0.0, 0.0, DecalZOffsetCm));
	RegionDecalComponent->SetRelativeRotation(FRotator(-90.0, 0.0, 0.0));
	RegionDecalComponent->SetVisibility(true);
}

void AEpisodeGroundRegion::CreateOrUpdateDecalMaterialInstance()
{
	if (!RegionDecalComponent || !GroundDecalMaterial)
	{
		return;
	}

	if (!GroundDecalMaterialInstance)
	{
		GroundDecalMaterialInstance = UMaterialInstanceDynamic::Create(GroundDecalMaterial, this);
		RegionDecalComponent->SetDecalMaterial(GroundDecalMaterialInstance);
	}

	GroundDecalMaterialInstance->SetVectorParameterValue(TEXT("RegionColor"), GetRegionColor());
	GroundDecalMaterialInstance->SetScalarParameterValue(TEXT("Opacity"), DecalOpacity);
}

FLinearColor AEpisodeGroundRegion::GetRegionColor() const
{
	switch (RegionSpec.RegionType)
	{
	case EEpisodeGroundRegionType::Walkable:
		return FLinearColor(0.05f, 0.85f, 0.16f, 1.0f);
	case EEpisodeGroundRegionType::Penalty:
		return FLinearColor(1.0f, 0.48f, 0.0f, 1.0f);
	case EEpisodeGroundRegionType::Blocked:
		return FLinearColor(1.0f, 0.03f, 0.02f, 1.0f);
	default:
		return FLinearColor::White;
	}
}
