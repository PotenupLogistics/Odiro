#include "Episode/Actors/EpisodeGroundRegion.h"

#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

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

	RegionDecalComponent = CreateDefaultSubobject<UDecalComponent>(TEXT("RegionDecalComponent"));
	RegionDecalComponent->SetupAttachment(SceneRoot);
	RegionDecalComponent->SetVisibility(false);
	RegionDecalComponent->SetFadeScreenSize(0.0f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshAsset.Succeeded())
	{
		RegionBoundsComponent->SetStaticMesh(CubeMeshAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GroundDecalMaterialAsset(TEXT("/Game/Episode/Material/M_EpisodeGroundDecal.M_EpisodeGroundDecal"));
	if (GroundDecalMaterialAsset.Succeeded())
	{
		GroundDecalMaterial = GroundDecalMaterialAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BlockedAreaMaterialAsset(TEXT("/Game/Episode/Material/MI_EpisodeBlockArea.MI_EpisodeBlockArea"));
	if (BlockedAreaMaterialAsset.Succeeded())
	{
		BlockedAreaMaterial = BlockedAreaMaterialAsset.Object;
		RegionBoundsComponent->SetMaterial(0, BlockedAreaMaterial);
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

	RegionBoundsComponent->SetRelativeLocation(FVector(0.0, 0.0, BlockedCollisionHeightCm * 0.5));
	RegionBoundsComponent->SetRelativeScale3D(FVector(
		RegionSpec.Size.X / 100.0,
		RegionSpec.Size.Y / 100.0,
		BlockedCollisionHeightCm / 100.0));
	if (BlockedAreaMaterial)
	{
		RegionBoundsComponent->SetMaterial(0, BlockedAreaMaterial);
	}

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
	if (!RegionBoundsComponent) return;


	if (RegionSpec.RegionType == EEpisodeGroundRegionType::Blocked)
	{
		RegionBoundsComponent->SetVisibility(true);
		RegionBoundsComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		RegionBoundsComponent->SetCollisionObjectType(ECC_WorldStatic);
		RegionBoundsComponent->SetCollisionResponseToAllChannels(ECR_Block);
		return;
	}

	RegionBoundsComponent->SetVisibility(false);
	RegionBoundsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RegionBoundsComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
}

void AEpisodeGroundRegion::UpdateDecalVisualization()
{
	if (!RegionDecalComponent) return;


	if (!bUseDecalVisualization
		|| RegionSpec.ShapeType != EEpisodeGroundShapeType::Rectangle
		|| RegionSpec.RegionType == EEpisodeGroundRegionType::Blocked)
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

	// JSON 단위를 좀더 sementic하게 만들기 위해 지면 영역 X/Y 너비의 절반을 받아서 그림.
	RegionDecalComponent->DecalSize = FVector(
		DecalProjectionDepthCm,
		RegionSpec.Size.Y * 0.5,
		RegionSpec.Size.X * 0.5);
	RegionDecalComponent->SetRelativeLocation(FVector(0.0, 0.0, DecalZOffsetCm));
	RegionDecalComponent->SetRelativeRotation(FRotator(-90.0, 0.0, 0.0));
	RegionDecalComponent->SetVisibility(true);
}

void AEpisodeGroundRegion::CreateOrUpdateDecalMaterialInstance()
{
	if (!RegionDecalComponent || !GroundDecalMaterial) return;

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
