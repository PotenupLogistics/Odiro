#include "Scenario/Replay/ScenarioReplayRouteMarkerActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const TCHAR* ReplayTopDownRouteMarkerMeshPath = TEXT("/Game/Models/GoalPoint/3DTarget.3DTarget");
	const TCHAR* ReplayBillboardRouteMarkerMeshPath = TEXT("/Game/Models/GoalPoint/SM_GoalPoint.SM_GoalPoint");
	const TCHAR* ReplayStartMarkerMaterialPath =
		TEXT("/Game/Models/GoalPoint/MI_ScenarioPointBlue.MI_ScenarioPointBlue");
	const TCHAR* ReplayGoalMarkerMaterialPath =
		TEXT("/Game/Models/GoalPoint/MI_ScenarioPointRed.MI_ScenarioPointRed");
	const FLinearColor ReplayStartMarkerColor(0.0f, 0.48f, 1.0f, 1.0f);
	const FLinearColor ReplayGoalMarkerColor(1.0f, 0.03f, 0.03f, 1.0f);
	const FName MaterialColorParameterName(TEXT("Color"));
	const FName MaterialBaseColorParameterName(TEXT("BaseColor"));
	const FName MaterialTintColorParameterName(TEXT("TintColor"));
	const FName MaterialOpacityParameterName(TEXT("Opacity"));
	const FName MaterialAlphaParameterName(TEXT("Alpha"));
}

// Creates marker mesh components and loads default replay marker assets.
AScenarioReplayRouteMarkerActor::AScenarioReplayRouteMarkerActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> TopDownMarkerMeshFinder(
		ReplayTopDownRouteMarkerMeshPath);
	if (TopDownMarkerMeshFinder.Succeeded())
	{
		TopDownMarkerMesh = TopDownMarkerMeshFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BillboardMarkerMeshFinder(
		ReplayBillboardRouteMarkerMeshPath);
	if (BillboardMarkerMeshFinder.Succeeded())
	{
		BillboardMarkerMesh = BillboardMarkerMeshFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> StartMarkerMaterialFinder(
		ReplayStartMarkerMaterialPath);
	if (StartMarkerMaterialFinder.Succeeded())
	{
		StartMarkerMaterial = StartMarkerMaterialFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GoalMarkerMaterialFinder(
		ReplayGoalMarkerMaterialPath);
	if (GoalMarkerMaterialFinder.Succeeded())
	{
		GoalMarkerMaterial = GoalMarkerMaterialFinder.Object;
	}

	StartMarkerComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StartMarkerComponent"));
	StartMarkerComponent->SetupAttachment(SceneRoot);
	ConfigureMarkerMeshComponent(StartMarkerComponent, StartMarkerMaterial, ReplayStartMarkerColor);

	GoalMarkerComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GoalMarkerComponent"));
	GoalMarkerComponent->SetupAttachment(SceneRoot);
	ConfigureMarkerMeshComponent(GoalMarkerComponent, GoalMarkerMaterial, ReplayGoalMarkerColor);
}

// Applies route endpoint locations and hides missing endpoints.
void AScenarioReplayRouteMarkerActor::ConfigureRouteMarkers(
	const FVector& StartLocationCm,
	const bool bHasStartLocation,
	const FVector& GoalLocationCm,
	const bool bHasGoalLocation)
{
	bHasStartMarker = bHasStartLocation;
	bHasGoalMarker = bHasGoalLocation;
	StartMarkerWorldLocationCm = StartLocationCm;
	GoalMarkerWorldLocationCm = GoalLocationCm;

	ApplyMarkerTransform(StartMarkerComponent, StartMarkerWorldLocationCm, bHasStartMarker);
	ApplyMarkerTransform(GoalMarkerComponent, GoalMarkerWorldLocationCm, bHasGoalMarker);
	SetRouteMarkersVisible(bRouteMarkersVisible);
}

// Shows or hides all available route markers without destroying them.
void AScenarioReplayRouteMarkerActor::SetRouteMarkersVisible(const bool bVisible)
{
	bRouteMarkersVisible = bVisible;
	SetActorHiddenInGame(!bVisible);

	if (StartMarkerComponent != nullptr)
	{
		const bool bShowStartMarker = bVisible && bHasStartMarker;
		StartMarkerComponent->SetVisibility(bShowStartMarker, true);
		StartMarkerComponent->SetHiddenInGame(!bShowStartMarker, true);
	}

	if (GoalMarkerComponent != nullptr)
	{
		const bool bShowGoalMarker = bVisible && bHasGoalMarker;
		GoalMarkerComponent->SetVisibility(bShowGoalMarker, true);
		GoalMarkerComponent->SetHiddenInGame(!bShowGoalMarker, true);
	}
}

// Selects the perspective pin mesh with yaw billboard rotation or the top-down target mesh.
void AScenarioReplayRouteMarkerActor::SetBillboardMarkersEnabled(const bool bEnabled)
{
	if (bUseBillboardMarkerPresentation == bEnabled)
	{
		return;
	}

	bUseBillboardMarkerPresentation = bEnabled;
	RefreshMarkerPresentation();
}

// Rotates perspective marker meshes to face the active replay camera.
void AScenarioReplayRouteMarkerActor::FaceCameraLocation(const FVector& CameraLocationCm)
{
	if (!bUseBillboardMarkerPresentation)
	{
		return;
	}

	auto FaceMarkerComponent = [this, &CameraLocationCm](UStaticMeshComponent* MeshComponent)
	{
		if (MeshComponent == nullptr || !MeshComponent->IsVisible())
		{
			return;
		}

		const FVector ToCamera = CameraLocationCm - MeshComponent->GetComponentLocation();
		if (ToCamera.IsNearlyZero())
		{
			return;
		}

		const FRotator CameraFacingYaw(
			0.0,
			ToCamera.Rotation().Yaw + BillboardMarkerYawOffsetDegrees,
			0.0);
		MeshComponent->SetWorldRotation(CameraFacingYaw);
	};

	FaceMarkerComponent(StartMarkerComponent);
	FaceMarkerComponent(GoalMarkerComponent);
}

// Applies shared mesh, material, collision, and rendering settings to one marker component.
void AScenarioReplayRouteMarkerActor::ConfigureMarkerMeshComponent(
	UStaticMeshComponent* MeshComponent,
	UMaterialInterface* MarkerMaterial,
	const FLinearColor& MarkerColor) const
{
	if (MeshComponent == nullptr)
	{
		return;
	}

	MeshComponent->SetMobility(EComponentMobility::Movable);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetGenerateOverlapEvents(false);
	MeshComponent->SetCastShadow(false);
	MeshComponent->bCastDynamicShadow = false;
	MeshComponent->bDisallowNanite = true;
	MeshComponent->SetReceivesDecals(false);
	MeshComponent->SetVisibility(false, true);
	MeshComponent->SetHiddenInGame(true, true);

	ApplyMarkerVisuals(MeshComponent, MarkerMaterial, MarkerColor);
}

// Applies the active camera-mode mesh and marker material to one marker component.
void AScenarioReplayRouteMarkerActor::ApplyMarkerVisuals(
	UStaticMeshComponent* MeshComponent,
	UMaterialInterface* MarkerMaterial,
	const FLinearColor& MarkerColor) const
{
	if (MeshComponent == nullptr)
	{
		return;
	}

	if (UStaticMesh* ActiveMesh = GetActiveMarkerMesh())
	{
		MeshComponent->SetStaticMesh(ActiveMesh);
		MeshComponent->SetRelativeScale3D(ResolveMarkerScale());
	}

	ApplyOpaqueMarkerMaterial(MeshComponent, MarkerMaterial, MarkerColor);
}

// Applies one alpha-1 dynamic marker material to every mesh material slot.
void AScenarioReplayRouteMarkerActor::ApplyOpaqueMarkerMaterial(
	UStaticMeshComponent* MeshComponent,
	UMaterialInterface* MarkerMaterial,
	const FLinearColor& MarkerColor) const
{
	if (MeshComponent == nullptr || MarkerMaterial == nullptr)
	{
		return;
	}

	UMaterialInstanceDynamic* DynamicMaterial =
		UMaterialInstanceDynamic::Create(MarkerMaterial, MeshComponent);
	if (DynamicMaterial == nullptr)
	{
		return;
	}

	const FLinearColor OpaqueMarkerColor(MarkerColor.R, MarkerColor.G, MarkerColor.B, 1.0f);
	DynamicMaterial->SetVectorParameterValue(MaterialColorParameterName, OpaqueMarkerColor);
	DynamicMaterial->SetVectorParameterValue(MaterialBaseColorParameterName, OpaqueMarkerColor);
	DynamicMaterial->SetVectorParameterValue(MaterialTintColorParameterName, OpaqueMarkerColor);
	DynamicMaterial->SetScalarParameterValue(MaterialOpacityParameterName, 1.0f);
	DynamicMaterial->SetScalarParameterValue(MaterialAlphaParameterName, 1.0f);

	const int32 MaterialSlotCount = FMath::Max(1, MeshComponent->GetNumMaterials());
	for (int32 MaterialSlotIndex = 0; MaterialSlotIndex < MaterialSlotCount; ++MaterialSlotIndex)
	{
		MeshComponent->SetMaterial(MaterialSlotIndex, DynamicMaterial);
	}
}

// Rebuilds both marker components after the camera-mode mesh presentation changes.
void AScenarioReplayRouteMarkerActor::RefreshMarkerPresentation()
{
	ApplyMarkerVisuals(StartMarkerComponent, StartMarkerMaterial, ReplayStartMarkerColor);
	ApplyMarkerVisuals(GoalMarkerComponent, GoalMarkerMaterial, ReplayGoalMarkerColor);
	ApplyMarkerTransform(StartMarkerComponent, StartMarkerWorldLocationCm, bHasStartMarker);
	ApplyMarkerTransform(GoalMarkerComponent, GoalMarkerWorldLocationCm, bHasGoalMarker);
	SetRouteMarkersVisible(bRouteMarkersVisible);
}

// Places one marker component at the route endpoint.
void AScenarioReplayRouteMarkerActor::ApplyMarkerTransform(
	UStaticMeshComponent* MeshComponent,
	const FVector& WorldLocationCm,
	const bool bEndpointAvailable) const
{
	if (MeshComponent == nullptr)
	{
		return;
	}

	MeshComponent->SetWorldLocation(WorldLocationCm + FVector(0.0, 0.0, GetActiveMarkerWorldZOffsetCm()));
	MeshComponent->SetWorldRotation(FRotator::ZeroRotator);
	MeshComponent->SetWorldScale3D(ResolveMarkerScale());
	MeshComponent->SetVisibility(bRouteMarkersVisible && bEndpointAvailable, true);
	MeshComponent->SetHiddenInGame(!(bRouteMarkersVisible && bEndpointAvailable), true);
}

// Returns the mesh currently selected by the replay camera mode.
UStaticMesh* AScenarioReplayRouteMarkerActor::GetActiveMarkerMesh() const
{
	if (bUseBillboardMarkerPresentation && BillboardMarkerMesh != nullptr)
	{
		return BillboardMarkerMesh.Get();
	}

	if (!bUseBillboardMarkerPresentation && TopDownMarkerMesh != nullptr)
	{
		return TopDownMarkerMesh.Get();
	}

	return TopDownMarkerMesh != nullptr
		? TopDownMarkerMesh.Get()
		: BillboardMarkerMesh.Get();
}

// Returns the largest target marker dimension for the active mesh mode.
float AScenarioReplayRouteMarkerActor::GetActiveMarkerVisualSizeCm() const
{
	return bUseBillboardMarkerPresentation
		? BillboardMarkerVisualSizeCm
		: TopDownMarkerVisualSizeCm;
}

// Returns the vertical placement offset for the active mesh mode.
float AScenarioReplayRouteMarkerActor::GetActiveMarkerWorldZOffsetCm() const
{
	return bUseBillboardMarkerPresentation
		? BillboardMarkerWorldZOffsetCm
		: TopDownMarkerWorldZOffsetCm;
}

// Returns a uniform scale that normalizes the authored mesh to the requested visual size.
FVector AScenarioReplayRouteMarkerActor::ResolveMarkerScale() const
{
	UStaticMesh* ActiveMesh = GetActiveMarkerMesh();
	if (ActiveMesh == nullptr)
	{
		return FVector::OneVector;
	}

	const FBoxSphereBounds MeshBounds = ActiveMesh->GetBounds();
	const FVector MeshSizeCm = MeshBounds.BoxExtent * 2.0;
	const double LargestDimensionCm = FMath::Max3(MeshSizeCm.X, MeshSizeCm.Y, MeshSizeCm.Z);
	if (LargestDimensionCm <= UE_SMALL_NUMBER)
	{
		return FVector::OneVector;
	}

	const double UniformScale = FMath::Max(1.0f, GetActiveMarkerVisualSizeCm()) / LargestDimensionCm;
	return FVector(UniformScale);
}
