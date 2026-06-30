#include "Platform/Preview/RobotPreviewSceneActor.h"

#include "Components/MeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
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
	const float PreviewLidarVerticalMinDegree = -10.0f;
	const float PreviewLidarVerticalMaxDegree = 10.0f;
	const FName PreviewMaterialColorParameterName(TEXT("PreviewColor"));
	const FName PreviewOpacityParameterName(TEXT("PreviewOpacity"));
	const FName PreviewColorParameterName(TEXT("Color"));
	const FName PreviewBaseColorParameterName(TEXT("BaseColor"));
	const FName PreviewTintParameterName(TEXT("Tint"));
	const FName PreviewTintColorParameterName(TEXT("TintColor"));
	const FName PreviewEmissiveColorParameterName(TEXT("EmissiveColor"));
	const FName RobotPreviewTag(TEXT("RobotPreviewOnly"));

	enum class EPreviewLidarMode : uint8
	{
		OneD,
		TwoD,
		ThreeD,
		OneDAndTwoD,
		TwoDAndThreeD,
		All
	};

	EPreviewLidarMode ResolvePreviewLidarMode(const FString& RawMode)
	{
		const FString NormalizedMode = RawMode.ToLower().Replace(TEXT("_"), TEXT("")).TrimStartAndEnd();
		if (NormalizedMode == TEXT("1d") || NormalizedMode == TEXT("oned"))
		{
			return EPreviewLidarMode::OneD;
		}
		if (NormalizedMode == TEXT("3d") || NormalizedMode == TEXT("threed"))
		{
			return EPreviewLidarMode::ThreeD;
		}
		if (NormalizedMode == TEXT("1dand2d") || NormalizedMode == TEXT("onedandtwod"))
		{
			return EPreviewLidarMode::OneDAndTwoD;
		}
		if (NormalizedMode == TEXT("2dand3d") || NormalizedMode == TEXT("twodandthreed"))
		{
			return EPreviewLidarMode::TwoDAndThreeD;
		}
		if (NormalizedMode == TEXT("all"))
		{
			return EPreviewLidarMode::All;
		}
		return EPreviewLidarMode::TwoD;
	}

	bool PreviewLidarModeIncludes1D(const EPreviewLidarMode Mode)
	{
		return Mode == EPreviewLidarMode::OneD
			|| Mode == EPreviewLidarMode::OneDAndTwoD
			|| Mode == EPreviewLidarMode::All;
	}

	bool PreviewLidarModeIncludes2D(const EPreviewLidarMode Mode)
	{
		return Mode == EPreviewLidarMode::TwoD
			|| Mode == EPreviewLidarMode::OneDAndTwoD
			|| Mode == EPreviewLidarMode::TwoDAndThreeD
			|| Mode == EPreviewLidarMode::All;
	}

	bool PreviewLidarModeIncludes3D(const EPreviewLidarMode Mode)
	{
		return Mode == EPreviewLidarMode::ThreeD
			|| Mode == EPreviewLidarMode::TwoDAndThreeD
			|| Mode == EPreviewLidarMode::All;
	}

	bool IsPreviewFrontYaw(const float YawDegree, const float FrontHalfAngleDegree)
	{
		const float SignedYawDegree = FMath::Abs(FMath::UnwindDegrees(YawDegree));
		return SignedYawDegree <= FrontHalfAngleDegree;
	}

	int32 CalculatePreviewYawRayCount(const float AngleStepDegree)
	{
		const float SafeAngleStepDegree = FMath::Max(AngleStepDegree, 1.0f);
		return FMath::Max(1, FMath::CeilToInt(360.0f / SafeAngleStepDegree));
	}

	int32 CalculatePreviewPitchRayCount(const float VerticalStepDegree)
	{
		const float SafeVerticalStepDegree = FMath::Max(VerticalStepDegree, 1.0f);
		return FMath::Max(
			1,
			FMath::FloorToInt(
				(PreviewLidarVerticalMaxDegree - PreviewLidarVerticalMinDegree) / SafeVerticalStepDegree)
			+ 1);
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
	UMaterialInterface* LidarPreviewRayMaterial = Cast<UMaterialInterface>(
		StaticLoadObject(
			UMaterialInterface::StaticClass(),
			nullptr,
			PreviewLidarPreviewRayMaterialPath,
			nullptr,
			LOAD_NoWarn));
	UMaterialInterface* LidarPreviewRangeMaterial = Cast<UMaterialInterface>(
		StaticLoadObject(
			UMaterialInterface::StaticClass(),
			nullptr,
			PreviewLidarPreviewRangeMaterialPath,
			nullptr,
			LOAD_NoWarn));

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
	UMaterialInterface* PrimaryRayMaterial =
		LidarPreviewRayMaterial
			? LidarPreviewRayMaterial
			: (LidarRayHitMaterial.Succeeded() ? LidarRayHitMaterial.Object.Get() : PreviewBaseMaterial);
	UMaterialInterface* SecondaryRayMaterial =
		LidarPreviewRayMaterial
			? LidarPreviewRayMaterial
			: (LidarRayMissMaterial.Succeeded() ? LidarRayMissMaterial.Object.Get() : PrimaryRayMaterial);
	UMaterialInterface* RingRayMaterial =
		LidarPreviewRangeMaterial ? LidarPreviewRangeMaterial : PrimaryRayMaterial;
	if (PrimaryRayMaterial)
	{
		PrimaryRayMaterial->CheckMaterialUsage(MATUSAGE_InstancedStaticMeshes);
		LidarPrimaryRayInstances->SetMaterial(0, PrimaryRayMaterial);
		LidarThreeDRayInstances->SetMaterial(0, PrimaryRayMaterial);
		LidarFrontBoundaryInstances->SetMaterial(0, PrimaryRayMaterial);
		LidarEndPointInstances->SetMaterial(0, PrimaryRayMaterial);
	}
	if (SecondaryRayMaterial)
	{
		SecondaryRayMaterial->CheckMaterialUsage(MATUSAGE_InstancedStaticMeshes);
		LidarSecondaryRayInstances->SetMaterial(0, SecondaryRayMaterial);
	}
	if (RingRayMaterial)
	{
		RingRayMaterial->CheckMaterialUsage(MATUSAGE_InstancedStaticMeshes);
		LidarRangeRingInstances->SetMaterial(0, RingRayMaterial);
		LidarSlowRangeRingInstances->SetMaterial(0, RingRayMaterial);
		LidarStopRangeRingInstances->SetMaterial(0, RingRayMaterial);
	}

	ApplyPreviewColor(StageFloor, FLinearColor(0.10f, 0.14f, 0.18f, 1.0f));
	if (bUsingFallbackBodyMesh)
	{
		ApplyPreviewColor(BodyVisual, FLinearColor(0.08f, 0.55f, 0.58f, 1.0f));
	}
	ApplyPreviewColor(LidarMarker, FLinearColor(1.20f, 0.78f, 0.14f, 1.0f));
	ApplyPreviewColor(LidarPrimaryRayInstances, FLinearColor(0.10f, 3.00f, 5.00f, 1.0f));
	ApplyPreviewColor(LidarSecondaryRayInstances, FLinearColor(0.20f, 1.30f, 1.80f, 0.95f));
	ApplyPreviewColor(LidarThreeDRayInstances, FLinearColor(2.20f, 1.15f, 5.50f, 1.0f));
	ApplyPreviewColor(LidarRangeRingInstances, FLinearColor(0.12f, 2.10f, 3.20f, 0.90f));
	ApplyPreviewColor(LidarSlowRangeRingInstances, FLinearColor(4.20f, 2.55f, 0.22f, 1.0f));
	ApplyPreviewColor(LidarStopRangeRingInstances, FLinearColor(5.50f, 0.42f, 0.18f, 1.0f));
	ApplyPreviewColor(LidarFrontBoundaryInstances, FLinearColor(0.35f, 3.40f, 5.50f, 1.0f));
	ApplyPreviewColor(LidarEndPointInstances, FLinearColor(0.35f, 4.20f, 6.00f, 1.0f));
	for (UStaticMeshComponent* WheelVisual : WheelVisuals)
	{
		ApplyPreviewColor(WheelVisual, FLinearColor(0.015f, 0.018f, 0.025f, 1.0f));
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
	if (Component)
	{
		UMaterialInterface* BaseMaterial = Component->GetMaterial(0);
		if (!BaseMaterial)
		{
			return;
		}

		UMaterialInstanceDynamic* DynamicMaterial =
			Component->CreateDynamicMaterialInstance(0, BaseMaterial);
		if (!DynamicMaterial)
		{
			return;
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

	const float SensorHeightCm = FMath::Max(CurrentSettings.Lidar.SensorHeightM, 0.0f) * 100.0f;
	const float ScanRangeCm = FMath::Max(CurrentSettings.Lidar.ScanRangeM, 0.01f) * 100.0f;
	const float StopDistanceCm =
		FMath::Clamp(FMath::Max(CurrentSettings.Lidar.StopDistanceM, 0.0f) * 100.0f, 0.0f, ScanRangeCm);
	const float SlowDistanceCm =
		FMath::Clamp(FMath::Max(CurrentSettings.Lidar.SlowDownDistanceM, 0.0f) * 100.0f, 0.0f, ScanRangeCm);
	const float AngleStepDegree = FMath::Max(CurrentSettings.Lidar.AngleStepDegree, 1.0f);
	const float VerticalStepDegree = FMath::Max(CurrentSettings.Lidar.VerticalStepDegree, 1.0f);
	const float FrontHalfAngleDegree = FMath::Clamp(CurrentSettings.Lidar.FrontHalfAngleDegree, 0.0f, 180.0f);
	const FVector SensorLocationCm(
		CurrentSettings.Lidar.SensorForwardOffsetM * 100.0f,
		CurrentSettings.Lidar.SensorRightOffsetM * 100.0f,
		SensorHeightCm);
	const EPreviewLidarMode PreviewMode = ResolvePreviewLidarMode(CurrentSettings.Lidar.LidarMode);

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

	if (PreviewLidarModeIncludes1D(PreviewMode))
	{
		++ActualLidarPreviewRayCount;
		AddLidarPreviewRay(
			LidarPrimaryRayInstances,
			SensorLocationCm,
			0.0f,
			0.0f,
			ScanRangeCm,
			PreviewLidarRayThicknessScale * 1.5f);
	}

	if (PreviewLidarModeIncludes2D(PreviewMode))
	{
		const int32 RequestedYawRayCount = CalculatePreviewYawRayCount(AngleStepDegree);
		ActualLidarPreviewRayCount += RequestedYawRayCount;
		const int32 YawRayStride =
			FMath::Max(
				1,
				FMath::CeilToInt(
					static_cast<float>(RequestedYawRayCount)
					/ ResolvePreviewLidarMaxVisible2DRays(LidarDisplayOptions.Density)));
		int32 YawRayIndex = 0;
		for (float YawDegree = 0.0f; YawDegree < 360.0f; YawDegree += AngleStepDegree)
		{
			if ((YawRayIndex % YawRayStride) == 0)
			{
				UInstancedStaticMeshComponent* TargetComponent =
					IsPreviewFrontYaw(YawDegree, FrontHalfAngleDegree)
						? LidarPrimaryRayInstances.Get()
						: LidarSecondaryRayInstances.Get();
				AddLidarPreviewRay(
					TargetComponent,
					SensorLocationCm,
					YawDegree,
					0.0f,
					ScanRangeCm,
					PreviewLidarRayThicknessScale);
			}
			++YawRayIndex;
		}
	}

	if (PreviewLidarModeIncludes3D(PreviewMode))
	{
		const int32 RequestedYawRayCount = CalculatePreviewYawRayCount(AngleStepDegree);
		const int32 RequestedPitchRayCount = CalculatePreviewPitchRayCount(VerticalStepDegree);
		const int32 RequestedRayCount = RequestedYawRayCount * RequestedPitchRayCount;
		ActualLidarPreviewRayCount += RequestedRayCount;

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
		for (int32 PitchIndex = 0; PitchIndex < RequestedPitchRayCount; ++PitchIndex)
		{
			if ((PitchIndex % PitchLayerStride) != 0)
			{
				continue;
			}

			const float PitchDegree = PreviewLidarVerticalMinDegree + static_cast<float>(PitchIndex) * VerticalStepDegree;
			int32 YawRayIndex = 0;
			for (float YawDegree = 0.0f; YawDegree < 360.0f; YawDegree += AngleStepDegree)
			{
				if ((YawRayIndex % YawRayStride) == 0)
				{
					AddLidarPreviewRay(
						LidarThreeDRayInstances,
						SensorLocationCm,
						YawDegree,
						PitchDegree,
						ScanRangeCm,
						PreviewLidarRayThicknessScale * 0.85f);
				}
				++YawRayIndex;
			}
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
