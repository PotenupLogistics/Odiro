#include "Scenario/Editor/ScenarioTransformGizmoActor.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	FName TransformGizmoHandleTag(EScenarioTransformGizmoHandle handle)
	{
		switch (handle)
		{
		case EScenarioTransformGizmoHandle::TranslateX:
			return FName(TEXT("TranslateX"));
		case EScenarioTransformGizmoHandle::TranslateY:
			return FName(TEXT("TranslateY"));
		case EScenarioTransformGizmoHandle::TranslateZ:
			return FName(TEXT("TranslateZ"));
		case EScenarioTransformGizmoHandle::TranslateXY:
			return FName(TEXT("TranslateXY"));
		case EScenarioTransformGizmoHandle::TranslateXZ:
			return FName(TEXT("TranslateXZ"));
		case EScenarioTransformGizmoHandle::TranslateYZ:
			return FName(TEXT("TranslateYZ"));
		case EScenarioTransformGizmoHandle::RotateX:
			return FName(TEXT("RotateX"));
		case EScenarioTransformGizmoHandle::RotateY:
			return FName(TEXT("RotateY"));
		case EScenarioTransformGizmoHandle::RotateZ:
			return FName(TEXT("RotateZ"));
		case EScenarioTransformGizmoHandle::ScaleX:
			return FName(TEXT("ScaleX"));
		case EScenarioTransformGizmoHandle::ScaleY:
			return FName(TEXT("ScaleY"));
		case EScenarioTransformGizmoHandle::ScaleZ:
			return FName(TEXT("ScaleZ"));
		case EScenarioTransformGizmoHandle::ScaleXY:
			return FName(TEXT("ScaleXY"));
		case EScenarioTransformGizmoHandle::ScaleXZ:
			return FName(TEXT("ScaleXZ"));
		case EScenarioTransformGizmoHandle::ScaleYZ:
			return FName(TEXT("ScaleYZ"));
		case EScenarioTransformGizmoHandle::ScaleUniform:
			return FName(TEXT("ScaleUniform"));
		default:
			return NAME_None;
		}
	}

	void SetPlaneHandleVisualCenter(
		UStaticMeshComponent* component,
		const FVector& visualCenter,
		const FRotator& relativeRotation)
	{
		if (!component) return;

		FVector compensatedLocation = visualCenter;
		if (const UStaticMesh* staticMesh = component->GetStaticMesh())
		{
			const FVector scaledBoundsOrigin = staticMesh->GetBounds().Origin * component->GetRelativeScale3D();
			compensatedLocation -= relativeRotation.RotateVector(scaledBoundsOrigin);
		}

		component->SetRelativeLocation(compensatedLocation);
		component->SetRelativeRotation(relativeRotation);
	}
}

AScenarioTransformGizmoActor::AScenarioTransformGizmoActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	TranslateXHandleComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TranslateXHandleComponent"));
	TranslateXHandleComponent->SetupAttachment(SceneRoot);

	TranslateYHandleComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TranslateYHandleComponent"));
	TranslateYHandleComponent->SetupAttachment(SceneRoot);

	TranslateZHandleComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TranslateZHandleComponent"));
	TranslateZHandleComponent->SetupAttachment(SceneRoot);

	TranslateXYHandleComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TranslateXYHandleComponent"));
	TranslateXYHandleComponent->SetupAttachment(SceneRoot);

	TranslateXZHandleComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TranslateXZHandleComponent"));
	TranslateXZHandleComponent->SetupAttachment(SceneRoot);

	TranslateYZHandleComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TranslateYZHandleComponent"));
	TranslateYZHandleComponent->SetupAttachment(SceneRoot);

	RotateXHandleComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RotateXHandleComponent"));
	RotateXHandleComponent->SetupAttachment(SceneRoot);

	RotateYHandleComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RotateYHandleComponent"));
	RotateYHandleComponent->SetupAttachment(SceneRoot);

	RotateZHandleComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RotateZHandleComponent"));
	RotateZHandleComponent->SetupAttachment(SceneRoot);

	ScaleXHandleComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScaleXHandleComponent"));
	ScaleXHandleComponent->SetupAttachment(SceneRoot);

	ScaleYHandleComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScaleYHandleComponent"));
	ScaleYHandleComponent->SetupAttachment(SceneRoot);

	ScaleZHandleComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScaleZHandleComponent"));
	ScaleZHandleComponent->SetupAttachment(SceneRoot);

	ScaleXYHandleComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScaleXYHandleComponent"));
	ScaleXYHandleComponent->SetupAttachment(SceneRoot);

	ScaleXZHandleComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScaleXZHandleComponent"));
	ScaleXZHandleComponent->SetupAttachment(SceneRoot);

	ScaleYZHandleComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScaleYZHandleComponent"));
	ScaleYZHandleComponent->SetupAttachment(SceneRoot);

	ScaleUniformHandleComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScaleUniformHandleComponent"));
	ScaleUniformHandleComponent->SetupAttachment(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> translationHandleMesh(
		TEXT("/Game/Models/Gizmo/SM_TranslationHandle.SM_TranslationHandle"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> planeHandleMesh(
		TEXT("/Game/Models/Gizmo/SM_PlaneHandle.SM_PlaneHandle"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> rotationHandleMesh(
		TEXT("/Game/Models/Gizmo/SM_RotationHandle.SM_RotationHandle"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> scaleHandleMesh(
		TEXT("/Game/Models/Gizmo/SM_ScaleHandle.SM_ScaleHandle"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> uniformScaleHandleMesh(
		TEXT("/Game/Models/Gizmo/SM_UniformScaleHandle.SM_UniformScaleHandle"));

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> xAxisMaterial(
		TEXT("/Game/Materials/Gizmo/MI_XAxis.MI_XAxis"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> yAxisMaterial(
		TEXT("/Game/Materials/Gizmo/MI_YAxis.MI_YAxis"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> xyAxisMaterial(
		TEXT("/Game/Materials/Gizmo/MI_ZAxis.MI_ZAxis"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> zAxisMaterial(
		TEXT("/Game/Materials/Gizmo/MI_ZAxis.MI_ZAxis"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> rotationXAxisMaterial(
		TEXT("/Game/Materials/Gizmo/MI_Rotation_XAxis.MI_Rotation_XAxis"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> rotationYAxisMaterial(
		TEXT("/Game/Materials/Gizmo/MI_Rotation_YAxis.MI_Rotation_YAxis"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> rotationZAxisMaterial(
		TEXT("/Game/Materials/Gizmo/MI_Rotation_ZAxis.MI_Rotation_ZAxis"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> hoveredMaterial(
		TEXT("/Game/Materials/Gizmo/MI_Hovered.MI_Hovered"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ghostMaterial(
		TEXT("/Game/Materials/Gizmo/MI_Ghost.MI_Ghost"));

	XAxisMaterial = xAxisMaterial.Succeeded() ? xAxisMaterial.Object : nullptr;
	YAxisMaterial = yAxisMaterial.Succeeded() ? yAxisMaterial.Object : nullptr;
	XYAxisMaterial = xyAxisMaterial.Succeeded() ? xyAxisMaterial.Object : nullptr;
	ZAxisMaterial = zAxisMaterial.Succeeded() ? zAxisMaterial.Object : nullptr;
	RotationXAxisMaterial = rotationXAxisMaterial.Succeeded() ? rotationXAxisMaterial.Object : nullptr;
	RotationYAxisMaterial = rotationYAxisMaterial.Succeeded() ? rotationYAxisMaterial.Object : nullptr;
	RotationZAxisMaterial = rotationZAxisMaterial.Succeeded() ? rotationZAxisMaterial.Object : nullptr;
	HoveredMaterial = hoveredMaterial.Succeeded() ? hoveredMaterial.Object : nullptr;
	GhostMaterial = ghostMaterial.Succeeded() ? ghostMaterial.Object : nullptr;

	if (translationHandleMesh.Succeeded())
	{
		TranslateXHandleComponent->SetStaticMesh(translationHandleMesh.Object);
		TranslateYHandleComponent->SetStaticMesh(translationHandleMesh.Object);
		TranslateZHandleComponent->SetStaticMesh(translationHandleMesh.Object);
	}
	if (planeHandleMesh.Succeeded())
	{
		TranslateXYHandleComponent->SetStaticMesh(planeHandleMesh.Object);
		TranslateXZHandleComponent->SetStaticMesh(planeHandleMesh.Object);
		TranslateYZHandleComponent->SetStaticMesh(planeHandleMesh.Object);
		ScaleXYHandleComponent->SetStaticMesh(planeHandleMesh.Object);
		ScaleXZHandleComponent->SetStaticMesh(planeHandleMesh.Object);
		ScaleYZHandleComponent->SetStaticMesh(planeHandleMesh.Object);
	}
	if (rotationHandleMesh.Succeeded())
	{
		RotateXHandleComponent->SetStaticMesh(rotationHandleMesh.Object);
		RotateYHandleComponent->SetStaticMesh(rotationHandleMesh.Object);
		RotateZHandleComponent->SetStaticMesh(rotationHandleMesh.Object);
	}
	if (scaleHandleMesh.Succeeded())
	{
		ScaleXHandleComponent->SetStaticMesh(scaleHandleMesh.Object);
		ScaleYHandleComponent->SetStaticMesh(scaleHandleMesh.Object);
		ScaleZHandleComponent->SetStaticMesh(scaleHandleMesh.Object);
	}
	if (uniformScaleHandleMesh.Succeeded())
	{
		ScaleUniformHandleComponent->SetStaticMesh(uniformScaleHandleMesh.Object);
	}

	ConfigureHandleComponent(
		TranslateXHandleComponent,
		EScenarioTransformGizmoHandle::TranslateX,
		XAxisMaterial);
	ConfigureHandleComponent(
		TranslateYHandleComponent,
		EScenarioTransformGizmoHandle::TranslateY,
		YAxisMaterial);
	ConfigureHandleComponent(
		TranslateZHandleComponent,
		EScenarioTransformGizmoHandle::TranslateZ,
		ZAxisMaterial);
	ConfigureHandleComponent(
		TranslateXYHandleComponent,
		EScenarioTransformGizmoHandle::TranslateXY,
		XYAxisMaterial);
	ConfigureHandleComponent(
		TranslateXZHandleComponent,
		EScenarioTransformGizmoHandle::TranslateXZ,
		YAxisMaterial);
	ConfigureHandleComponent(
		TranslateYZHandleComponent,
		EScenarioTransformGizmoHandle::TranslateYZ,
		XAxisMaterial);
	ConfigureHandleComponent(
		RotateXHandleComponent,
		EScenarioTransformGizmoHandle::RotateX,
		RotationXAxisMaterial);
	ConfigureHandleComponent(
		RotateYHandleComponent,
		EScenarioTransformGizmoHandle::RotateY,
		RotationYAxisMaterial);
	ConfigureHandleComponent(
		RotateZHandleComponent,
		EScenarioTransformGizmoHandle::RotateZ,
		RotationZAxisMaterial);
	ConfigureHandleComponent(
		ScaleXHandleComponent,
		EScenarioTransformGizmoHandle::ScaleX,
		XAxisMaterial);
	ConfigureHandleComponent(
		ScaleYHandleComponent,
		EScenarioTransformGizmoHandle::ScaleY,
		YAxisMaterial);
	ConfigureHandleComponent(
		ScaleZHandleComponent,
		EScenarioTransformGizmoHandle::ScaleZ,
		ZAxisMaterial);
	ConfigureHandleComponent(
		ScaleXYHandleComponent,
		EScenarioTransformGizmoHandle::ScaleXY,
		XYAxisMaterial);
	ConfigureHandleComponent(
		ScaleXZHandleComponent,
		EScenarioTransformGizmoHandle::ScaleXZ,
		YAxisMaterial);
	ConfigureHandleComponent(
		ScaleYZHandleComponent,
		EScenarioTransformGizmoHandle::ScaleYZ,
		XAxisMaterial);
	ConfigureHandleComponent(
		ScaleUniformHandleComponent,
		EScenarioTransformGizmoHandle::ScaleUniform,
		XYAxisMaterial);

	constexpr double PlaneHandleOffsetCm = 36.0;
	constexpr double PlaneLiftCm = 2.0;
	constexpr double ScaleAxisLengthScale = 128.0 / 117.0;

	TranslateYHandleComponent->SetRelativeRotation(FRotator(0.0, 90.0, 0.0));
	TranslateZHandleComponent->SetRelativeRotation(FRotator(90.0, 0.0, 0.0));

	// SM_PlaneHandle is authored as a YZ plane with +X as its normal and an off-center pivot.
	SetPlaneHandleVisualCenter(
		TranslateXYHandleComponent,
		FVector(PlaneHandleOffsetCm, PlaneHandleOffsetCm, PlaneLiftCm),
		FRotator(90.0, 0.0, 0.0));
	SetPlaneHandleVisualCenter(
		TranslateXZHandleComponent,
		FVector(PlaneHandleOffsetCm, 0.0, PlaneHandleOffsetCm),
		FRotator(0.0, 90.0, 0.0));
	SetPlaneHandleVisualCenter(
		TranslateYZHandleComponent,
		FVector(0.0, PlaneHandleOffsetCm, PlaneHandleOffsetCm),
		FRotator::ZeroRotator);
	RotateXHandleComponent->SetRelativeRotation(FRotator(0.0, 90.0, 0.0));
	RotateYHandleComponent->SetRelativeRotation(FRotator(0.0, 0.0, 90.0));
	RotateZHandleComponent->SetRelativeLocation(FVector(0.0, 0.0, 2.0));
	RotateZHandleComponent->SetRelativeRotation(FRotator(90.0, 0.0, 0.0));
	RotateZHandleComponent->SetRelativeScale3D(FVector(1.35, 1.35, 1.35));
	RotateXHandleComponent->SetRelativeScale3D(FVector(1.35, 1.35, 1.35));
	RotateYHandleComponent->SetRelativeScale3D(FVector(1.35, 1.35, 1.35));
	ScaleXHandleComponent->SetRelativeScale3D(FVector(ScaleAxisLengthScale, 1.0, 1.0));
	ScaleYHandleComponent->SetRelativeScale3D(FVector(ScaleAxisLengthScale, 1.0, 1.0));
	ScaleYHandleComponent->SetRelativeRotation(FRotator(0.0, 90.0, 0.0));
	ScaleZHandleComponent->SetRelativeScale3D(FVector(ScaleAxisLengthScale, 1.0, 1.0));
	ScaleZHandleComponent->SetRelativeRotation(FRotator(90.0, 0.0, 0.0));
	SetPlaneHandleVisualCenter(
		ScaleXYHandleComponent,
		FVector(PlaneHandleOffsetCm, PlaneHandleOffsetCm, PlaneLiftCm),
		FRotator(90.0, 0.0, 0.0));
	SetPlaneHandleVisualCenter(
		ScaleXZHandleComponent,
		FVector(PlaneHandleOffsetCm, 0.0, PlaneHandleOffsetCm),
		FRotator(0.0, 90.0, 0.0));
	SetPlaneHandleVisualCenter(
		ScaleYZHandleComponent,
		FVector(0.0, PlaneHandleOffsetCm, PlaneHandleOffsetCm),
		FRotator::ZeroRotator);
	ScaleUniformHandleComponent->SetRelativeLocation(
		FVector(PlaneHandleOffsetCm, PlaneHandleOffsetCm, PlaneHandleOffsetCm));

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
}

void AScenarioTransformGizmoActor::Tick(float deltaSeconds)
{
	Super::Tick(deltaSeconds);

	RefreshFromTarget();
	UpdateScreenScale();
}

void AScenarioTransformGizmoActor::ShowForTarget(AActor* targetActor)
{
	if (!targetActor)
	{
		HideGizmo();
		return;
	}

	TargetActor = targetActor;
	HoveredHandle = EScenarioTransformGizmoHandle::None;
	ApplyHandleMaterials();
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorTickEnabled(true);
	RefreshFromTarget();
	UpdateScreenScale();
}

void AScenarioTransformGizmoActor::HideGizmo()
{
	TargetActor.Reset();
	HoveredHandle = EScenarioTransformGizmoHandle::None;
	ApplyHandleMaterials();
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
}

void AScenarioTransformGizmoActor::RefreshFromTarget()
{
	AActor* targetActor = TargetActor.Get();
	if (!targetActor)
	{
		HideGizmo();
		return;
	}

	SetActorLocationAndRotation(
		targetActor->GetActorLocation(),
		OrientationMode == EScenarioTransformGizmoOrientationMode::World
			? FRotator::ZeroRotator
			: targetActor->GetActorRotation(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
}

void AScenarioTransformGizmoActor::SetHoveredHandle(EScenarioTransformGizmoHandle handle)
{
	if (!IsHandleEnabled(handle))
	{
		handle = EScenarioTransformGizmoHandle::None;
	}

	if (HoveredHandle == handle)
	{
		return;
	}

	HoveredHandle = handle;
	ApplyHandleMaterials();
}

void AScenarioTransformGizmoActor::SetGizmoMode(EScenarioTransformGizmoMode mode)
{
	if (GizmoMode == mode)
	{
		return;
	}

	GizmoMode = mode;
	HoveredHandle = EScenarioTransformGizmoHandle::None;
	ApplyHandleMaterials();
}

void AScenarioTransformGizmoActor::SetGizmoOrientationMode(
	EScenarioTransformGizmoOrientationMode orientationMode)
{
	if (OrientationMode == orientationMode)
	{
		return;
	}

	OrientationMode = orientationMode;
	RefreshFromTarget();
}

bool AScenarioTransformGizmoActor::IsHandleEnabled(EScenarioTransformGizmoHandle handle) const
{
	if (!IsHandleVisibleInMode(handle))
	{
		return false;
	}

	switch (GizmoMode)
	{
	case EScenarioTransformGizmoMode::Translate:
		return handle == EScenarioTransformGizmoHandle::TranslateX
			|| handle == EScenarioTransformGizmoHandle::TranslateY
			|| handle == EScenarioTransformGizmoHandle::TranslateXY;
	case EScenarioTransformGizmoMode::Rotate:
		return handle == EScenarioTransformGizmoHandle::RotateZ;
	case EScenarioTransformGizmoMode::Scale:
		return false;
	default:
		return false;
	}
}

EScenarioTransformGizmoHandle AScenarioTransformGizmoActor::GetHandleForComponent(
	const UPrimitiveComponent* component) const
{
	if (component == TranslateXHandleComponent.Get())
	{
		return EScenarioTransformGizmoHandle::TranslateX;
	}
	if (component == TranslateYHandleComponent.Get())
	{
		return EScenarioTransformGizmoHandle::TranslateY;
	}
	if (component == TranslateZHandleComponent.Get())
	{
		return EScenarioTransformGizmoHandle::TranslateZ;
	}
	if (component == TranslateXYHandleComponent.Get())
	{
		return EScenarioTransformGizmoHandle::TranslateXY;
	}
	if (component == TranslateXZHandleComponent.Get())
	{
		return EScenarioTransformGizmoHandle::TranslateXZ;
	}
	if (component == TranslateYZHandleComponent.Get())
	{
		return EScenarioTransformGizmoHandle::TranslateYZ;
	}
	if (component == RotateXHandleComponent.Get())
	{
		return EScenarioTransformGizmoHandle::RotateX;
	}
	if (component == RotateYHandleComponent.Get())
	{
		return EScenarioTransformGizmoHandle::RotateY;
	}
	if (component == RotateZHandleComponent.Get())
	{
		return EScenarioTransformGizmoHandle::RotateZ;
	}
	if (component == ScaleXHandleComponent.Get())
	{
		return EScenarioTransformGizmoHandle::ScaleX;
	}
	if (component == ScaleYHandleComponent.Get())
	{
		return EScenarioTransformGizmoHandle::ScaleY;
	}
	if (component == ScaleZHandleComponent.Get())
	{
		return EScenarioTransformGizmoHandle::ScaleZ;
	}
	if (component == ScaleXYHandleComponent.Get())
	{
		return EScenarioTransformGizmoHandle::ScaleXY;
	}
	if (component == ScaleXZHandleComponent.Get())
	{
		return EScenarioTransformGizmoHandle::ScaleXZ;
	}
	if (component == ScaleYZHandleComponent.Get())
	{
		return EScenarioTransformGizmoHandle::ScaleYZ;
	}
	if (component == ScaleUniformHandleComponent.Get())
	{
		return EScenarioTransformGizmoHandle::ScaleUniform;
	}

	return EScenarioTransformGizmoHandle::None;
}

void AScenarioTransformGizmoActor::ConfigureHandleComponent(
	UStaticMeshComponent* component,
	EScenarioTransformGizmoHandle handle,
	UMaterialInterface* material) const
{
	if (!component) return;

	component->SetMobility(EComponentMobility::Movable);
	component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	component->SetCollisionObjectType(ECC_WorldDynamic);
	component->SetCollisionResponseToAllChannels(ECR_Ignore);
	component->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	component->SetGenerateOverlapEvents(false);
	component->SetCastShadow(false);
	component->SetTranslucentSortPriority(20);
	component->ComponentTags.AddUnique(FName(TEXT("EpisodeTransformGizmo")));
	const FName handleTag = TransformGizmoHandleTag(handle);
	if (!handleTag.IsNone())
	{
		component->ComponentTags.AddUnique(handleTag);
	}
	ApplyHandleMaterial(component, handle, material);
}

void AScenarioTransformGizmoActor::UpdateScreenScale()
{
	const UWorld* world = GetWorld();
	if (!world) return;

	const APlayerController* playerController = world->GetFirstPlayerController();
	const APlayerCameraManager* cameraManager = playerController ? playerController->PlayerCameraManager : nullptr;
	if (!cameraManager) return;

	const double distanceCm = FVector::Dist(cameraManager->GetCameraLocation(), GetActorLocation());
	const double scale = FMath::Clamp(
		distanceCm * ScreenScalePerDistanceCm,
		MinScreenScale,
		MaxScreenScale);
	SetActorScale3D(FVector(scale));
}

void AScenarioTransformGizmoActor::ApplyHandleMaterials()
{
	ApplyHandleMaterial(TranslateXHandleComponent, EScenarioTransformGizmoHandle::TranslateX, XAxisMaterial);
	ApplyHandleMaterial(TranslateYHandleComponent, EScenarioTransformGizmoHandle::TranslateY, YAxisMaterial);
	ApplyHandleMaterial(TranslateZHandleComponent, EScenarioTransformGizmoHandle::TranslateZ, ZAxisMaterial);
	ApplyHandleMaterial(TranslateXYHandleComponent, EScenarioTransformGizmoHandle::TranslateXY, XYAxisMaterial);
	ApplyHandleMaterial(TranslateXZHandleComponent, EScenarioTransformGizmoHandle::TranslateXZ, YAxisMaterial);
	ApplyHandleMaterial(TranslateYZHandleComponent, EScenarioTransformGizmoHandle::TranslateYZ, XAxisMaterial);
	ApplyHandleMaterial(RotateXHandleComponent, EScenarioTransformGizmoHandle::RotateX, RotationXAxisMaterial);
	ApplyHandleMaterial(RotateYHandleComponent, EScenarioTransformGizmoHandle::RotateY, RotationYAxisMaterial);
	ApplyHandleMaterial(RotateZHandleComponent, EScenarioTransformGizmoHandle::RotateZ, RotationZAxisMaterial);
	ApplyHandleMaterial(ScaleXHandleComponent, EScenarioTransformGizmoHandle::ScaleX, XAxisMaterial);
	ApplyHandleMaterial(ScaleYHandleComponent, EScenarioTransformGizmoHandle::ScaleY, YAxisMaterial);
	ApplyHandleMaterial(ScaleZHandleComponent, EScenarioTransformGizmoHandle::ScaleZ, ZAxisMaterial);
	ApplyHandleMaterial(ScaleXYHandleComponent, EScenarioTransformGizmoHandle::ScaleXY, XYAxisMaterial);
	ApplyHandleMaterial(ScaleXZHandleComponent, EScenarioTransformGizmoHandle::ScaleXZ, YAxisMaterial);
	ApplyHandleMaterial(ScaleYZHandleComponent, EScenarioTransformGizmoHandle::ScaleYZ, XAxisMaterial);
	ApplyHandleMaterial(ScaleUniformHandleComponent, EScenarioTransformGizmoHandle::ScaleUniform, XYAxisMaterial);
}

void AScenarioTransformGizmoActor::ApplyHandleMaterial(
	UStaticMeshComponent* component,
	EScenarioTransformGizmoHandle handle,
	UMaterialInterface* defaultMaterial) const
{
	if (!component) return;

	ApplyHandleState(component, handle);
	if (!IsHandleVisibleInMode(handle))
	{
		return;
	}

	UMaterialInterface* material = IsHandleEnabled(handle) ? defaultMaterial : GhostMaterial.Get();
	if (IsHandleEnabled(handle) && HoveredHandle == handle && HoveredMaterial)
	{
		material = HoveredMaterial.Get();
	}

	if (material)
	{
		component->SetMaterial(0, material);
	}
}

bool AScenarioTransformGizmoActor::IsHandleVisibleInMode(EScenarioTransformGizmoHandle handle) const
{
	switch (GizmoMode)
	{
	case EScenarioTransformGizmoMode::Translate:
		return handle == EScenarioTransformGizmoHandle::TranslateX
			|| handle == EScenarioTransformGizmoHandle::TranslateY
			|| handle == EScenarioTransformGizmoHandle::TranslateZ
			|| handle == EScenarioTransformGizmoHandle::TranslateXY
			|| handle == EScenarioTransformGizmoHandle::TranslateXZ
			|| handle == EScenarioTransformGizmoHandle::TranslateYZ;
	case EScenarioTransformGizmoMode::Rotate:
		return handle == EScenarioTransformGizmoHandle::RotateX
			|| handle == EScenarioTransformGizmoHandle::RotateY
			|| handle == EScenarioTransformGizmoHandle::RotateZ;
	case EScenarioTransformGizmoMode::Scale:
		return handle == EScenarioTransformGizmoHandle::ScaleX
			|| handle == EScenarioTransformGizmoHandle::ScaleY
			|| handle == EScenarioTransformGizmoHandle::ScaleZ
			|| handle == EScenarioTransformGizmoHandle::ScaleXY
			|| handle == EScenarioTransformGizmoHandle::ScaleXZ
			|| handle == EScenarioTransformGizmoHandle::ScaleYZ
			|| handle == EScenarioTransformGizmoHandle::ScaleUniform;
	default:
		return false;
	}
}

void AScenarioTransformGizmoActor::ApplyHandleState(
	UStaticMeshComponent* component,
	EScenarioTransformGizmoHandle handle) const
{
	if (!component) return;

	// 실제로 동작하는(enabled) 핸들만 노출함. Z 이동, X/Y 회전, 모든 scale 핸들은
	// 인터랙션이 비활성이므로 시각적으로도 숨겨 2.5D 에디터에 맞춤.
	const bool bEnabled = IsHandleEnabled(handle);
	component->SetVisibility(bEnabled, true);
	component->SetHiddenInGame(!bEnabled);
	component->SetCollisionEnabled(
		bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}
