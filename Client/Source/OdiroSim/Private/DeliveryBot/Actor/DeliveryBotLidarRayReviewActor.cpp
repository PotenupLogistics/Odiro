#include "DeliveryBot/Actor/DeliveryBotLidarRayReviewActor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "DeliveryBot/DeliveryBotLidarRayBeamRendering.h"
#include "DeliveryBot/DeliveryBotLidarRayPattern.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Shared/EpisodeLidarRayReplayDataTypes.h"
#include "Shared/EpisodeReplayDataTypes.h"
#include "Shared/Struct/DeliveryBot/Perception/DeliveryBotLidarSensorInfo.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeliveryBotLidarRayReview, Log, All);

namespace
{
	const TCHAR* LidarRayBeamMeshPath =
		TEXT("/Script/Engine.StaticMesh'/Game/Models/DeliveryBot/SM_LiDARLay.SM_LiDARLay'");
	const TCHAR* PreviewLidarRayMaterialPath =
		TEXT("/Script/Engine.Material'/Game/Materials/M_RobotPreview_LidarRay.M_RobotPreview_LidarRay'");
	const TCHAR* PreviewLidarRangeMaterialPath =
		TEXT("/Script/Engine.Material'/Game/Materials/M_RobotPreview_LidarRange.M_RobotPreview_LidarRange'");
	const TCHAR* PreviewLidarPrimaryRayMaterialPath =
		TEXT("/Script/Engine.MaterialInstanceConstant'/Game/Materials/MI_RobotPreview_LidarRay_Primary.MI_RobotPreview_LidarRay_Primary'");
	const TCHAR* PreviewLidarSecondaryRayMaterialPath =
		TEXT("/Script/Engine.MaterialInstanceConstant'/Game/Materials/MI_RobotPreview_LidarRay_Secondary.MI_RobotPreview_LidarRay_Secondary'");
	const TCHAR* PreviewLidarThreeDRayMaterialPath =
		TEXT("/Script/Engine.MaterialInstanceConstant'/Game/Materials/MI_RobotPreview_LidarRay_3D.MI_RobotPreview_LidarRay_3D'");
	const TCHAR* PreviewLidarOusterActiveRayMaterialPath =
		TEXT("/Script/Engine.MaterialInstanceConstant'/Game/Materials/MI_RobotPreview_LidarRay_OusterActive.MI_RobotPreview_LidarRay_OusterActive'");
	const TCHAR* PreviewLidarOusterFullRayMaterialPath =
		TEXT("/Script/Engine.MaterialInstanceConstant'/Game/Materials/MI_RobotPreview_LidarRay_OusterFull.MI_RobotPreview_LidarRay_OusterFull'");
	const TCHAR* PreviewLidarFrontBoundaryMaterialPath =
		TEXT("/Script/Engine.MaterialInstanceConstant'/Game/Materials/MI_RobotPreview_LidarRay_Front.MI_RobotPreview_LidarRay_Front'");
	const TCHAR* PreviewLidarObstacleWarningRangeRayMaterialPath =
		TEXT("/Script/Engine.MaterialInstanceConstant'/Game/Materials/MI_RobotPreview_LidarRay_ObstacleWarning.MI_RobotPreview_LidarRay_ObstacleWarning'");
	const TCHAR* PreviewLidarScanRangeMaterialPath =
		TEXT("/Script/Engine.MaterialInstanceConstant'/Game/Materials/MI_RobotPreview_LidarRange_Max.MI_RobotPreview_LidarRange_Max'");
	const TCHAR* PreviewLidarSlowRangeMaterialPath =
		TEXT("/Script/Engine.MaterialInstanceConstant'/Game/Materials/MI_RobotPreview_LidarRange_Slow.MI_RobotPreview_LidarRange_Slow'");
	const TCHAR* PreviewLidarStopRangeMaterialPath =
		TEXT("/Script/Engine.MaterialInstanceConstant'/Game/Materials/MI_RobotPreview_LidarRange_Stop.MI_RobotPreview_LidarRange_Stop'");
	const TCHAR* PreviewLidarSlowRangeRayMaterialPath =
		TEXT("/Script/Engine.MaterialInstanceConstant'/Game/Materials/MI_RobotPreview_LidarRay_Slow.MI_RobotPreview_LidarRay_Slow'");
	const TCHAR* PreviewLidarStopRangeRayMaterialPath =
		TEXT("/Script/Engine.MaterialInstanceConstant'/Game/Materials/MI_RobotPreview_LidarRay_Stop.MI_RobotPreview_LidarRay_Stop'");
	const TCHAR* FallbackLidarRayHitMaterialPath =
		TEXT("/Script/Engine.Material'/Game/Materials/M_LidarRayHit.M_LidarRayHit'");
	const TCHAR* FallbackLidarRayMissMaterialPath =
		TEXT("/Script/Engine.Material'/Game/Materials/M_LidarRayMiss.M_LidarRayMiss'");

	constexpr int32 LidarRangeRingSegments = 64;
	constexpr int32 StandardVisible2DRayBeams = 180;
	constexpr int32 StandardVisible3DYawSamplesPerLayer = 36;
	constexpr int32 StandardVisible3DPitchLayers = 9;
	constexpr float OusterOS1ReplayVisibleSweepAngleDegree = 45.0f;
	constexpr float OusterOS1ReplayMaxVisibleSpinHz = 1.0f;

	// Loads optional preview materials without warning when a specific layer asset does not exist yet.
	UMaterialInterface* LoadOptionalMaterial(const TCHAR* MaterialPath)
	{
		return Cast<UMaterialInterface>(
			StaticLoadObject(
				UMaterialInterface::StaticClass(),
				nullptr,
				MaterialPath,
				nullptr,
				LOAD_NoWarn));
	}

	// Returns Source when it exists, otherwise returns Fallback.
	UMaterialInterface* ResolveMaterial(
		UMaterialInterface* Source,
		UMaterialInterface* Fallback)
	{
		return Source ? Source : Fallback;
	}

	// Clamps replay LiDAR settings into the same safe range used by the preview layer.
	FDeliveryBotLidarSensorConfigInfo SanitizeReplayLidarConfig(
		const FDeliveryBotLidarSensorConfigInfo& SourceConfig,
		const FEpisodeLidarRayFrame* RayFrame)
	{
		FDeliveryBotLidarSensorConfigInfo Config = SourceConfig;
		Config.ScanRangeM = FMath::Max(Config.ScanRangeM, 0.01f);
		Config.AngleStepDegree = FMath::Max(Config.AngleStepDegree, 1.0f);
		Config.SensorHeightM = FMath::Max(Config.SensorHeightM, 0.0f);
		Config.SensorForwardOffsetM = FMath::Clamp(Config.SensorForwardOffsetM, -10.0f, 10.0f);
		Config.SensorRightOffsetM = FMath::Clamp(Config.SensorRightOffsetM, -10.0f, 10.0f);
		Config.FrontHalfAngleDegree = FMath::Clamp(Config.FrontHalfAngleDegree, 0.0f, 180.0f);
		Config.StopDistanceM = FMath::Max(Config.StopDistanceM, 0.0f);
		Config.ObstacleWarningDistanceM = FMath::Max(Config.ObstacleWarningDistanceM, 0.0f);
		Config.SlowDownDistanceM = FMath::Max(Config.SlowDownDistanceM, 0.0f);
		Config.VerticalMinDegree = FMath::Clamp(Config.VerticalMinDegree, -89.0f, 89.0f);
		Config.VerticalMaxDegree = FMath::Clamp(Config.VerticalMaxDegree, -89.0f, 89.0f);
		Config.VerticalStepDegree = FMath::Max(Config.VerticalStepDegree, 1.0f);

		if (RayFrame != nullptr && !RayFrame->Rays.IsEmpty())
		{
			double MaxRecordedRangeCm = 0.0;
			for (const FEpisodeLidarRaySample& Ray : RayFrame->Rays)
			{
				MaxRecordedRangeCm = FMath::Max(
					MaxRecordedRangeCm,
					FVector::Distance(Ray.StartLocationCm, Ray.EndLocationCm));
			}

			if (MaxRecordedRangeCm > Config.ScanRangeM * 100.0f)
			{
				Config.ScanRangeM = static_cast<float>(MaxRecordedRangeCm / 100.0);
			}
		}

		return Config;
	}

	// Returns whether an OS1 ray column belongs to the replay-time rotating debug sweep.
	bool ShouldRenderReplayOusterOS1Ray(
		const FDeliveryBotLidarSensorConfigInfo& Config,
		const FDeliveryBotLidarRaySample& RaySample,
		const double ReplayTimeSeconds)
	{
		if (!FDeliveryBotLidarRayPattern::IsOusterOS1Mode(Config.LidarModeType)
			|| RaySample.ColumnIndex == INDEX_NONE)
		{
			return true;
		}

		const int32 ColumnCount = FDeliveryBotLidarRayPattern::CountYawSamples(Config);
		if (ColumnCount <= 1)
		{
			return true;
		}

		const float ColumnAngleDegree = 360.0f / static_cast<float>(ColumnCount);
		const float VisibleSweepAngleDegree = FMath::Clamp(
			OusterOS1ReplayVisibleSweepAngleDegree,
			ColumnAngleDegree,
			360.0f);
		const int32 VisibleColumnHalfWindow =
			FMath::Max(1, FMath::CeilToInt(VisibleSweepAngleDegree * 0.5f / ColumnAngleDegree));
		const float MaxGapFreeSpinHz =
			FMath::Max(Config.ScanRateHz, 0.1f) * VisibleSweepAngleDegree / 360.0f;
		const float VisibleSpinHz = FMath::Clamp(
			MaxGapFreeSpinHz,
			0.01f,
			OusterOS1ReplayMaxVisibleSpinHz);
		const double SafeReplayTimeSeconds =
			FMath::IsFinite(ReplayTimeSeconds)
				? FMath::Max(0.0, ReplayTimeSeconds)
				: 0.0;
		const float ScanPhase = FMath::Fmod(
			static_cast<float>(SafeReplayTimeSeconds) * VisibleSpinHz,
			1.0f);
		const int32 ActiveColumnIndex =
			FMath::FloorToInt(ScanPhase * static_cast<float>(ColumnCount)) % ColumnCount;
		const int32 RawColumnDistance = FMath::Abs(RaySample.ColumnIndex - ActiveColumnIndex);
		const int32 WrappedColumnDistance = FMath::Min(RawColumnDistance, ColumnCount - RawColumnDistance);
		return WrappedColumnDistance <= VisibleColumnHalfWindow;
	}
}

ADeliveryBotLidarRayReviewActor::ADeliveryBotLidarRayReviewActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	LidarPrimaryRayInstances =
		CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LidarPrimaryRayInstances"));
	LidarSecondaryRayInstances =
		CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LidarSecondaryRayInstances"));
	LidarThreeDRayInstances =
		CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LidarThreeDRayInstances"));
	LidarThreeDFullRayInstances =
		CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LidarThreeDFullRayInstances"));
	LidarRangeRingInstances =
		CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LidarRangeRingInstances"));
	LidarSlowRangeRingInstances =
		CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LidarSlowRangeRingInstances"));
	LidarStopRangeRingInstances =
		CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LidarStopRangeRingInstances"));
	LidarObstacleWarningRangeRingInstances =
		CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LidarObstacleWarningRangeRingInstances"));
	LidarSlowRangeRayInstances =
		CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LidarSlowRangeRayInstances"));
	LidarStopRangeRayInstances =
		CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LidarStopRangeRayInstances"));
	LidarObstacleWarningRangeRayInstances =
		CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LidarObstacleWarningRangeRayInstances"));
	LidarFrontBoundaryInstances =
		CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LidarFrontBoundaryInstances"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BeamMeshFinder(LidarRayBeamMeshPath);
	UStaticMesh* BeamMesh = BeamMeshFinder.Succeeded() ? BeamMeshFinder.Object : nullptr;
	if (!BeamMesh)
	{
		UE_LOG(
			LogDeliveryBotLidarRayReview,
			Warning,
			TEXT("Failed to load LiDAR ray beam mesh: %s"),
			LidarRayBeamMeshPath);
	}

	FDeliveryBotLidarRayBeamComponentOptions BeamComponentOptions;
	BeamComponentOptions.bVisible = false;
	BeamComponentOptions.bHiddenInGame = true;
	BeamComponentOptions.bCastShadow = false;
	ForEachLidarBeamComponent(
		[this, BeamMesh, &BeamComponentOptions](UInstancedStaticMeshComponent* Component)
		{
			FDeliveryBotLidarRayBeamRendering::ConfigureBeamComponent(
				Component,
				SceneRoot,
				BeamMesh,
				BeamComponentOptions);
		});

	UMaterialInterface* PreviewRayMaterial = LoadOptionalMaterial(PreviewLidarRayMaterialPath);
	UMaterialInterface* PreviewRangeMaterial = LoadOptionalMaterial(PreviewLidarRangeMaterialPath);
	UMaterialInterface* PrimaryRayMaterial = LoadOptionalMaterial(PreviewLidarPrimaryRayMaterialPath);
	UMaterialInterface* SecondaryRayMaterial = LoadOptionalMaterial(PreviewLidarSecondaryRayMaterialPath);
	UMaterialInterface* ThreeDRayMaterial = LoadOptionalMaterial(PreviewLidarThreeDRayMaterialPath);
	UMaterialInterface* OusterFullRayMaterial = LoadOptionalMaterial(PreviewLidarOusterFullRayMaterialPath);
	UMaterialInterface* FrontBoundaryMaterial = LoadOptionalMaterial(PreviewLidarFrontBoundaryMaterialPath);
	UMaterialInterface* ObstacleWarningRangeRayMaterial =
		LoadOptionalMaterial(PreviewLidarObstacleWarningRangeRayMaterialPath);
	UMaterialInterface* ScanRangeMaterial = LoadOptionalMaterial(PreviewLidarScanRangeMaterialPath);
	UMaterialInterface* SlowRangeMaterial = LoadOptionalMaterial(PreviewLidarSlowRangeMaterialPath);
	UMaterialInterface* StopRangeMaterial = LoadOptionalMaterial(PreviewLidarStopRangeMaterialPath);
	UMaterialInterface* SlowRangeRayMaterial = LoadOptionalMaterial(PreviewLidarSlowRangeRayMaterialPath);
	UMaterialInterface* StopRangeRayMaterial = LoadOptionalMaterial(PreviewLidarStopRangeRayMaterialPath);
	UMaterialInterface* FallbackHitMaterial = LoadOptionalMaterial(FallbackLidarRayHitMaterialPath);
	UMaterialInterface* FallbackMissMaterial = LoadOptionalMaterial(FallbackLidarRayMissMaterialPath);
	UMaterialInterface* RayFallback = ResolveMaterial(PreviewRayMaterial, FallbackHitMaterial);
	UMaterialInterface* RangeFallback = ResolveMaterial(PreviewRangeMaterial, RayFallback);

	FDeliveryBotLidarRayBeamRendering::ApplyBeamMaterial(
		LidarPrimaryRayInstances,
		ResolveMaterial(PrimaryRayMaterial, RayFallback));
	FDeliveryBotLidarRayBeamRendering::ApplyBeamMaterial(
		LidarSecondaryRayInstances,
		ResolveMaterial(SecondaryRayMaterial, ResolveMaterial(FallbackMissMaterial, RayFallback)));
	FDeliveryBotLidarRayBeamRendering::ApplyBeamMaterial(
		LidarThreeDRayInstances,
		ResolveMaterial(ThreeDRayMaterial, RayFallback));
	FDeliveryBotLidarRayBeamRendering::ApplyBeamMaterial(
		LidarThreeDFullRayInstances,
		ResolveMaterial(OusterFullRayMaterial, RayFallback));
	FDeliveryBotLidarRayBeamRendering::ApplyBeamMaterial(
		LidarFrontBoundaryInstances,
		ResolveMaterial(FrontBoundaryMaterial, RayFallback));
	FDeliveryBotLidarRayBeamRendering::ApplyBeamMaterial(
		LidarRangeRingInstances,
		ResolveMaterial(ScanRangeMaterial, RangeFallback));
	FDeliveryBotLidarRayBeamRendering::ApplyBeamMaterial(
		LidarSlowRangeRingInstances,
		ResolveMaterial(SlowRangeMaterial, RangeFallback));
	FDeliveryBotLidarRayBeamRendering::ApplyBeamMaterial(
		LidarStopRangeRingInstances,
		ResolveMaterial(StopRangeMaterial, RangeFallback));
	FDeliveryBotLidarRayBeamRendering::ApplyBeamMaterial(
		LidarObstacleWarningRangeRingInstances,
		ResolveMaterial(ObstacleWarningRangeRayMaterial, RangeFallback));
	FDeliveryBotLidarRayBeamRendering::ApplyBeamMaterial(
		LidarSlowRangeRayInstances,
		ResolveMaterial(SlowRangeRayMaterial, ResolveMaterial(SlowRangeMaterial, RangeFallback)));
	FDeliveryBotLidarRayBeamRendering::ApplyBeamMaterial(
		LidarStopRangeRayInstances,
		ResolveMaterial(StopRangeRayMaterial, ResolveMaterial(StopRangeMaterial, RangeFallback)));
	FDeliveryBotLidarRayBeamRendering::ApplyBeamMaterial(
		LidarObstacleWarningRangeRayInstances,
		ResolveMaterial(ObstacleWarningRangeRayMaterial, RangeFallback));
}

void ADeliveryBotLidarRayReviewActor::ApplyLidarRayFrame(
	const FEpisodeReplayRobotFrame& RobotFrame,
	const FDeliveryBotLidarSensorConfigInfo& LidarConfig,
	const FEpisodeLidarRayFrame* RayFrame,
	const FVector& ReplayWorldOffset)
{
	ClearLidarRays();
	if (!bLidarRaysVisible)
	{
		return;
	}

	const FDeliveryBotLidarSensorConfigInfo SafeConfig =
		SanitizeReplayLidarConfig(LidarConfig, RayFrame);
	const bool bIsOusterOS1Mode = FDeliveryBotLidarRayPattern::IsOusterOS1Mode(SafeConfig.LidarModeType);
	const float ScanRangeCm = FMath::Max(SafeConfig.ScanRangeM, 0.01f) * 100.0f;
	const float StopDistanceCm = FMath::Clamp(SafeConfig.StopDistanceM * 100.0f, 0.0f, ScanRangeCm);
	const float ObstacleWarningDistanceCm =
		FMath::Clamp(SafeConfig.ObstacleWarningDistanceM * 100.0f, 0.0f, ScanRangeCm);
	const float SlowDistanceCm = FMath::Clamp(SafeConfig.SlowDownDistanceM * 100.0f, 0.0f, ScanRangeCm);
	const float FrontHalfAngleDegree = SafeConfig.FrontHalfAngleDegree;
	const double ReplayTimeSeconds = RobotFrame.TimeSeconds;
	const FVector SensorLocationLocalCm(
		SafeConfig.SensorForwardOffsetM * 100.0f,
		SafeConfig.SensorRightOffsetM * 100.0f,
		SafeConfig.SensorHeightM * 100.0f);
	const FTransform RobotWorldTransform(
		RobotFrame.Rotation,
		RobotFrame.PositionCm + ReplayWorldOffset,
		FVector::OneVector);

	TArray<FDeliveryBotLidarRaySample> RaySamples;
	FDeliveryBotLidarRayPattern::BuildRaySamples(SafeConfig, RaySamples);

	UMaterialInterface* PreviewRayMaterial = LoadOptionalMaterial(PreviewLidarRayMaterialPath);
	UMaterialInterface* PrimaryRayMaterial = LoadOptionalMaterial(PreviewLidarPrimaryRayMaterialPath);
	UMaterialInterface* ThreeDRayMaterial = LoadOptionalMaterial(PreviewLidarThreeDRayMaterialPath);
	UMaterialInterface* OusterActiveRayMaterial = LoadOptionalMaterial(PreviewLidarOusterActiveRayMaterialPath);
	UMaterialInterface* OusterFullRayMaterial = LoadOptionalMaterial(PreviewLidarOusterFullRayMaterialPath);
	FDeliveryBotLidarRayBeamRendering::ApplyBeamMaterial(
		LidarThreeDRayInstances,
		bIsOusterOS1Mode
			? ResolveMaterial(
				OusterActiveRayMaterial,
				ResolveMaterial(PrimaryRayMaterial, ResolveMaterial(ThreeDRayMaterial, PreviewRayMaterial)))
			: ResolveMaterial(ThreeDRayMaterial, PreviewRayMaterial));
	FDeliveryBotLidarRayBeamRendering::ApplyBeamMaterial(
		LidarThreeDFullRayInstances,
		ResolveMaterial(OusterFullRayMaterial, PreviewRayMaterial));

	AddRangeRing(
		LidarRangeRingInstances,
		RobotWorldTransform,
		SensorLocationLocalCm,
		ScanRangeCm,
		static_cast<float>(RangeBeamThicknessScale));
	if (SlowDistanceCm > UE_SMALL_NUMBER)
	{
		AddRangeRing(
			LidarSlowRangeRingInstances,
			RobotWorldTransform,
			SensorLocationLocalCm,
			SlowDistanceCm,
			static_cast<float>(RangeBeamThicknessScale));
		AddRangeRaySet(
			LidarSlowRangeRayInstances,
			RobotWorldTransform,
			SensorLocationLocalCm,
			SlowDistanceCm,
			FrontHalfAngleDegree,
			static_cast<float>(RangeRayBeamThicknessScale));
	}
	if (StopDistanceCm > UE_SMALL_NUMBER)
	{
		AddRangeRing(
			LidarStopRangeRingInstances,
			RobotWorldTransform,
			SensorLocationLocalCm,
			StopDistanceCm,
			static_cast<float>(RangeBeamThicknessScale));
		AddRangeRaySet(
			LidarStopRangeRayInstances,
			RobotWorldTransform,
			SensorLocationLocalCm,
			StopDistanceCm,
			FrontHalfAngleDegree,
			static_cast<float>(RangeRayBeamThicknessScale));
	}
	if (ObstacleWarningDistanceCm > UE_SMALL_NUMBER)
	{
		AddRangeRing(
			LidarObstacleWarningRangeRingInstances,
			RobotWorldTransform,
			SensorLocationLocalCm,
			ObstacleWarningDistanceCm,
			static_cast<float>(RangeBeamThicknessScale));
		AddRangeRaySet(
			LidarObstacleWarningRangeRayInstances,
			RobotWorldTransform,
			SensorLocationLocalCm,
			ObstacleWarningDistanceCm,
			FrontHalfAngleDegree,
			static_cast<float>(RangeRayBeamThicknessScale));
	}

	AddRobotLocalRay(
		LidarFrontBoundaryInstances,
		RobotWorldTransform,
		SensorLocationLocalCm,
		FrontHalfAngleDegree,
		0.0f,
		ScanRangeCm,
		static_cast<float>(RayBeamThicknessScale));
	AddRobotLocalRay(
		LidarFrontBoundaryInstances,
		RobotWorldTransform,
		SensorLocationLocalCm,
		-FrontHalfAngleDegree,
		0.0f,
		ScanRangeCm,
		static_cast<float>(RayBeamThicknessScale));

	for (const FDeliveryBotLidarRaySample& RaySample : RaySamples)
	{
		if (RaySample.DimensionType != EDeliveryBotLidarRayDimensionType::OneD)
		{
			continue;
		}

		AddRobotLocalRay(
			LidarPrimaryRayInstances,
			RobotWorldTransform,
			SensorLocationLocalCm,
			RaySample.YawDegree,
			RaySample.PitchDegree,
			ScanRangeCm,
			static_cast<float>(RayBeamThicknessScale * 1.5));
	}

	if (FDeliveryBotLidarRayPattern::DoesModeIncludeDimension(
		SafeConfig.LidarModeType,
		EDeliveryBotLidarRayDimensionType::TwoD))
	{
		const int32 RequestedYawRayCount = FDeliveryBotLidarRayPattern::CountYawSamples(SafeConfig);
		const int32 YawRayStride = FMath::Max(
			1,
			FMath::CeilToInt(
				static_cast<float>(RequestedYawRayCount)
				/ static_cast<float>(FMath::Min(MaxVisibleScanRays, StandardVisible2DRayBeams))));
		for (const FDeliveryBotLidarRaySample& RaySample : RaySamples)
		{
			if (RaySample.DimensionType != EDeliveryBotLidarRayDimensionType::TwoD
				|| (RaySample.RayIndex % YawRayStride) != 0)
			{
				continue;
			}

			UInstancedStaticMeshComponent* TargetComponent =
				FDeliveryBotLidarRayPattern::IsFrontYaw(RaySample.YawDegree, FrontHalfAngleDegree)
					? LidarPrimaryRayInstances.Get()
					: LidarSecondaryRayInstances.Get();
			AddRobotLocalRay(
				TargetComponent,
				RobotWorldTransform,
				SensorLocationLocalCm,
				RaySample.YawDegree,
				RaySample.PitchDegree,
				ScanRangeCm,
				static_cast<float>(RayBeamThicknessScale));
		}
	}

	if (FDeliveryBotLidarRayPattern::DoesModeIncludeDimension(
		SafeConfig.LidarModeType,
		EDeliveryBotLidarRayDimensionType::ThreeD))
	{
		const int32 RequestedYawRayCount = FDeliveryBotLidarRayPattern::CountYawSamples(SafeConfig);
		const int32 RequestedPitchRayCount = FDeliveryBotLidarRayPattern::CountPitchSamples(SafeConfig);
		const int32 YawRayStride = FMath::Max(
			1,
			FMath::CeilToInt(
				static_cast<float>(RequestedYawRayCount)
				/ static_cast<float>(StandardVisible3DYawSamplesPerLayer)));
		const int32 PitchLayerStride = FMath::Max(
			1,
			FMath::CeilToInt(
				static_cast<float>(RequestedPitchRayCount)
				/ static_cast<float>(StandardVisible3DPitchLayers)));
		for (const FDeliveryBotLidarRaySample& RaySample : RaySamples)
		{
			if (RaySample.DimensionType != EDeliveryBotLidarRayDimensionType::ThreeD)
			{
				continue;
			}

			int32 PitchIndex = 0;
			int32 YawRayIndex = RaySample.RayIndex;
			if (RaySample.ChannelIndex != INDEX_NONE)
			{
				PitchIndex = RaySample.ChannelIndex;
			}
			else if (RequestedYawRayCount > 0)
			{
				PitchIndex = RaySample.RayIndex / RequestedYawRayCount;
			}

			if (RaySample.ColumnIndex != INDEX_NONE)
			{
				YawRayIndex = RaySample.ColumnIndex;
			}
			else if (RequestedYawRayCount > 0)
			{
				YawRayIndex = RaySample.RayIndex % RequestedYawRayCount;
			}
			if ((PitchIndex % PitchLayerStride) != 0 || (YawRayIndex % YawRayStride) != 0)
			{
				continue;
			}
			if (bIsOusterOS1Mode)
			{
				AddRobotLocalRay(
					LidarThreeDFullRayInstances,
					RobotWorldTransform,
					SensorLocationLocalCm,
					RaySample.YawDegree,
					RaySample.PitchDegree,
					ScanRangeCm,
					static_cast<float>(RayBeamThicknessScale * 0.32));
			}
			if (!ShouldRenderReplayOusterOS1Ray(SafeConfig, RaySample, ReplayTimeSeconds))
			{
				continue;
			}

			AddRobotLocalRay(
				LidarThreeDRayInstances,
				RobotWorldTransform,
				SensorLocationLocalCm,
				RaySample.YawDegree,
				RaySample.PitchDegree,
				ScanRangeCm,
				static_cast<float>(RayBeamThicknessScale * (bIsOusterOS1Mode ? 0.95 : 0.85)));
		}
	}

	ForEachLidarBeamComponent(
		[](UInstancedStaticMeshComponent* Component)
		{
			FDeliveryBotLidarRayBeamRendering::MarkBeamRenderStateDirty(Component);
		});
}

void ADeliveryBotLidarRayReviewActor::ClearLidarRays()
{
	RenderedRayCount = 0;
	ForEachLidarBeamComponent(
		[](UInstancedStaticMeshComponent* Component)
		{
			FDeliveryBotLidarRayBeamRendering::ClearBeamInstances(Component);
		});
}

void ADeliveryBotLidarRayReviewActor::SetLidarRaysVisible(const bool bVisible)
{
	bLidarRaysVisible = bVisible;
	SetActorHiddenInGame(!bVisible);
	ForEachLidarBeamComponent(
		[bVisible](UInstancedStaticMeshComponent* Component)
		{
			FDeliveryBotLidarRayBeamRendering::SetBeamComponentVisible(Component, bVisible);
		});

	if (!bVisible)
	{
		ClearLidarRays();
	}
}

void ADeliveryBotLidarRayReviewActor::ForEachLidarBeamComponent(
	TFunctionRef<void(UInstancedStaticMeshComponent*)> Operation) const
{
	Operation(LidarPrimaryRayInstances.Get());
	Operation(LidarSecondaryRayInstances.Get());
	Operation(LidarThreeDRayInstances.Get());
	Operation(LidarThreeDFullRayInstances.Get());
	Operation(LidarRangeRingInstances.Get());
	Operation(LidarSlowRangeRingInstances.Get());
	Operation(LidarStopRangeRingInstances.Get());
	Operation(LidarObstacleWarningRangeRingInstances.Get());
	Operation(LidarSlowRangeRayInstances.Get());
	Operation(LidarStopRangeRayInstances.Get());
	Operation(LidarObstacleWarningRangeRayInstances.Get());
	Operation(LidarFrontBoundaryInstances.Get());
}

bool ADeliveryBotLidarRayReviewActor::AddWorldBeam(
	UInstancedStaticMeshComponent* Component,
	const FVector& StartLocationCm,
	const FVector& EndLocationCm,
	const float ThicknessScale)
{
	if (FDeliveryBotLidarRayBeamRendering::AddBeamInstance(
		Component,
		StartLocationCm,
		EndLocationCm,
		RayBeamLengthCm,
		ThicknessScale,
		true))
	{
		++RenderedRayCount;
		return true;
	}

	return false;
}

bool ADeliveryBotLidarRayReviewActor::AddRobotLocalRay(
	UInstancedStaticMeshComponent* Component,
	const FTransform& RobotWorldTransform,
	const FVector& SensorLocationLocalCm,
	const float YawDegree,
	const float PitchDegree,
	const float RangeCm,
	const float ThicknessScale)
{
	const FVector LocalDirection = FRotator(PitchDegree, YawDegree, 0.0f).Vector();
	const FVector StartLocation = RobotWorldTransform.TransformPosition(SensorLocationLocalCm);
	const FVector EndLocation = RobotWorldTransform.TransformPosition(
		SensorLocationLocalCm + LocalDirection * RangeCm);
	return AddWorldBeam(Component, StartLocation, EndLocation, ThicknessScale);
}

void ADeliveryBotLidarRayReviewActor::AddRangeRing(
	UInstancedStaticMeshComponent* Component,
	const FTransform& RobotWorldTransform,
	const FVector& SensorLocationLocalCm,
	const float RadiusCm,
	const float ThicknessScale)
{
	if (!Component || RadiusCm <= UE_SMALL_NUMBER)
	{
		return;
	}

	const int32 SegmentCount = FMath::Max(12, LidarRangeRingSegments);
	for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
	{
		const float StartYawDegree = static_cast<float>(SegmentIndex) * 360.0f / static_cast<float>(SegmentCount);
		const float EndYawDegree = static_cast<float>(SegmentIndex + 1) * 360.0f / static_cast<float>(SegmentCount);
		const FVector StartDirection = FRotator(0.0f, StartYawDegree, 0.0f).Vector();
		const FVector EndDirection = FRotator(0.0f, EndYawDegree, 0.0f).Vector();
		AddWorldBeam(
			Component,
			RobotWorldTransform.TransformPosition(SensorLocationLocalCm + StartDirection * RadiusCm),
			RobotWorldTransform.TransformPosition(SensorLocationLocalCm + EndDirection * RadiusCm),
			ThicknessScale);
	}
}

void ADeliveryBotLidarRayReviewActor::AddRangeRaySet(
	UInstancedStaticMeshComponent* Component,
	const FTransform& RobotWorldTransform,
	const FVector& SensorLocationLocalCm,
	const float RangeCm,
	const float FrontHalfAngleDegree,
	const float ThicknessScale)
{
	if (!Component || RangeCm <= UE_SMALL_NUMBER)
	{
		return;
	}

	AddRobotLocalRay(
		Component,
		RobotWorldTransform,
		SensorLocationLocalCm,
		0.0f,
		0.0f,
		RangeCm,
		ThicknessScale);

	if (FrontHalfAngleDegree <= UE_SMALL_NUMBER)
	{
		return;
	}

	AddRobotLocalRay(
		Component,
		RobotWorldTransform,
		SensorLocationLocalCm,
		FrontHalfAngleDegree,
		0.0f,
		RangeCm,
		ThicknessScale);
	AddRobotLocalRay(
		Component,
		RobotWorldTransform,
		SensorLocationLocalCm,
		-FrontHalfAngleDegree,
		0.0f,
		RangeCm,
		ThicknessScale);
}
