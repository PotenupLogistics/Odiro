#include "Scenario/Editor/ScenarioCorridorHandleActor.h"

#include "Components/SceneComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Scenario/Components/ScenarioPlaceableComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const double BasicShapeCylinderRadiusCm = 50.0;
	const double SegmentHandleCylinderRadiusCm = 7.5;
}

AScenarioCorridorHandleActor::AScenarioCorridorHandleActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	HandleMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandleMeshComponent"));
	HandleMeshComponent->SetupAttachment(SceneRoot);
	HandleMeshComponent->SetMobility(EComponentMobility::Movable);
	HandleMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HandleMeshComponent->SetCollisionObjectType(ECC_WorldDynamic);
	HandleMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	HandleMeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	HandleMeshComponent->SetGenerateOverlapEvents(false);

	SegmentSplineMeshComponent = CreateDefaultSubobject<USplineMeshComponent>(TEXT("SegmentSplineMeshComponent"));
	SegmentSplineMeshComponent->SetupAttachment(SceneRoot);
	SegmentSplineMeshComponent->SetMobility(EComponentMobility::Movable);
	SegmentSplineMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SegmentSplineMeshComponent->SetCollisionObjectType(ECC_WorldDynamic);
	SegmentSplineMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	SegmentSplineMeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SegmentSplineMeshComponent->SetGenerateOverlapEvents(false);
	SegmentSplineMeshComponent->SetForwardAxis(ESplineMeshAxis::Z, false);
	SegmentSplineMeshComponent->SetCastShadow(false);

	PlaceableComponent = CreateDefaultSubobject<UScenarioPlaceableComponent>(TEXT("PlaceableComponent"));
	PlaceableComponent->Category = EScenarioActorCategory::GroundRegion;
	PlaceableComponent->bAuthoringSelectable = true;
	PlaceableComponent->bAuthoringRenamable = false;
	PlaceableComponent->bAuthoringDeletable = false;
	PlaceableComponent->bAuthoringAllowLocationEdit = true;
	PlaceableComponent->bAuthoringAllowScaleEdit = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> vertexMeshAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (vertexMeshAsset.Succeeded())
	{
		VertexMesh = vertexMeshAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> segmentMeshAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (segmentMeshAsset.Succeeded())
	{
		SegmentMesh = segmentMeshAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> handleMaterialAsset(
		TEXT("/Game/Materials/Gizmo/M_GizmoAxis.M_GizmoAxis"));
	if (handleMaterialAsset.Succeeded())
	{
		HandleMaterial = handleMaterialAsset.Object;
	}

	ApplyHandleVisual();
}

void AScenarioCorridorHandleActor::ConfigureVertexHandle(
	int32 InVertexIndex,
	const FString& InInstanceId,
	const FTransform& InTransform)
{
	HandleType = EScenarioCorridorHandleType::Vertex;
	VertexIndex = InVertexIndex;
	SegmentIndex = INDEX_NONE;
	SetActorTransform(InTransform);

	if (PlaceableComponent)
	{
		PlaceableComponent->InstanceId = InInstanceId;
		PlaceableComponent->AssetId = TEXT("corridor_vertex_handle");
		PlaceableComponent->AuthoringRole = EScenarioPlaceableAuthoringRole::CorridorVertexHandle;
	}

	ApplyHandleVisual();
}

void AScenarioCorridorHandleActor::ConfigureSegmentHandle(
	int32 InSegmentIndex,
	const FString& InInstanceId,
	const FTransform& InTransform,
	double InSegmentLengthCm)
{
	HandleType = EScenarioCorridorHandleType::Segment;
	VertexIndex = INDEX_NONE;
	SegmentIndex = InSegmentIndex;
	SegmentLengthCm = FMath::Max(InSegmentLengthCm, 0.0);
	SetActorTransform(InTransform);

	if (PlaceableComponent)
	{
		PlaceableComponent->InstanceId = InInstanceId;
		PlaceableComponent->AssetId = TEXT("corridor_segment_handle");
		PlaceableComponent->AuthoringRole = EScenarioPlaceableAuthoringRole::CorridorSegmentHandle;
	}

	ApplyHandleVisual();
}

void AScenarioCorridorHandleActor::ApplyHandleVisual()
{
	if (!HandleMeshComponent || !PlaceableComponent)
	{
		return;
	}

	if (HandleType == EScenarioCorridorHandleType::Segment)
	{
		HandleMeshComponent->SetVisibility(false, true);
		HandleMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (SegmentSplineMeshComponent)
		{
			const double halfLengthCm = SegmentLengthCm * 0.5;
			const FVector startLocation(-halfLengthCm, 0.0, 0.0);
			const FVector endLocation(halfLengthCm, 0.0, 0.0);
			const FVector tangent(FMath::Max(SegmentLengthCm, 1.0), 0.0, 0.0);
			const float radiusScale = static_cast<float>(SegmentHandleCylinderRadiusCm / BasicShapeCylinderRadiusCm);
			SegmentSplineMeshComponent->SetVisibility(true, true);
			SegmentSplineMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			SegmentSplineMeshComponent->SetStaticMesh(SegmentMesh);
			SegmentSplineMeshComponent->SetForwardAxis(ESplineMeshAxis::Z, false);
			SegmentSplineMeshComponent->SetStartAndEnd(startLocation, tangent, endLocation, tangent, false);
			SegmentSplineMeshComponent->SetStartScale(FVector2D(radiusScale, radiusScale), false);
			SegmentSplineMeshComponent->SetEndScale(FVector2D(radiusScale, radiusScale), false);
			if (HandleMaterial)
			{
				SegmentSplineMeshComponent->SetMaterial(0, HandleMaterial);
			}
			SegmentSplineMeshComponent->UpdateMesh();
		}
		PlaceableComponent->bAuthoringAllowRotationEdit = true;
	}
	else
	{
		HandleMeshComponent->SetVisibility(true, true);
		HandleMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		HandleMeshComponent->SetStaticMesh(VertexMesh);
		if (SegmentSplineMeshComponent)
		{
			SegmentSplineMeshComponent->SetVisibility(false, true);
			SegmentSplineMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		PlaceableComponent->bAuthoringAllowRotationEdit = false;
	}

	if (HandleMaterial)
	{
		HandleMeshComponent->SetMaterial(0, HandleMaterial);
	}
}
