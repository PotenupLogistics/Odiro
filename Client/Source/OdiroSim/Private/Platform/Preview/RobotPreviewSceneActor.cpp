#include "Platform/Preview/RobotPreviewSceneActor.h"

#include "Components/MeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "DeliveryBot/DeliveryBotLidarRayPattern.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Platform/RobotProfileSettings.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const TCHAR* PreviewCubeMeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* PreviewSphereMeshPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	const TCHAR* PreviewCylinderMeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	const TCHAR* PreviewMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	const TCHAR* PreviewRobotBodyMeshPath = TEXT("/Game/Models/DeliveryBot/SM_DeliveryBot.SM_DeliveryBot");
	const TCHAR* PreviewRobotSkeletalMeshPath = TEXT("/Game/Models/DeliveryBot/SKM_DeliveryBot.SKM_DeliveryBot");
	const TCHAR* PreviewLidarRayBeamMeshPath =
		TEXT("/Script/Engine.StaticMesh'/Game/Models/DeliveryBot/SM_LiDARLay.SM_LiDARLay'");
	const TCHAR* PreviewLidarPreviewRayMaterialPath =
		TEXT("/Script/Engine.Material'/Game/Materials/M_RobotPreview_LidarRay.M_RobotPreview_LidarRay'");
	const TCHAR* PreviewLidarPreviewRangeMaterialPath =
		TEXT("/Script/Engine.Material'/Game/Materials/M_RobotPreview_LidarRange.M_RobotPreview_LidarRange'");
	const TCHAR* PreviewLidarPrimaryRayMaterialPath =
		TEXT("/Script/Engine.MaterialInstanceConstant'/Game/Materials/MI_RobotPreview_LidarRay_Primary.MI_RobotPreview_LidarRay_Primary'");
	const TCHAR* PreviewLidarSecondaryRayMaterialPath =
		TEXT("/Script/Engine.MaterialInstanceConstant'/Game/Materials/MI_RobotPreview_LidarRay_Secondary.MI_RobotPreview_LidarRay_Secondary'");
	const TCHAR* PreviewLidarThreeDRayMaterialPath =
		TEXT("/Script/Engine.MaterialInstanceConstant'/Game/Materials/MI_RobotPreview_LidarRay_3D.MI_RobotPreview_LidarRay_3D'");
	const TCHAR* PreviewLidarFrontBoundaryMaterialPath =
		TEXT("/Script/Engine.MaterialInstanceConstant'/Game/Materials/MI_RobotPreview_LidarRay_Front.MI_RobotPreview_LidarRay_Front'");
	const TCHAR* PreviewLidarEndPointMaterialPath =
		TEXT("/Script/Engine.MaterialInstanceConstant'/Game/Materials/MI_RobotPreview_LidarRay_Point.MI_RobotPreview_LidarRay_Point'");
	const TCHAR* PreviewLidarScanRangeMaterialPath =
		TEXT("/Script/Engine.MaterialInstanceConstant'/Game/Materials/MI_RobotPreview_LidarRange_Max.MI_RobotPreview_LidarRange_Max'");
	const TCHAR* PreviewLidarSlowRangeMaterialPath =
		TEXT("/Script/Engine.MaterialInstanceConstant'/Game/Materials/MI_RobotPreview_LidarRange_Slow.MI_RobotPreview_LidarRange_Slow'");
	const TCHAR* PreviewLidarStopRangeMaterialPath =
		TEXT("/Script/Engine.MaterialInstanceConstant'/Game/Materials/MI_RobotPreview_LidarRange_Stop.MI_RobotPreview_LidarRange_Stop'");
	const TCHAR* PreviewLidarRayHitMaterialPath =
		TEXT("/Script/Engine.Material'/Game/Materials/M_LidarRayHit.M_LidarRayHit'");
	const TCHAR* PreviewLidarRayMissMaterialPath =
		TEXT("/Script/Engine.Material'/Game/Materials/M_LidarRayMiss.M_LidarRayMiss'");

	const float DeliveryBotVisualYawCorrectionDegrees = -90.0f;
	const float PreviewFallbackBeamMeshLengthCm = 100.0f;
	const float PreviewLidarBeamMeshLengthCm = 10.0f;
	const float PreviewLidarRayThicknessScale = 0.64f;
	const float PreviewLidarRangeThicknessScale = 0.4f;
	const float PreviewLidarPointDiameterCm = 5.0f;
	const int32 PreviewLidarRangeRingSegments = 64;
	const int32 PreviewLidarSparseVisible2DRayBeams = 72;
	const int32 PreviewLidarStandardVisible2DRayBeams = 180;
	const int32 PreviewLidarDenseVisible2DRayBeams = 360;
	const int32 PreviewLidarSparseVisible3DYawSamplesPerLayer = 18;
	const int32 PreviewLidarStandardVisible3DYawSamplesPerLayer = 36;
	const int32 PreviewLidarDenseVisible3DYawSamplesPerLayer = 60;
	const int32 PreviewLidarSparseVisible3DPitchLayers = 5;
	const int32 PreviewLidarStandardVisible3DPitchLayers = 9;
	const int32 PreviewLidarDenseVisible3DPitchLayers = 15;
	const FName PreviewMaterialColorParameterName(TEXT("PreviewColor"));
	const FName PreviewOpacityParameterName(TEXT("PreviewOpacity"));
	const FName PreviewColorParameterName(TEXT("Color"));
	const FName PreviewBaseColorParameterName(TEXT("BaseColor"));
	const FName PreviewTintParameterName(TEXT("Tint"));
	const FName PreviewTintColorParameterName(TEXT("TintColor"));
	const FName PreviewEmissiveColorParameterName(TEXT("EmissiveColor"));
	const FName RobotPreviewTag(TEXT("RobotPreviewOnly"));
	const FLinearColor PreviewStageFloorColor(0.10f, 0.14f, 0.18f, 1.0f);
	const FLinearColor PreviewFallbackBodyColor(0.08f, 0.55f, 0.58f, 1.0f);
	const FLinearColor PreviewWheelColor(0.015f, 0.018f, 0.025f, 1.0f);
	const FLinearColor PreviewLidarMarkerColor(1.20f, 0.78f, 0.14f, 1.0f);
	const FLinearColor PreviewLidarPrimaryRayColor(0.05f, 4.20f, 5.80f, 1.0f);
	const FLinearColor PreviewLidarSecondaryRayColor(0.08f, 1.45f, 1.85f, 0.72f);
	const FLinearColor PreviewLidarThreeDRayColor(2.75f, 0.55f, 6.00f, 0.92f);
	const FLinearColor PreviewLidarFrontBoundaryColor(0.20f, 0.70f, 8.00f, 1.0f);
	const FLinearColor PreviewLidarScanRangeColor(0.10f, 4.70f, 1.25f, 0.90f);
	const FLinearColor PreviewLidarSlowRangeColor(5.40f, 2.80f, 0.10f, 1.0f);
	const FLinearColor PreviewLidarStopRangeColor(7.00f, 0.18f, 0.08f, 1.0f);
	const FLinearColor PreviewLidarEndPointColor(0.65f, 5.50f, 6.50f, 1.0f);

	// Loads optional artist-authored preview materials without warning while assets are being created.
	UMaterialInterface* LoadOptionalPreviewMaterial(const TCHAR* MaterialPath)
	{
		return Cast<UMaterialInterface>(
			StaticLoadObject(
				UMaterialInterface::StaticClass(),
				nullptr,
				MaterialPath,
				nullptr,
				LOAD_NoWarn));
	}

	void ApplyOptionalPreviewMaterial(
		UInstancedStaticMeshComponent* Component,
		UMaterialInterface* Material)
	{
		if (!Component || !Material)
		{
			return;
		}

		Material->CheckMaterialUsage(MATUSAGE_InstancedStaticMeshes);
		const int32 MaterialSlotCount = FMath::Max(1, Component->GetNumMaterials());
		for (int32 MaterialSlotIndex = 0; MaterialSlotIndex < MaterialSlotCount; ++MaterialSlotIndex)
		{
			Component->SetMaterial(MaterialSlotIndex, Material);
		}
	}

	EDeliveryBotLidarModeType ResolvePreviewLidarModeType(const FString& RawMode)
	{
		const FString NormalizedMode = RawMode.ToLower().Replace(TEXT("_"), TEXT("")).TrimStartAndEnd();
		if (NormalizedMode == TEXT("1d") || NormalizedMode == TEXT("oned"))
		{
			return EDeliveryBotLidarModeType::OneD;
		}
		if (NormalizedMode == TEXT("3d") || NormalizedMode == TEXT("threed"))
		{
			return EDeliveryBotLidarModeType::ThreeD;
		}
		if (NormalizedMode == TEXT("1dand2d") || NormalizedMode == TEXT("onedandtwod"))
		{
			return EDeliveryBotLidarModeType::OneDAndTwoD;
		}
		if (NormalizedMode == TEXT("2dand3d") || NormalizedMode == TEXT("twodandthreed"))
		{
			return EDeliveryBotLidarModeType::TwoDAndThreeD;
		}
		if (NormalizedMode == TEXT("all"))
		{
			return EDeliveryBotLidarModeType::All;
		}
		return EDeliveryBotLidarModeType::TwoD;
	}

	FDeliveryBotLidarSensorConfigInfo MakePreviewLidarConfig(const FRobotProfileLidarSettings& Settings)
	{
		FDeliveryBotLidarSensorConfigInfo Config;
		Config.bDrawDebug = Settings.bDrawDebug;
		Config.ScanRangeM = FMath::Max(Settings.ScanRangeM, 0.0f);
		Config.AngleStepDegree = FMath::Max(Settings.AngleStepDegree, 1.0f);
		Config.SensorHeightM = FMath::Max(Settings.SensorHeightM, 0.0f);
		Config.SensorForwardOffsetM = FMath::Clamp(Settings.SensorForwardOffsetM, -10.0f, 10.0f);
		Config.SensorRightOffsetM = FMath::Clamp(Settings.SensorRightOffsetM, -10.0f, 10.0f);
		Config.FrontHalfAngleDegree = FMath::Clamp(Settings.FrontHalfAngleDegree, 0.0f, 180.0f);
		Config.StopDistanceM = FMath::Max(Settings.StopDistanceM, 0.0f);
		Config.SlowDownDistanceM = FMath::Max(Settings.SlowDownDistanceM, 0.0f);
		Config.VerticalMinDegree = FMath::Clamp(Settings.VerticalMinDegree, -89.0f, 89.0f);
		Config.VerticalMaxDegree = FMath::Clamp(Settings.VerticalMaxDegree, -89.0f, 89.0f);
		Config.VerticalStepDegree = FMath::Max(Settings.VerticalStepDegree, 1.0f);
		Config.ScanRateHz = FMath::Max(Settings.ScanRateHz, 0.1f);
		Config.LidarModeType = ResolvePreviewLidarModeType(Settings.LidarMode);
		return Config;
	}

	int32 ResolvePreviewLidarMaxVisible2DRays(const ERobotPreviewLidarDisplayDensity Density)
	{
		switch (Density)
		{
		case ERobotPreviewLidarDisplayDensity::Sparse:
			return PreviewLidarSparseVisible2DRayBeams;
		case ERobotPreviewLidarDisplayDensity::Dense:
			return PreviewLidarDenseVisible2DRayBeams;
		case ERobotPreviewLidarDisplayDensity::Standard:
		default:
			return PreviewLidarStandardVisible2DRayBeams;
		}
	}

	int32 ResolvePreviewLidarMaxVisible3DYawSamples(const ERobotPreviewLidarDisplayDensity Density)
	{
		switch (Density)
		{
		case ERobotPreviewLidarDisplayDensity::Sparse:
			return PreviewLidarSparseVisible3DYawSamplesPerLayer;
		case ERobotPreviewLidarDisplayDensity::Dense:
			return PreviewLidarDenseVisible3DYawSamplesPerLayer;
		case ERobotPreviewLidarDisplayDensity::Standard:
		default:
			return PreviewLidarStandardVisible3DYawSamplesPerLayer;
		}
	}

	int32 ResolvePreviewLidarMaxVisible3DPitchLayers(const ERobotPreviewLidarDisplayDensity Density)
	{
		switch (Density)
		{
		case ERobotPreviewLidarDisplayDensity::Sparse:
			return PreviewLidarSparseVisible3DPitchLayers;
		case ERobotPreviewLidarDisplayDensity::Dense:
			return PreviewLidarDenseVisible3DPitchLayers;
		case ERobotPreviewLidarDisplayDensity::Standard:
		default:
			return PreviewLidarStandardVisible3DPitchLayers;
		}
	}
}

ARobotPreviewSceneActor::ARobotPreviewSceneActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Tags.Add(RobotPreviewTag);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	RobotRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RobotRoot"));
	RobotRoot->SetupAttachment(SceneRoot);

	StageFloor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StageFloor"));
	StageFloor->SetupAttachment(SceneRoot);
	StageFloor->SetRelativeLocation(FVector(0.0, 0.0, -3.0));
	StageFloor->SetRelativeScale3D(FVector(8.0, 5.0, 0.04));

	BodyVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyVisual"));
	BodyVisual->SetupAttachment(RobotRoot);

	SkeletalBodyVisual = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalBodyVisual"));
	SkeletalBodyVisual->SetupAttachment(RobotRoot);

	LidarMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LidarMarker"));
	LidarMarker->SetupAttachment(RobotRoot);
	LidarMarker->SetRelativeScale3D(FVector(0.10, 0.10, 0.10));

	LidarRayRoot = CreateDefaultSubobject<USceneComponent>(TEXT("LidarRayRoot"));
	LidarRayRoot->SetupAttachment(RobotRoot);

	LidarPrimaryRayInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LidarPrimaryRayInstances"));
	LidarSecondaryRayInstances =
		CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LidarSecondaryRayInstances"));
	LidarThreeDRayInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LidarThreeDRayInstances"));
	LidarRangeRingInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LidarRangeRingInstances"));
	LidarSlowRangeRingInstances =
		CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LidarSlowRangeRingInstances"));
	LidarStopRangeRingInstances =
		CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LidarStopRangeRingInstances"));
	LidarFrontBoundaryInstances =
		CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LidarFrontBoundaryInstances"));
	LidarEndPointInstances =
		CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LidarEndPointInstances"));

	for (int32 Index = 0; Index < 4; ++Index)
	{
		UStaticMeshComponent* WheelVisual = CreateDefaultSubobject<UStaticMeshComponent>(
			*FString::Printf(TEXT("WheelVisual_%d"), Index));
		WheelVisual->SetupAttachment(RobotRoot);
		WheelVisual->SetRelativeRotation(FRotator(90.0, 0.0, 0.0));
		WheelVisual->SetRelativeScale3D(FVector(0.16, 0.16, 0.08));
		WheelVisuals.Add(WheelVisual);
	}

	const FVector KeyLightLocation(-260.0, -300.0, 300.0);
	KeyLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(SceneRoot);
	KeyLight->SetRelativeLocation(KeyLightLocation);
	KeyLight->SetRelativeRotation((-KeyLightLocation).Rotation());
	KeyLight->SetIntensity(32000.0f);
	KeyLight->SetAttenuationRadius(900.0f);
	KeyLight->SetInnerConeAngle(28.0f);
	KeyLight->SetOuterConeAngle(55.0f);

	FillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(SceneRoot);
	FillLight->SetRelativeLocation(FVector(-180.0, -220.0, 220.0));
	FillLight->SetIntensity(1200.0f);
	FillLight->SetAttenuationRadius(650.0f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(PreviewCubeMeshPath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(PreviewSphereMeshPath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(PreviewCylinderMeshPath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PreviewMaterial(PreviewMaterialPath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RobotBodyMesh(PreviewRobotBodyMeshPath);
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> RobotSkeletalMesh(PreviewRobotSkeletalMeshPath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LidarRayBeamMesh(PreviewLidarRayBeamMeshPath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> LidarRayHitMaterial(PreviewLidarRayHitMaterialPath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> LidarRayMissMaterial(PreviewLidarRayMissMaterialPath);
	UMaterialInterface* LidarPreviewRayMaterial = LoadOptionalPreviewMaterial(PreviewLidarPreviewRayMaterialPath);
	UMaterialInterface* LidarPreviewRangeMaterial = LoadOptionalPreviewMaterial(PreviewLidarPreviewRangeMaterialPath);
	UMaterialInterface* LidarPrimaryRayMaterial = LoadOptionalPreviewMaterial(PreviewLidarPrimaryRayMaterialPath);
	UMaterialInterface* LidarSecondaryRayMaterial = LoadOptionalPreviewMaterial(PreviewLidarSecondaryRayMaterialPath);
	UMaterialInterface* LidarThreeDRayMaterial = LoadOptionalPreviewMaterial(PreviewLidarThreeDRayMaterialPath);
	UMaterialInterface* LidarFrontBoundaryMaterial = LoadOptionalPreviewMaterial(PreviewLidarFrontBoundaryMaterialPath);
	UMaterialInterface* LidarEndPointMaterial = LoadOptionalPreviewMaterial(PreviewLidarEndPointMaterialPath);
	UMaterialInterface* LidarScanRangeMaterial = LoadOptionalPreviewMaterial(PreviewLidarScanRangeMaterialPath);
	UMaterialInterface* LidarSlowRangeMaterial = LoadOptionalPreviewMaterial(PreviewLidarSlowRangeMaterialPath);
	UMaterialInterface* LidarStopRangeMaterial = LoadOptionalPreviewMaterial(PreviewLidarStopRangeMaterialPath);

	bUsingSkeletalBodyMesh = RobotSkeletalMesh.Succeeded();
	bUsingFallbackBodyMesh = !bUsingSkeletalBodyMesh && !RobotBodyMesh.Succeeded();
	ConfigurePreviewSkeletalMeshComponent(SkeletalBodyVisual);
	if (bUsingSkeletalBodyMesh)
	{
		SkeletalBodyVisual->SetSkeletalMesh(RobotSkeletalMesh.Object);
	}
	SkeletalBodyVisual->SetVisibility(bUsingSkeletalBodyMesh, true);
	SkeletalBodyVisual->SetHiddenInGame(!bUsingSkeletalBodyMesh);

	ConfigurePreviewMeshComponent(StageFloor, CubeMesh.Object);
	ConfigurePreviewMeshComponent(
		BodyVisual,
		bUsingFallbackBodyMesh ? CubeMesh.Object : RobotBodyMesh.Object);
	BodyVisual->SetVisibility(!bUsingSkeletalBodyMesh, true);
	BodyVisual->SetHiddenInGame(bUsingSkeletalBodyMesh);
	ConfigurePreviewMeshComponent(LidarMarker, SphereMesh.Object);

	UStaticMesh* LidarBeamMesh = LidarRayBeamMesh.Succeeded() ? LidarRayBeamMesh.Object : CubeMesh.Object;
	LidarBeamMeshLengthCm =
		LidarRayBeamMesh.Succeeded() ? PreviewLidarBeamMeshLengthCm : PreviewFallbackBeamMeshLengthCm;
	ConfigurePreviewInstancedMeshComponent(LidarPrimaryRayInstances, LidarBeamMesh);
	ConfigurePreviewInstancedMeshComponent(LidarSecondaryRayInstances, LidarBeamMesh);
	ConfigurePreviewInstancedMeshComponent(LidarThreeDRayInstances, LidarBeamMesh);
	ConfigurePreviewInstancedMeshComponent(LidarRangeRingInstances, LidarBeamMesh);
	ConfigurePreviewInstancedMeshComponent(LidarSlowRangeRingInstances, LidarBeamMesh);
	ConfigurePreviewInstancedMeshComponent(LidarStopRangeRingInstances, LidarBeamMesh);
	ConfigurePreviewInstancedMeshComponent(LidarFrontBoundaryInstances, LidarBeamMesh);
	ConfigurePreviewInstancedMeshComponent(LidarEndPointInstances, SphereMesh.Object);

	for (UStaticMeshComponent* WheelVisual : WheelVisuals)
	{
		ConfigurePreviewMeshComponent(WheelVisual, CylinderMesh.Object);
		WheelVisual->SetVisibility(bUsingFallbackBodyMesh, true);
		WheelVisual->SetHiddenInGame(!bUsingFallbackBodyMesh);
	}

	if (PreviewMaterial.Succeeded())
	{
		StageFloor->SetMaterial(0, PreviewMaterial.Object);
		if (bUsingFallbackBodyMesh)
		{
			BodyVisual->SetMaterial(0, PreviewMaterial.Object);
		}
		LidarMarker->SetMaterial(0, PreviewMaterial.Object);
		for (UStaticMeshComponent* WheelVisual : WheelVisuals)
		{
			WheelVisual->SetMaterial(0, PreviewMaterial.Object);
		}
	}

	UMaterialInterface* PreviewBaseMaterial =
		PreviewMaterial.Succeeded() ? PreviewMaterial.Object.Get() : nullptr;
	UMaterialInterface* FallbackPrimaryRayMaterial =
		LidarPreviewRayMaterial
			? LidarPreviewRayMaterial
			: (LidarRayHitMaterial.Succeeded() ? LidarRayHitMaterial.Object.Get() : PreviewBaseMaterial);
	UMaterialInterface* FallbackSecondaryRayMaterial =
		LidarPreviewRayMaterial
			? LidarPreviewRayMaterial
			: (LidarRayMissMaterial.Succeeded() ? LidarRayMissMaterial.Object.Get() : FallbackPrimaryRayMaterial);
	UMaterialInterface* FallbackRangeMaterial =
		LidarPreviewRangeMaterial ? LidarPreviewRangeMaterial : FallbackPrimaryRayMaterial;
	UMaterialInterface* PrimaryRayMaterial = LidarPrimaryRayMaterial ? LidarPrimaryRayMaterial : FallbackPrimaryRayMaterial;
	UMaterialInterface* SecondaryRayMaterial =
		LidarSecondaryRayMaterial ? LidarSecondaryRayMaterial : FallbackSecondaryRayMaterial;
	UMaterialInterface* ThreeDRayMaterial = LidarThreeDRayMaterial ? LidarThreeDRayMaterial : FallbackPrimaryRayMaterial;
	UMaterialInterface* FrontBoundaryMaterial =
		LidarFrontBoundaryMaterial ? LidarFrontBoundaryMaterial : FallbackPrimaryRayMaterial;
	UMaterialInterface* EndPointMaterial = LidarEndPointMaterial ? LidarEndPointMaterial : FallbackPrimaryRayMaterial;
	UMaterialInterface* ScanRangeMaterial = LidarScanRangeMaterial ? LidarScanRangeMaterial : FallbackRangeMaterial;
	UMaterialInterface* SlowRangeMaterial = LidarSlowRangeMaterial ? LidarSlowRangeMaterial : FallbackRangeMaterial;
	UMaterialInterface* StopRangeMaterial = LidarStopRangeMaterial ? LidarStopRangeMaterial : FallbackRangeMaterial;
	if (PrimaryRayMaterial)
	{
		PrimaryRayMaterial->CheckMaterialUsage(MATUSAGE_InstancedStaticMeshes);
		LidarPrimaryRayInstances->SetMaterial(0, PrimaryRayMaterial);
	}
	if (SecondaryRayMaterial)
	{
		SecondaryRayMaterial->CheckMaterialUsage(MATUSAGE_InstancedStaticMeshes);
		LidarSecondaryRayInstances->SetMaterial(0, SecondaryRayMaterial);
	}
	if (ThreeDRayMaterial)
	{
		ThreeDRayMaterial->CheckMaterialUsage(MATUSAGE_InstancedStaticMeshes);
		LidarThreeDRayInstances->SetMaterial(0, ThreeDRayMaterial);
	}
	if (FrontBoundaryMaterial)
	{
		FrontBoundaryMaterial->CheckMaterialUsage(MATUSAGE_InstancedStaticMeshes);
		LidarFrontBoundaryInstances->SetMaterial(0, FrontBoundaryMaterial);
	}
	if (EndPointMaterial)
	{
		EndPointMaterial->CheckMaterialUsage(MATUSAGE_InstancedStaticMeshes);
		LidarEndPointInstances->SetMaterial(0, EndPointMaterial);
	}
	if (ScanRangeMaterial)
	{
		ScanRangeMaterial->CheckMaterialUsage(MATUSAGE_InstancedStaticMeshes);
		LidarRangeRingInstances->SetMaterial(0, ScanRangeMaterial);
	}
	if (SlowRangeMaterial)
	{
		SlowRangeMaterial->CheckMaterialUsage(MATUSAGE_InstancedStaticMeshes);
		LidarSlowRangeRingInstances->SetMaterial(0, SlowRangeMaterial);
	}
	if (StopRangeMaterial)
	{
		StopRangeMaterial->CheckMaterialUsage(MATUSAGE_InstancedStaticMeshes);
		LidarStopRangeRingInstances->SetMaterial(0, StopRangeMaterial);
	}

	ApplyPreviewColor(StageFloor, PreviewStageFloorColor);
	if (bUsingFallbackBodyMesh)
	{
		ApplyPreviewColor(BodyVisual, PreviewFallbackBodyColor);
	}
	ApplyPreviewColor(LidarMarker, PreviewLidarMarkerColor);
	if (!LidarPrimaryRayMaterial)
	{
		ApplyPreviewColor(LidarPrimaryRayInstances, PreviewLidarPrimaryRayColor);
	}
	if (!LidarSecondaryRayMaterial)
	{
		ApplyPreviewColor(LidarSecondaryRayInstances, PreviewLidarSecondaryRayColor);
	}
	if (!LidarThreeDRayMaterial)
	{
		ApplyPreviewColor(LidarThreeDRayInstances, PreviewLidarThreeDRayColor);
	}
	if (!LidarScanRangeMaterial)
	{
		ApplyPreviewColor(LidarRangeRingInstances, PreviewLidarScanRangeColor);
	}
	if (!LidarSlowRangeMaterial)
	{
		ApplyPreviewColor(LidarSlowRangeRingInstances, PreviewLidarSlowRangeColor);
	}
	if (!LidarStopRangeMaterial)
	{
		ApplyPreviewColor(LidarStopRangeRingInstances, PreviewLidarStopRangeColor);
	}
	if (!LidarFrontBoundaryMaterial)
	{
		ApplyPreviewColor(LidarFrontBoundaryInstances, PreviewLidarFrontBoundaryColor);
	}
	if (!LidarEndPointMaterial)
	{
		ApplyPreviewColor(LidarEndPointInstances, PreviewLidarEndPointColor);
	}
	for (UStaticMeshComponent* WheelVisual : WheelVisuals)
	{
		ApplyPreviewColor(WheelVisual, PreviewWheelColor);
	}
}

void ARobotPreviewSceneActor::ApplySettings(const FRobotProfileSettings& Settings)
{
	CurrentSettings = Settings;

	const float LengthM = FMath::Max(CurrentSettings.Body.LengthM, 0.05f);
	const float WidthM = FMath::Max(CurrentSettings.Body.WidthM, 0.05f);
	const float HeightM = FMath::Max(CurrentSettings.Body.HeightM, 0.05f);
	const float SensorHeightM = FMath::Max(CurrentSettings.Lidar.SensorHeightM, 0.0f);
	const float SensorForwardOffsetM = CurrentSettings.Lidar.SensorForwardOffsetM;
	const float SensorRightOffsetM = CurrentSettings.Lidar.SensorRightOffsetM;
	const float MaxBodyCm = FMath::Max3(LengthM * 100.0f, WidthM * 100.0f, HeightM * 100.0f);
	const float LidarMarkerDiameterCm = FMath::Clamp(MaxBodyCm * 0.16f, 5.0f, 18.0f);

	RefreshBodyTransform(CurrentSettings);

	LidarMarker->SetRelativeLocation(FVector(
		SensorForwardOffsetM * 100.0f,
		SensorRightOffsetM * 100.0f,
		SensorHeightM * 100.0f));
	LidarMarker->SetRelativeScale3D(FVector(LidarMarkerDiameterCm / 100.0f));

	RefreshWheelTransforms(CurrentSettings);
	SetRobotYawDegrees(CurrentYawDegrees);
	if (bLidarPreviewRaysVisible)
	{
		RefreshLidarPreviewRays();
	}
}

void ARobotPreviewSceneActor::SetLidarDisplayOptions(const FRobotPreviewLidarDisplayOptions& Options)
{
	LidarDisplayOptions = Options;
	if (bLidarPreviewRaysVisible)
	{
		RefreshLidarPreviewRays();
	}
}

void ARobotPreviewSceneActor::SetRobotYawDegrees(const float YawDegrees)
{
	CurrentYawDegrees = YawDegrees;
	RobotRoot->SetRelativeRotation(FRotator(0.0, CurrentYawDegrees, 0.0));
}

FVector ARobotPreviewSceneActor::GetPreviewFocusLocation() const
{
	const float HeightM = FMath::Max(CurrentSettings.Body.HeightM, 0.05f);
	return GetActorLocation() + FVector(0.0, 0.0, HeightM * 55.0f);
}

float ARobotPreviewSceneActor::GetPreviewRadiusCm() const
{
	const float LengthCm = FMath::Max(CurrentSettings.Body.LengthM * 100.0f, 25.0f);
	const float WidthCm = FMath::Max(CurrentSettings.Body.WidthM * 100.0f, 25.0f);
	const float HeightCm = FMath::Max(CurrentSettings.Body.HeightM * 100.0f, 25.0f);
	return FMath::Max3(LengthCm, WidthCm, HeightCm);
}

void ARobotPreviewSceneActor::AddShowOnlyActors(TArray<AActor*>& OutActors)
{
	OutActors.Add(this);
}

void ARobotPreviewSceneActor::DrawLidarPreviewRays()
{
	bLidarPreviewRaysVisible = true;
	RefreshLidarPreviewRays();
}

void ARobotPreviewSceneActor::ClearLidarPreviewRays()
{
	bLidarPreviewRaysVisible = false;
	ClearLidarPreviewGeometry();
}

void ARobotPreviewSceneActor::ConfigurePreviewMeshComponent(
	UStaticMeshComponent* Component,
	UStaticMesh* Mesh)
{
	if (!Component)
	{
		return;
	}

	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetCastShadow(true);
	Component->ComponentTags.Add(RobotPreviewTag);
	if (Mesh)
	{
		Component->SetStaticMesh(Mesh);
	}
}

void ARobotPreviewSceneActor::ConfigurePreviewInstancedMeshComponent(
	UInstancedStaticMeshComponent* Component,
	UStaticMesh* Mesh)
{
	if (!Component)
	{
		return;
	}

	Component->SetupAttachment(LidarRayRoot);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetGenerateOverlapEvents(false);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetCastShadow(false);
	Component->SetVisibility(false, true);
	Component->SetHiddenInGame(true);
	Component->SetCanEverAffectNavigation(false);
	Component->ComponentTags.Add(RobotPreviewTag);
	if (Mesh)
	{
		Component->SetStaticMesh(Mesh);
	}
}

void ARobotPreviewSceneActor::ConfigurePreviewSkeletalMeshComponent(USkeletalMeshComponent* Component)
{
	if (!Component)
	{
		return;
	}

	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetGenerateOverlapEvents(false);
	Component->SetSimulatePhysics(false);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetCastShadow(true);
	Component->ComponentTags.Add(RobotPreviewTag);
	Component->SetComponentTickEnabled(false);
	Component->PrimaryComponentTick.SetTickFunctionEnable(false);
}

void ARobotPreviewSceneActor::ApplyPreviewColor(
	UMeshComponent* Component,
	const FLinearColor& Color)
{
	if (!Component)
	{
		return;
	}

	const int32 MaterialSlotCount = FMath::Max(1, Component->GetNumMaterials());
	for (int32 MaterialSlotIndex = 0; MaterialSlotIndex < MaterialSlotCount; ++MaterialSlotIndex)
	{
		UMaterialInterface* BaseMaterial = Component->GetMaterial(MaterialSlotIndex);
		if (!BaseMaterial)
		{
			continue;
		}

		UMaterialInstanceDynamic* DynamicMaterial =
			Component->CreateDynamicMaterialInstance(MaterialSlotIndex, BaseMaterial);
		if (!DynamicMaterial)
		{
			continue;
		}

		DynamicMaterial->SetVectorParameterValue(PreviewMaterialColorParameterName, Color);
		DynamicMaterial->SetScalarParameterValue(PreviewOpacityParameterName, Color.A);
		DynamicMaterial->SetVectorParameterValue(PreviewColorParameterName, Color);
		DynamicMaterial->SetVectorParameterValue(PreviewBaseColorParameterName, Color);
		DynamicMaterial->SetVectorParameterValue(PreviewTintParameterName, Color);
		DynamicMaterial->SetVectorParameterValue(PreviewTintColorParameterName, Color);
		DynamicMaterial->SetVectorParameterValue(PreviewEmissiveColorParameterName, Color);
	}
}

void ARobotPreviewSceneActor::RefreshLidarPreviewRays()
{
	ClearLidarPreviewGeometry();

	if (!bLidarPreviewRaysVisible)
	{
		return;
	}

	ActualLidarPreviewRayCount = 0;

	UMaterialInterface* LidarPreviewRayMaterial = LoadOptionalPreviewMaterial(PreviewLidarPreviewRayMaterialPath);
	UMaterialInterface* LidarPreviewRangeMaterial = LoadOptionalPreviewMaterial(PreviewLidarPreviewRangeMaterialPath);
	UMaterialInterface* LidarPrimaryRayMaterial = LoadOptionalPreviewMaterial(PreviewLidarPrimaryRayMaterialPath);
	UMaterialInterface* LidarSecondaryRayMaterial = LoadOptionalPreviewMaterial(PreviewLidarSecondaryRayMaterialPath);
	UMaterialInterface* LidarThreeDRayMaterial = LoadOptionalPreviewMaterial(PreviewLidarThreeDRayMaterialPath);
	UMaterialInterface* LidarFrontBoundaryMaterial = LoadOptionalPreviewMaterial(PreviewLidarFrontBoundaryMaterialPath);
	UMaterialInterface* LidarEndPointMaterial = LoadOptionalPreviewMaterial(PreviewLidarEndPointMaterialPath);
	UMaterialInterface* LidarScanRangeMaterial = LoadOptionalPreviewMaterial(PreviewLidarScanRangeMaterialPath);
	UMaterialInterface* LidarSlowRangeMaterial = LoadOptionalPreviewMaterial(PreviewLidarSlowRangeMaterialPath);
	UMaterialInterface* LidarStopRangeMaterial = LoadOptionalPreviewMaterial(PreviewLidarStopRangeMaterialPath);

	TArray<UInstancedStaticMeshComponent*> LidarOverlayComponents = {
		LidarPrimaryRayInstances.Get(),
		LidarSecondaryRayInstances.Get(),
		LidarThreeDRayInstances.Get(),
		LidarRangeRingInstances.Get(),
		LidarSlowRangeRingInstances.Get(),
		LidarStopRangeRingInstances.Get(),
		LidarFrontBoundaryInstances.Get(),
		LidarEndPointInstances.Get()
	};
	for (UInstancedStaticMeshComponent* Component : LidarOverlayComponents)
	{
		if (Component)
		{
			Component->SetVisibility(true, true);
			Component->SetHiddenInGame(false);
		}
	}

	ApplyOptionalPreviewMaterial(
		LidarPrimaryRayInstances.Get(),
		LidarPrimaryRayMaterial ? LidarPrimaryRayMaterial : LidarPreviewRayMaterial);
	ApplyOptionalPreviewMaterial(
		LidarSecondaryRayInstances.Get(),
		LidarSecondaryRayMaterial ? LidarSecondaryRayMaterial : LidarPreviewRayMaterial);
	ApplyOptionalPreviewMaterial(
		LidarThreeDRayInstances.Get(),
		LidarThreeDRayMaterial ? LidarThreeDRayMaterial : LidarPreviewRayMaterial);
	ApplyOptionalPreviewMaterial(
		LidarFrontBoundaryInstances.Get(),
		LidarFrontBoundaryMaterial ? LidarFrontBoundaryMaterial : LidarPreviewRayMaterial);
	ApplyOptionalPreviewMaterial(
		LidarEndPointInstances.Get(),
		LidarEndPointMaterial ? LidarEndPointMaterial : LidarPreviewRayMaterial);
	ApplyOptionalPreviewMaterial(
		LidarRangeRingInstances.Get(),
		LidarScanRangeMaterial ? LidarScanRangeMaterial : LidarPreviewRangeMaterial);
	ApplyOptionalPreviewMaterial(
		LidarSlowRangeRingInstances.Get(),
		LidarSlowRangeMaterial ? LidarSlowRangeMaterial : LidarPreviewRangeMaterial);
	ApplyOptionalPreviewMaterial(
		LidarStopRangeRingInstances.Get(),
		LidarStopRangeMaterial ? LidarStopRangeMaterial : LidarPreviewRangeMaterial);

	if (!LidarPrimaryRayMaterial)
	{
		ApplyPreviewColor(LidarPrimaryRayInstances, PreviewLidarPrimaryRayColor);
	}
	if (!LidarSecondaryRayMaterial)
	{
		ApplyPreviewColor(LidarSecondaryRayInstances, PreviewLidarSecondaryRayColor);
	}
	if (!LidarThreeDRayMaterial)
	{
		ApplyPreviewColor(LidarThreeDRayInstances, PreviewLidarThreeDRayColor);
	}
	if (!LidarFrontBoundaryMaterial)
	{
		ApplyPreviewColor(LidarFrontBoundaryInstances, PreviewLidarFrontBoundaryColor);
	}
	if (!LidarEndPointMaterial)
	{
		ApplyPreviewColor(LidarEndPointInstances, PreviewLidarEndPointColor);
	}
	if (!LidarScanRangeMaterial)
	{
		ApplyPreviewColor(LidarRangeRingInstances, PreviewLidarScanRangeColor);
	}
	if (!LidarSlowRangeMaterial)
	{
		ApplyPreviewColor(LidarSlowRangeRingInstances, PreviewLidarSlowRangeColor);
	}
	if (!LidarStopRangeMaterial)
	{
		ApplyPreviewColor(LidarStopRangeRingInstances, PreviewLidarStopRangeColor);
	}

	const FDeliveryBotLidarSensorConfigInfo LidarConfig = MakePreviewLidarConfig(CurrentSettings.Lidar);
	const float SensorHeightCm = LidarConfig.SensorHeightM * 100.0f;
	const float ScanRangeCm = FMath::Max(LidarConfig.ScanRangeM, 0.01f) * 100.0f;
	const float StopDistanceCm =
		FMath::Clamp(LidarConfig.StopDistanceM * 100.0f, 0.0f, ScanRangeCm);
	const float SlowDistanceCm =
		FMath::Clamp(LidarConfig.SlowDownDistanceM * 100.0f, 0.0f, ScanRangeCm);
	const float FrontHalfAngleDegree = LidarConfig.FrontHalfAngleDegree;
	const FVector SensorLocationCm(
		LidarConfig.SensorForwardOffsetM * 100.0f,
		LidarConfig.SensorRightOffsetM * 100.0f,
		SensorHeightCm);

	TArray<FDeliveryBotLidarRaySample> RaySamples;
	FDeliveryBotLidarRayPattern::BuildRaySamples(LidarConfig, RaySamples);
	ActualLidarPreviewRayCount = RaySamples.Num();

	if (LidarDisplayOptions.bShowRange)
	{
		AddLidarPreviewRangeRing(
			LidarRangeRingInstances,
			SensorLocationCm,
			ScanRangeCm,
			PreviewLidarRangeThicknessScale);
		if (SlowDistanceCm > UE_SMALL_NUMBER)
		{
			AddLidarPreviewRangeRing(
				LidarSlowRangeRingInstances,
				SensorLocationCm,
				SlowDistanceCm,
				PreviewLidarRangeThicknessScale);
		}
		if (StopDistanceCm > UE_SMALL_NUMBER)
		{
			AddLidarPreviewRangeRing(
				LidarStopRangeRingInstances,
				SensorLocationCm,
				StopDistanceCm,
				PreviewLidarRangeThicknessScale);
		}

		AddLidarPreviewBeam(
			LidarFrontBoundaryInstances,
			SensorLocationCm,
			SensorLocationCm + FRotator(0.0f, FrontHalfAngleDegree, 0.0f).Vector() * ScanRangeCm,
			PreviewLidarRayThicknessScale);
		AddLidarPreviewBeam(
			LidarFrontBoundaryInstances,
			SensorLocationCm,
			SensorLocationCm + FRotator(0.0f, -FrontHalfAngleDegree, 0.0f).Vector() * ScanRangeCm,
			PreviewLidarRayThicknessScale);
	}

	for (const FDeliveryBotLidarRaySample& RaySample : RaySamples)
	{
		if (RaySample.DimensionType != EDeliveryBotLidarRayDimensionType::OneD)
		{
			continue;
		}

		AddLidarPreviewRay(
			LidarPrimaryRayInstances,
			SensorLocationCm,
			RaySample.YawDegree,
			RaySample.PitchDegree,
			ScanRangeCm,
			PreviewLidarRayThicknessScale * 1.5f);
	}

	if (FDeliveryBotLidarRayPattern::DoesModeIncludeDimension(
		LidarConfig.LidarModeType,
		EDeliveryBotLidarRayDimensionType::TwoD))
	{
		const int32 RequestedYawRayCount = FDeliveryBotLidarRayPattern::CountYawSamples(LidarConfig);
		const int32 YawRayStride =
			FMath::Max(
				1,
				FMath::CeilToInt(
					static_cast<float>(RequestedYawRayCount)
					/ ResolvePreviewLidarMaxVisible2DRays(LidarDisplayOptions.Density)));
		for (const FDeliveryBotLidarRaySample& RaySample : RaySamples)
		{
			if (RaySample.DimensionType != EDeliveryBotLidarRayDimensionType::TwoD)
			{
				continue;
			}

			if ((RaySample.RayIndex % YawRayStride) == 0)
			{
				UInstancedStaticMeshComponent* TargetComponent =
					FDeliveryBotLidarRayPattern::IsFrontYaw(RaySample.YawDegree, FrontHalfAngleDegree)
						? LidarPrimaryRayInstances.Get()
						: LidarSecondaryRayInstances.Get();
				AddLidarPreviewRay(
					TargetComponent,
					SensorLocationCm,
					RaySample.YawDegree,
					RaySample.PitchDegree,
					ScanRangeCm,
					PreviewLidarRayThicknessScale);
			}
		}
	}

	if (FDeliveryBotLidarRayPattern::DoesModeIncludeDimension(
		LidarConfig.LidarModeType,
		EDeliveryBotLidarRayDimensionType::ThreeD))
	{
		const int32 RequestedYawRayCount = FDeliveryBotLidarRayPattern::CountYawSamples(LidarConfig);
		const int32 RequestedPitchRayCount = FDeliveryBotLidarRayPattern::CountPitchSamples(LidarConfig);
		const int32 YawRayStride =
			FMath::Max(
				1,
				FMath::CeilToInt(
					static_cast<float>(RequestedYawRayCount)
					/ ResolvePreviewLidarMaxVisible3DYawSamples(LidarDisplayOptions.Density)));
		const int32 PitchLayerStride =
			FMath::Max(
				1,
				FMath::CeilToInt(
					static_cast<float>(RequestedPitchRayCount)
					/ ResolvePreviewLidarMaxVisible3DPitchLayers(LidarDisplayOptions.Density)));
		for (const FDeliveryBotLidarRaySample& RaySample : RaySamples)
		{
			if (RaySample.DimensionType != EDeliveryBotLidarRayDimensionType::ThreeD)
			{
				continue;
			}

			const int32 PitchIndex = RaySample.RayIndex / RequestedYawRayCount;
			const int32 YawRayIndex = RaySample.RayIndex % RequestedYawRayCount;
			if ((PitchIndex % PitchLayerStride) != 0 || (YawRayIndex % YawRayStride) != 0)
			{
				continue;
			}

			AddLidarPreviewRay(
				LidarThreeDRayInstances,
				SensorLocationCm,
				RaySample.YawDegree,
				RaySample.PitchDegree,
				ScanRangeCm,
				PreviewLidarRayThicknessScale * 0.85f);
		}
	}

	for (UInstancedStaticMeshComponent* Component : LidarOverlayComponents)
	{
		if (Component)
		{
			Component->MarkRenderStateDirty();
		}
	}
}

void ARobotPreviewSceneActor::ClearLidarPreviewGeometry()
{
	RenderedLidarPreviewRayCount = 0;
	RenderedLidarPreviewPointCount = 0;
	ActualLidarPreviewRayCount = 0;
	TArray<UInstancedStaticMeshComponent*> LidarOverlayComponents = {
		LidarPrimaryRayInstances.Get(),
		LidarSecondaryRayInstances.Get(),
		LidarThreeDRayInstances.Get(),
		LidarRangeRingInstances.Get(),
		LidarSlowRangeRingInstances.Get(),
		LidarStopRangeRingInstances.Get(),
		LidarFrontBoundaryInstances.Get(),
		LidarEndPointInstances.Get()
	};
	for (UInstancedStaticMeshComponent* Component : LidarOverlayComponents)
	{
		if (Component)
		{
			Component->ClearInstances();
			Component->SetVisibility(bLidarPreviewRaysVisible, true);
			Component->SetHiddenInGame(!bLidarPreviewRaysVisible);
		}
	}
}

bool ARobotPreviewSceneActor::AddLidarPreviewBeam(
	UInstancedStaticMeshComponent* Component,
	const FVector& StartLocationCm,
	const FVector& EndLocationCm,
	const float ThicknessScale)
{
	if (!Component)
	{
		return false;
	}

	const FVector RayDelta = EndLocationCm - StartLocationCm;
	const double RayLengthCm = RayDelta.Size();
	constexpr double MinRayBeamDimension = 0.001;
	if (RayLengthCm <= MinRayBeamDimension)
	{
		return false;
	}

	const FVector RayDirection = RayDelta / RayLengthCm;
	const FVector Midpoint = StartLocationCm + RayDelta * 0.5;
	const FRotator Rotation = FRotationMatrix::MakeFromX(RayDirection).Rotator();
	const double SafeBeamLengthCm = FMath::Max(MinRayBeamDimension, static_cast<double>(LidarBeamMeshLengthCm));
	const double SafeThicknessScale = FMath::Max(MinRayBeamDimension, static_cast<double>(ThicknessScale));
	const FVector Scale(
		RayLengthCm / SafeBeamLengthCm,
		SafeThicknessScale,
		SafeThicknessScale);
	Component->AddInstance(FTransform(Rotation, Midpoint, Scale), false);
	return true;
}

void ARobotPreviewSceneActor::AddLidarPreviewRangeRing(
	UInstancedStaticMeshComponent* Component,
	const FVector& CenterLocationCm,
	const float RadiusCm,
	const float ThicknessScale)
{
	if (!Component || RadiusCm <= UE_SMALL_NUMBER)
	{
		return;
	}

	const int32 SegmentCount = FMath::Max(12, PreviewLidarRangeRingSegments);
	for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
	{
		const float StartYawDegree = static_cast<float>(SegmentIndex) * 360.0f / static_cast<float>(SegmentCount);
		const float EndYawDegree = static_cast<float>(SegmentIndex + 1) * 360.0f / static_cast<float>(SegmentCount);
		const FVector StartDirection = FRotator(0.0f, StartYawDegree, 0.0f).Vector();
		const FVector EndDirection = FRotator(0.0f, EndYawDegree, 0.0f).Vector();
		AddLidarPreviewBeam(
			Component,
			CenterLocationCm + StartDirection * RadiusCm,
			CenterLocationCm + EndDirection * RadiusCm,
			ThicknessScale);
	}
}

void ARobotPreviewSceneActor::AddLidarPreviewRay(
	UInstancedStaticMeshComponent* Component,
	const FVector& SensorLocationCm,
	const float YawDegree,
	const float PitchDegree,
	const float RangeCm,
	const float ThicknessScale)
{
	const FVector Direction = FRotator(PitchDegree, YawDegree, 0.0f).Vector();
	const FVector EndLocationCm = SensorLocationCm + Direction * RangeCm;
	if (LidarDisplayOptions.bShowRays
		&& AddLidarPreviewBeam(
			Component,
			SensorLocationCm,
			EndLocationCm,
			ThicknessScale))
	{
		++RenderedLidarPreviewRayCount;
	}

	if (LidarDisplayOptions.bShowPoints)
	{
		AddLidarPreviewPoint(LidarEndPointInstances, EndLocationCm, PreviewLidarPointDiameterCm);
	}
}

void ARobotPreviewSceneActor::AddLidarPreviewPoint(
	UInstancedStaticMeshComponent* Component,
	const FVector& LocationCm,
	const float DiameterCm)
{
	if (!Component || DiameterCm <= UE_SMALL_NUMBER)
	{
		return;
	}

	const float SafeScale = FMath::Max(DiameterCm / 100.0f, 0.001f);
	Component->AddInstance(FTransform(FRotator::ZeroRotator, LocationCm, FVector(SafeScale)), false);
	++RenderedLidarPreviewPointCount;
}

void ARobotPreviewSceneActor::RefreshWheelTransforms(const FRobotProfileSettings& Settings)
{
	if (WheelVisuals.Num() < 4)
	{
		return;
	}

	const float BodyLengthCm = FMath::Max(Settings.Body.LengthM * 100.0f, 40.0f);
	const float BodyWidthCm = FMath::Max(Settings.Body.WidthM * 100.0f, 35.0f);
	const float WheelBaseCm = FMath::Clamp(
		Settings.Body.WheelBaseM * 100.0f,
		20.0f,
		BodyLengthCm * 0.9f);
	const float WheelX = WheelBaseCm * 0.5f;
	const float WheelY = BodyWidthCm * 0.5f + 8.0f;
	const float WheelZ = 16.0f;

	WheelVisuals[0]->SetRelativeLocation(FVector(WheelX, -WheelY, WheelZ));
	WheelVisuals[1]->SetRelativeLocation(FVector(WheelX, WheelY, WheelZ));
	WheelVisuals[2]->SetRelativeLocation(FVector(-WheelX, -WheelY, WheelZ));
	WheelVisuals[3]->SetRelativeLocation(FVector(-WheelX, WheelY, WheelZ));
}

void ARobotPreviewSceneActor::RefreshBodyTransform(const FRobotProfileSettings& Settings)
{
	const float LengthM = FMath::Max(Settings.Body.LengthM, 0.05f);
	const float WidthM = FMath::Max(Settings.Body.WidthM, 0.05f);
	const float HeightM = FMath::Max(Settings.Body.HeightM, 0.05f);

	if (bUsingSkeletalBodyMesh)
	{
		ApplyMeshComponentBoundsTransform(SkeletalBodyVisual, Settings);
		return;
	}

	if (bUsingFallbackBodyMesh)
	{
		BodyVisual->SetRelativeScale3D(FVector(LengthM, WidthM, HeightM));
		BodyVisual->SetRelativeLocation(FVector(0.0, 0.0, HeightM * 50.0f));
		return;
	}

	ApplyMeshComponentBoundsTransform(BodyVisual, Settings);
}

void ARobotPreviewSceneActor::ApplyMeshComponentBoundsTransform(
	UMeshComponent* Component,
	const FRobotProfileSettings& Settings)
{
	if (!IsValid(Component))
	{
		return;
	}

	Component->SetRelativeLocation(FVector::ZeroVector);
	Component->SetRelativeRotation(FRotator(0.0f, DeliveryBotVisualYawCorrectionDegrees, 0.0f));
	Component->SetRelativeScale3D(FVector::OneVector);

	FBoxSphereBounds MeshBounds = Component->CalcBounds(FTransform::Identity);
	const FVector MeshSizeCm = MeshBounds.BoxExtent * 2.0f;
	if (MeshSizeCm.IsNearlyZero())
	{
		return;
	}

	const float LengthM = FMath::Max(Settings.Body.LengthM, 0.05f);
	const float WidthM = FMath::Max(Settings.Body.WidthM, 0.05f);
	const float HeightM = FMath::Max(Settings.Body.HeightM, 0.05f);
	const FVector TargetSizeCm(WidthM * 100.0f, LengthM * 100.0f, HeightM * 100.0f);
	const FVector BodyScale(
		MeshSizeCm.X > UE_SMALL_NUMBER ? TargetSizeCm.X / MeshSizeCm.X : 1.0f,
		MeshSizeCm.Y > UE_SMALL_NUMBER ? TargetSizeCm.Y / MeshSizeCm.Y : 1.0f,
		MeshSizeCm.Z > UE_SMALL_NUMBER ? TargetSizeCm.Z / MeshSizeCm.Z : 1.0f);
	const float MeshBottomZ = (MeshBounds.Origin.Z - MeshBounds.BoxExtent.Z) * BodyScale.Z;
	const FVector MeshCenterOffset(
		MeshBounds.Origin.X * BodyScale.X,
		MeshBounds.Origin.Y * BodyScale.Y,
		0.0f);
	const FVector RotatedCenterOffset =
		FRotator(0.0f, DeliveryBotVisualYawCorrectionDegrees, 0.0f).RotateVector(MeshCenterOffset);

	Component->SetRelativeScale3D(BodyScale);
	Component->SetRelativeLocation(FVector(-RotatedCenterOffset.X, -RotatedCenterOffset.Y, -MeshBottomZ));
}
