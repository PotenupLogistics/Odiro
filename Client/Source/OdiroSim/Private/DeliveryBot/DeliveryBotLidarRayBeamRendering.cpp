#include "DeliveryBot/DeliveryBotLidarRayBeamRendering.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeliveryBotLidarRayBeamRendering, Log, All);

namespace
{
	// Minimum non-zero dimension used to avoid invalid beam transforms.
	constexpr double MinLidarBeamDimension = 0.001;
}

void FDeliveryBotLidarRayBeamRendering::ConfigureBeamComponent(
	UInstancedStaticMeshComponent* Component,
	USceneComponent* Parent,
	UStaticMesh* Mesh,
	const FDeliveryBotLidarRayBeamComponentOptions& Options)
{
	if (Component == nullptr)
	{
		return;
	}

	if (Parent != nullptr)
	{
		Component->SetupAttachment(Parent);
	}
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetGenerateOverlapEvents(false);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetCastShadow(Options.bCastShadow);
	Component->SetVisibility(Options.bVisible, true);
	Component->SetHiddenInGame(Options.bHiddenInGame);
	Component->SetCanEverAffectNavigation(false);
	if (!Options.ComponentTag.IsNone())
	{
		Component->ComponentTags.AddUnique(Options.ComponentTag);
	}
	if (Mesh != nullptr)
	{
		Component->SetStaticMesh(Mesh);
	}
}

bool FDeliveryBotLidarRayBeamRendering::ApplyBeamMaterial(
	UInstancedStaticMeshComponent* Component,
	UMaterialInterface* Material)
{
	if (Component == nullptr || Material == nullptr)
	{
		return false;
	}

	if (!Material->CheckMaterialUsage(MATUSAGE_InstancedStaticMeshes))
	{
		UE_LOG(
			LogDeliveryBotLidarRayBeamRendering,
			Warning,
			TEXT("LiDAR ray material is not usable with instanced static meshes: %s"),
			*Material->GetPathName());
	}
	const int32 MaterialSlotCount = FMath::Max(1, Component->GetNumMaterials());
	for (int32 MaterialSlotIndex = 0; MaterialSlotIndex < MaterialSlotCount; ++MaterialSlotIndex)
	{
		Component->SetMaterial(MaterialSlotIndex, Material);
	}
	return true;
}

bool FDeliveryBotLidarRayBeamRendering::BuildBeamTransform(
	const FVector& StartLocationCm,
	const FVector& EndLocationCm,
	const double BeamMeshLengthCm,
	const double ThicknessScale,
	FTransform& OutTransform)
{
	const FVector RayDelta = EndLocationCm - StartLocationCm;
	const double RayLengthCm = RayDelta.Size();
	if (RayLengthCm <= MinLidarBeamDimension)
	{
		return false;
	}

	const FVector RayDirection = RayDelta / RayLengthCm;
	const FVector Midpoint = StartLocationCm + RayDelta * 0.5;
	const FRotator Rotation = FRotationMatrix::MakeFromX(RayDirection).Rotator();
	const double SafeBeamLengthCm = FMath::Max(MinLidarBeamDimension, BeamMeshLengthCm);
	const double SafeThicknessScale = FMath::Max(MinLidarBeamDimension, ThicknessScale);
	const FVector Scale(
		RayLengthCm / SafeBeamLengthCm,
		SafeThicknessScale,
		SafeThicknessScale);
	OutTransform = FTransform(Rotation, Midpoint, Scale);
	return true;
}

bool FDeliveryBotLidarRayBeamRendering::AddBeamInstance(
	UInstancedStaticMeshComponent* Component,
	const FVector& StartLocationCm,
	const FVector& EndLocationCm,
	const double BeamMeshLengthCm,
	const double ThicknessScale,
	const bool bWorldSpace)
{
	if (Component == nullptr)
	{
		return false;
	}

	FTransform BeamTransform;
	if (!BuildBeamTransform(
		StartLocationCm,
		EndLocationCm,
		BeamMeshLengthCm,
		ThicknessScale,
		BeamTransform))
	{
		return false;
	}

	Component->AddInstance(BeamTransform, bWorldSpace);
	return true;
}

void FDeliveryBotLidarRayBeamRendering::ClearBeamInstances(UInstancedStaticMeshComponent* Component)
{
	if (Component != nullptr)
	{
		Component->ClearInstances();
	}
}

void FDeliveryBotLidarRayBeamRendering::SetBeamComponentVisible(
	UInstancedStaticMeshComponent* Component,
	const bool bVisible)
{
	if (Component != nullptr)
	{
		Component->SetVisibility(bVisible, true);
		Component->SetHiddenInGame(!bVisible);
	}
}

void FDeliveryBotLidarRayBeamRendering::MarkBeamRenderStateDirty(UInstancedStaticMeshComponent* Component)
{
	if (Component != nullptr)
	{
		Component->MarkRenderStateDirty();
	}
}
