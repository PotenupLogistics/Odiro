#include "Scenario/Editor/ScenarioCorridorHandleActor.h"

#include "Components/SceneComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Scenario/Components/ScenarioPlaceableComponent.h"

AScenarioCorridorHandleActor::AScenarioCorridorHandleActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	HandleMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandleMeshComponent"));
	HandleMeshComponent->SetupAttachment(SceneRoot);
	HandleMeshComponent->SetMobility(EComponentMobility::Movable);
	HandleMeshComponent->SetVisibility(false, true);
	HandleMeshComponent->SetHiddenInGame(true);
	HandleMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HandleMeshComponent->SetCollisionObjectType(ECC_WorldDynamic);
	HandleMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	HandleMeshComponent->SetGenerateOverlapEvents(false);
	HandleMeshComponent->SetCastShadow(false);

	SegmentSplineMeshComponent = CreateDefaultSubobject<USplineMeshComponent>(TEXT("SegmentSplineMeshComponent"));
	SegmentSplineMeshComponent->SetupAttachment(SceneRoot);
	SegmentSplineMeshComponent->SetMobility(EComponentMobility::Movable);
	SegmentSplineMeshComponent->SetVisibility(false, true);
	SegmentSplineMeshComponent->SetHiddenInGame(true);
	SegmentSplineMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SegmentSplineMeshComponent->SetCollisionObjectType(ECC_WorldDynamic);
	SegmentSplineMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
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
	(void)InSegmentLengthCm;
	HandleType = EScenarioCorridorHandleType::Segment;
	VertexIndex = INDEX_NONE;
	SegmentIndex = InSegmentIndex;
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
			SegmentSplineMeshComponent->SetVisibility(false, true);
			SegmentSplineMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		PlaceableComponent->bAuthoringAllowRotationEdit = true;
	}
	else
	{
		HandleMeshComponent->SetVisibility(false, true);
		HandleMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (SegmentSplineMeshComponent)
		{
			SegmentSplineMeshComponent->SetVisibility(false, true);
			SegmentSplineMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		PlaceableComponent->bAuthoringAllowRotationEdit = false;
	}
}
