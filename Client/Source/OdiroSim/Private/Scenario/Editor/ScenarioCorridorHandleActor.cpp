#include "Scenario/Editor/ScenarioCorridorHandleActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Scenario/Components/ScenarioPlaceableComponent.h"
#include "UObject/ConstructorHelpers.h"

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

	static ConstructorHelpers::FObjectFinder<UStaticMesh> segmentMeshAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (segmentMeshAsset.Succeeded())
	{
		SegmentMesh = segmentMeshAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> handleMaterialAsset(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
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
	const FTransform& InTransform)
{
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
		HandleMeshComponent->SetStaticMesh(SegmentMesh);
		PlaceableComponent->bAuthoringAllowRotationEdit = true;
	}
	else
	{
		HandleMeshComponent->SetStaticMesh(VertexMesh);
		PlaceableComponent->bAuthoringAllowRotationEdit = false;
	}

	if (HandleMaterial)
	{
		HandleMeshComponent->SetMaterial(0, HandleMaterial);
	}
}
