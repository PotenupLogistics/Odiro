#include "DeliveryBot/Actor/DeliveryBotLidarRayReviewActor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeliveryBotLidarRayReview, Log, All);

namespace
{
	const TCHAR* LidarRayBeamMeshPath =
		TEXT("/Script/Engine.StaticMesh'/Game/Models/DeliveryBot/SM_LiDARLay.SM_LiDARLay'");
	const TCHAR* LidarRayHitMaterialPath =
		TEXT("/Script/Engine.Material'/Game/Materials/M_LidarRayHit.M_LidarRayHit'");
	const TCHAR* LidarRayMissMaterialPath =
		TEXT("/Script/Engine.Material'/Game/Materials/M_LidarRayMiss.M_LidarRayMiss'");

	// Quantized replay ray segment key used to skip duplicate beam instances.
	struct FReplayLidarRaySegmentKey
	{
		// Quantized start and end coordinates packed as X/Y/Z integer components.
		FIntVector Start;

		// Quantized segment end coordinates.
		FIntVector End;

		// Whether the segment is rendered by the hit or miss component.
		bool bHit = false;

		// Returns true when two quantized replay ray segments represent the same beam.
		bool operator==(const FReplayLidarRaySegmentKey& Other) const
		{
			return Start == Other.Start
				&& End == Other.End
				&& bHit == Other.bHit;
		}
	};

	// Hashes one quantized replay ray segment key for duplicate detection.
	uint32 GetTypeHash(const FReplayLidarRaySegmentKey& Key)
	{
		uint32 Hash = static_cast<uint32>(Key.Start.X);
		Hash = HashCombine(Hash, static_cast<uint32>(Key.Start.Y));
		Hash = HashCombine(Hash, static_cast<uint32>(Key.Start.Z));
		Hash = HashCombine(Hash, static_cast<uint32>(Key.End.X));
		Hash = HashCombine(Hash, static_cast<uint32>(Key.End.Y));
		Hash = HashCombine(Hash, static_cast<uint32>(Key.End.Z));
		Hash = HashCombine(Hash, Key.bHit ? 0x85ebca6bu : 0xc2b2ae35u);
		return Hash;
	}

	// Converts one world-space coordinate to a quantized grid coordinate.
	int32 QuantizeReplayRayCoordinate(
		const double CoordinateCm,
		const double GridCm)
	{
		return FMath::RoundToInt(CoordinateCm / GridCm);
	}

	// Converts one location to a quantized grid location for duplicate detection.
	FIntVector QuantizeReplayRayLocation(
		const FVector& LocationCm,
		const double GridCm)
	{
		return FIntVector(
			QuantizeReplayRayCoordinate(LocationCm.X, GridCm),
			QuantizeReplayRayCoordinate(LocationCm.Y, GridCm),
			QuantizeReplayRayCoordinate(LocationCm.Z, GridCm));
	}

	// Configures one instanced mesh component for replay-only LiDAR ray rendering.
	void ConfigureReplayRayInstanceComponent(
		UInstancedStaticMeshComponent* Component,
		USceneComponent* Parent)
	{
		if (Component == nullptr)
		{
			return;
		}

		Component->SetupAttachment(Parent);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCastShadow(false);
		Component->SetVisibility(false, true);
		Component->SetCanEverAffectNavigation(false);
	}

	// Ensures the material has an instanced-static-mesh shader permutation before assignment.
	void EnsureReplayRayMaterialUsage(
		UMaterialInterface* Material,
		const TCHAR* MaterialPath)
	{
		if (Material == nullptr)
		{
			return;
		}

		if (!Material->CheckMaterialUsage(MATUSAGE_InstancedStaticMeshes))
		{
			UE_LOG(
				LogDeliveryBotLidarRayReview,
				Warning,
				TEXT("LiDAR ray material is not usable with instanced static meshes: %s"),
				MaterialPath);
		}
	}

	// Returns true when every vector component is finite.
	bool IsFiniteReplayLidarRayVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X)
			&& FMath::IsFinite(Value.Y)
			&& FMath::IsFinite(Value.Z);
	}
}

ADeliveryBotLidarRayReviewActor::ADeliveryBotLidarRayReviewActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	HitRayInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HitRayInstances"));
	MissRayInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("MissRayInstances"));
	ConfigureReplayRayInstanceComponent(HitRayInstances, SceneRoot);
	ConfigureReplayRayInstanceComponent(MissRayInstances, SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BeamMeshFinder(LidarRayBeamMeshPath);
	if (BeamMeshFinder.Succeeded())
	{
		HitRayInstances->SetStaticMesh(BeamMeshFinder.Object);
		MissRayInstances->SetStaticMesh(BeamMeshFinder.Object);
	}
	else
	{
		UE_LOG(
			LogDeliveryBotLidarRayReview,
			Warning,
			TEXT("Failed to load LiDAR ray beam mesh: %s"),
			LidarRayBeamMeshPath);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HitMaterialFinder(LidarRayHitMaterialPath);
	if (HitMaterialFinder.Succeeded())
	{
		EnsureReplayRayMaterialUsage(HitMaterialFinder.Object, LidarRayHitMaterialPath);
		HitRayInstances->SetMaterial(0, HitMaterialFinder.Object);
	}
	else
	{
		UE_LOG(
			LogDeliveryBotLidarRayReview,
			Warning,
			TEXT("Failed to load LiDAR hit material: %s"),
			LidarRayHitMaterialPath);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MissMaterialFinder(LidarRayMissMaterialPath);
	if (MissMaterialFinder.Succeeded())
	{
		EnsureReplayRayMaterialUsage(MissMaterialFinder.Object, LidarRayMissMaterialPath);
		MissRayInstances->SetMaterial(0, MissMaterialFinder.Object);
	}
	else
	{
		UE_LOG(
			LogDeliveryBotLidarRayReview,
			Warning,
			TEXT("Failed to load LiDAR miss material: %s"),
			LidarRayMissMaterialPath);
	}
}

void ADeliveryBotLidarRayReviewActor::ApplyLidarRayFrame(
	const FEpisodeLidarRayFrame* RayFrame,
	const FEpisodeLidarRayReplayManifest& /*Manifest*/,
	const FVector& ReplayWorldOffset)
{
	ClearLidarRays();
	if (!bLidarRaysVisible
		|| RayFrame == nullptr
		|| RayFrame->Rays.IsEmpty()
		|| HitRayInstances == nullptr
		|| MissRayInstances == nullptr)
	{
		return;
	}

	const int32 RayCount = RayFrame->Rays.Num();
	const int32 EffectiveMaxVisibleRays = FMath::Max(1, MaxVisibleRays);
	const int32 RayStep = FMath::Max(1, FMath::CeilToInt(static_cast<float>(RayCount) / EffectiveMaxVisibleRays));
	const double SafeDuplicateRayMergeGridCm = FMath::Max(0.0, DuplicateRayMergeGridCm);
	TSet<FReplayLidarRaySegmentKey> RenderedRaySegments;
	if (SafeDuplicateRayMergeGridCm > 0.0)
	{
		RenderedRaySegments.Reserve(EffectiveMaxVisibleRays);
	}

	for (int32 RayIndex = 0; RayIndex < RayCount; RayIndex += RayStep)
	{
		const FEpisodeLidarRaySample& Ray = RayFrame->Rays[RayIndex];
		if (!ShouldDrawRay(Ray))
		{
			continue;
		}

		if (SafeDuplicateRayMergeGridCm > 0.0)
		{
			const FReplayLidarRaySegmentKey SegmentKey{
				QuantizeReplayRayLocation(Ray.StartLocationCm, SafeDuplicateRayMergeGridCm),
				QuantizeReplayRayLocation(ResolveRayEndLocation(Ray), SafeDuplicateRayMergeGridCm),
				Ray.bHit};
			if (RenderedRaySegments.Contains(SegmentKey))
			{
				continue;
			}
			RenderedRaySegments.Add(SegmentKey);
		}

		FTransform RayTransform;
		if (!TryBuildRayInstanceTransform(Ray, ReplayWorldOffset, RayTransform))
		{
			continue;
		}

		UInstancedStaticMeshComponent* TargetComponent =
			Ray.bHit ? HitRayInstances.Get() : MissRayInstances.Get();
		TargetComponent->AddInstance(RayTransform, true);
		++RenderedRayCount;
	}

	HitRayInstances->MarkRenderStateDirty();
	MissRayInstances->MarkRenderStateDirty();
}

void ADeliveryBotLidarRayReviewActor::ClearLidarRays()
{
	RenderedRayCount = 0;
	if (HitRayInstances != nullptr)
	{
		HitRayInstances->ClearInstances();
	}
	if (MissRayInstances != nullptr)
	{
		MissRayInstances->ClearInstances();
	}
}

void ADeliveryBotLidarRayReviewActor::SetLidarRaysVisible(const bool bVisible)
{
	bLidarRaysVisible = bVisible;
	SetActorHiddenInGame(!bVisible);
	if (HitRayInstances != nullptr)
	{
		HitRayInstances->SetVisibility(bVisible, true);
	}
	if (MissRayInstances != nullptr)
	{
		MissRayInstances->SetVisibility(bVisible, true);
	}

	if (!bVisible)
	{
		ClearLidarRays();
	}
}

FVector ADeliveryBotLidarRayReviewActor::ResolveRayEndLocation(const FEpisodeLidarRaySample& Ray) const
{
	return Ray.bHit
		? Ray.HitLocationCm
		: Ray.EndLocationCm;
}

bool ADeliveryBotLidarRayReviewActor::TryBuildRayInstanceTransform(
	const FEpisodeLidarRaySample& Ray,
	const FVector& ReplayWorldOffset,
	FTransform& OutTransform) const
{
	const FVector StartLocation = Ray.StartLocationCm + ReplayWorldOffset;
	const FVector EndLocation = ResolveRayEndLocation(Ray) + ReplayWorldOffset;
	const FVector RayDelta = EndLocation - StartLocation;
	const double RayLengthCm = RayDelta.Size();
	constexpr double MinRayBeamDimension = 0.001;
	if (RayLengthCm <= MinRayBeamDimension)
	{
		return false;
	}

	const FVector RayDirection = RayDelta / RayLengthCm;
	const FVector Midpoint = StartLocation + RayDelta * 0.5;
	const FRotator Rotation = FRotationMatrix::MakeFromX(RayDirection).Rotator();
	const double SafeBeamLengthCm = FMath::Max(MinRayBeamDimension, RayBeamLengthCm);
	const double SafeThicknessScale = FMath::Max(MinRayBeamDimension, RayBeamThicknessScale);
	const FVector Scale(
		RayLengthCm / SafeBeamLengthCm,
		SafeThicknessScale,
		SafeThicknessScale);
	OutTransform = FTransform(Rotation, Midpoint, Scale);
	return true;
}

bool ADeliveryBotLidarRayReviewActor::ShouldDrawRay(const FEpisodeLidarRaySample& Ray) const
{
	if (!Ray.bHit && !bDrawMissRays)
	{
		return false;
	}

	return IsFiniteReplayLidarRayVector(Ray.StartLocationCm)
		&& IsFiniteReplayLidarRayVector(Ray.EndLocationCm)
		&& IsFiniteReplayLidarRayVector(Ray.HitLocationCm);
}
