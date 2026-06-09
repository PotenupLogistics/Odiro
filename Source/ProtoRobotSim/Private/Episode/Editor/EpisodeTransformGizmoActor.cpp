#include "Episode/Editor/EpisodeTransformGizmoActor.h"

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
	FName TransformGizmoHandleTag(EEpisodeTransformGizmoHandle handle)
	{
		switch (handle)
		{
		case EEpisodeTransformGizmoHandle::TranslateX:
			return FName(TEXT("TranslateX"));
		case EEpisodeTransformGizmoHandle::TranslateY:
			return FName(TEXT("TranslateY"));
		case EEpisodeTransformGizmoHandle::TranslateZ:
			return FName(TEXT("TranslateZ"));
		case EEpisodeTransformGizmoHandle::TranslateXY:
			return FName(TEXT("TranslateXY"));
		case EEpisodeTransformGizmoHandle::TranslateXZ:
			return FName(TEXT("TranslateXZ"));
		case EEpisodeTransformGizmoHandle::TranslateYZ:
			return FName(TEXT("TranslateYZ"));
		case EEpisodeTransformGizmoHandle::RotateX:
			return FName(TEXT("RotateX"));
		case EEpisodeTransformGizmoHandle::RotateY:
			return FName(TEXT("RotateY"));
		case EEpisodeTransformGizmoHandle::RotateZ:
			return FName(TEXT("RotateZ"));
		case EEpisodeTransformGizmoHandle::ScaleX:
			return FName(TEXT("ScaleX"));
		case EEpisodeTransformGizmoHandle::ScaleY:
			return FName(TEXT("ScaleY"));
		case EEpisodeTransformGizmoHandle::ScaleZ:
			return FName(TEXT("ScaleZ"));
		case EEpisodeTransformGizmoHandle::ScaleXY:
			return FName(TEXT("ScaleXY"));
		case EEpisodeTransformGizmoHandle::ScaleXZ:
			return FName(TEXT("ScaleXZ"));
		case EEpisodeTransformGizmoHandle::ScaleYZ:
			return FName(TEXT("ScaleYZ"));
		case EEpisodeTransformGizmoHandle::ScaleUniform:
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

AEpisodeTransformGizmoActor::AEpisodeTransformGizmoActor()
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
		EEpisodeTransformGizmoHandle::TranslateX,
		XAxisMaterial);
	ConfigureHandleComponent(
		TranslateYHandleComponent,
		EEpisodeTransformGizmoHandle::TranslateY,
		YAxisMaterial);
	ConfigureHandleComponent(
		TranslateZHandleComponent,
		EEpisodeTransformGizmoHandle::TranslateZ,
		ZAxisMaterial);
	ConfigureHandleComponent(
		TranslateXYHandleComponent,
		EEpisodeTransformGizmoHandle::TranslateXY,
		XYAxisMaterial);
	ConfigureHandleComponent(
		TranslateXZHandleComponent,
		EEpisodeTransformGizmoHandle::TranslateXZ,
		YAxisMaterial);
	ConfigureHandleComponent(
		TranslateYZHandleComponent,
		EEpisodeTransformGizmoHandle::TranslateYZ,
		XAxisMaterial);
	ConfigureHandleComponent(
		RotateXHandleComponent,
		EEpisodeTransformGizmoHandle::RotateX,
		RotationXAxisMaterial);
	ConfigureHandleComponent(
		RotateYHandleComponent,
		EEpisodeTransformGizmoHandle::RotateY,
		RotationYAxisMaterial);
	ConfigureHandleComponent(
		RotateZHandleComponent,
		EEpisodeTransformGizmoHandle::RotateZ,
		RotationZAxisMaterial);
	ConfigureHandleComponent(
		ScaleXHandleComponent,
		EEpisodeTransformGizmoHandle::ScaleX,
		XAxisMaterial);
	ConfigureHandleComponent(
		ScaleYHandleComponent,
		EEpisodeTransformGizmoHandle::ScaleY,
		YAxisMaterial);
	ConfigureHandleComponent(
		ScaleZHandleComponent,
		EEpisodeTransformGizmoHandle::ScaleZ,
		ZAxisMaterial);
	ConfigureHandleComponent(
		ScaleXYHandleComponent,
		EEpisodeTransformGizmoHandle::ScaleXY,
		XYAxisMaterial);
	ConfigureHandleComponent(
		ScaleXZHandleComponent,
		EEpisodeTransformGizmoHandle::ScaleXZ,
		YAxisMaterial);
	ConfigureHandleComponent(
		ScaleYZHandleComponent,
		EEpisodeTransformGizmoHandle::ScaleYZ,
		XAxisMaterial);
	ConfigureHandleComponent(
		ScaleUniformHandleComponent,
		EEpisodeTransformGizmoHandle::ScaleUniform,
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

void AEpisodeTransformGizmoActor::Tick(float deltaSeconds)
{
	Super::Tick(deltaSeconds);

	RefreshFromTarget();
	UpdateScreenScale();
}

void AEpisodeTransformGizmoActor::ShowForTarget(AActor* targetActor)
{
	if (!targetActor)
	{
		HideGizmo();
		return;
	}

	TargetActor = targetActor;
	HoveredHandle = EEpisodeTransformGizmoHandle::None;
	ApplyHandleMaterials();
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorTickEnabled(true);
	RefreshFromTarget();
	UpdateScreenScale();
}

void AEpisodeTransformGizmoActor::HideGizmo()
{
	TargetActor.Reset();
	HoveredHandle = EEpisodeTransformGizmoHandle::None;
	ApplyHandleMaterials();
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
}

void AEpisodeTransformGizmoActor::RefreshFromTarget()
{
	AActor* targetActor = TargetActor.Get();
	if (!targetActor)
	{
		HideGizmo();
		return;
	}

	SetActorLocationAndRotation(
		targetActor->GetActorLocation(),
		OrientationMode == EEpisodeTransformGizmoOrientationMode::World
			? FRotator::ZeroRotator
			: targetActor->GetActorRotation(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
}

void AEpisodeTransformGizmoActor::SetHoveredHandle(EEpisodeTransformGizmoHandle handle)
{
	if (!IsHandleEnabled(handle))
	{
		handle = EEpisodeTransformGizmoHandle::None;
	}

	if (HoveredHandle == handle)
	{
		return;
	}

	HoveredHandle = handle;
	ApplyHandleMaterials();
}

void AEpisodeTransformGizmoActor::SetGizmoMode(EEpisodeTransformGizmoMode mode)
{
	if (GizmoMode == mode)
	{
		return;
	}

	GizmoMode = mode;
	HoveredHandle = EEpisodeTransformGizmoHandle::None;
	ApplyHandleMaterials();
}

void AEpisodeTransformGizmoActor::SetGizmoOrientationMode(
	EEpisodeTransformGizmoOrientationMode orientationMode)
{
	if (OrientationMode == orientationMode)
	{
		return;
	}

	OrientationMode = orientationMode;
	RefreshFromTarget();
}

bool AEpisodeTransformGizmoActor::IsHandleEnabled(EEpisodeTransformGizmoHandle handle) const
{
	if (!IsHandleVisibleInMode(handle))
	{
		return false;
	}

	switch (GizmoMode)
	{
	case EEpisodeTransformGizmoMode::Translate:
		return handle == EEpisodeTransformGizmoHandle::TranslateX
			|| handle == EEpisodeTransformGizmoHandle::TranslateY
			|| handle == EEpisodeTransformGizmoHandle::TranslateXY;
	case EEpisodeTransformGizmoMode::Rotate:
		return handle == EEpisodeTransformGizmoHandle::RotateZ;
	case EEpisodeTransformGizmoMode::Scale:
		return false;
	default:
		return false;
	}
}

EEpisodeTransformGizmoHandle AEpisodeTransformGizmoActor::GetHandleForComponent(
	const UPrimitiveComponent* component) const
{
	if (component == TranslateXHandleComponent.Get())
	{
		return EEpisodeTransformGizmoHandle::TranslateX;
	}
	if (component == TranslateYHandleComponent.Get())
	{
		return EEpisodeTransformGizmoHandle::TranslateY;
	}
	if (component == TranslateZHandleComponent.Get())
	{
		return EEpisodeTransformGizmoHandle::TranslateZ;
	}
	if (component == TranslateXYHandleComponent.Get())
	{
		return EEpisodeTransformGizmoHandle::TranslateXY;
	}
	if (component == TranslateXZHandleComponent.Get())
	{
		return EEpisodeTransformGizmoHandle::TranslateXZ;
	}
	if (component == TranslateYZHandleComponent.Get())
	{
		return EEpisodeTransformGizmoHandle::TranslateYZ;
	}
	if (component == RotateXHandleComponent.Get())
	{
		return EEpisodeTransformGizmoHandle::RotateX;
	}
	if (component == RotateYHandleComponent.Get())
	{
		return EEpisodeTransformGizmoHandle::RotateY;
	}
	if (component == RotateZHandleComponent.Get())
	{
		return EEpisodeTransformGizmoHandle::RotateZ;
	}
	if (component == ScaleXHandleComponent.Get())
	{
		return EEpisodeTransformGizmoHandle::ScaleX;
	}
	if (component == ScaleYHandleComponent.Get())
	{
		return EEpisodeTransformGizmoHandle::ScaleY;
	}
	if (component == ScaleZHandleComponent.Get())
	{
		return EEpisodeTransformGizmoHandle::ScaleZ;
	}
	if (component == ScaleXYHandleComponent.Get())
	{
		return EEpisodeTransformGizmoHandle::ScaleXY;
	}
	if (component == ScaleXZHandleComponent.Get())
	{
		return EEpisodeTransformGizmoHandle::ScaleXZ;
	}
	if (component == ScaleYZHandleComponent.Get())
	{
		return EEpisodeTransformGizmoHandle::ScaleYZ;
	}
	if (component == ScaleUniformHandleComponent.Get())
	{
		return EEpisodeTransformGizmoHandle::ScaleUniform;
	}

	return EEpisodeTransformGizmoHandle::None;
}

void AEpisodeTransformGizmoActor::ConfigureHandleComponent(
	UStaticMeshComponent* component,
	EEpisodeTransformGizmoHandle handle,
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

void AEpisodeTransformGizmoActor::UpdateScreenScale()
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

void AEpisodeTransformGizmoActor::ApplyHandleMaterials()
{
	ApplyHandleMaterial(TranslateXHandleComponent, EEpisodeTransformGizmoHandle::TranslateX, XAxisMaterial);
	ApplyHandleMaterial(TranslateYHandleComponent, EEpisodeTransformGizmoHandle::TranslateY, YAxisMaterial);
	ApplyHandleMaterial(TranslateZHandleComponent, EEpisodeTransformGizmoHandle::TranslateZ, ZAxisMaterial);
	ApplyHandleMaterial(TranslateXYHandleComponent, EEpisodeTransformGizmoHandle::TranslateXY, XYAxisMaterial);
	ApplyHandleMaterial(TranslateXZHandleComponent, EEpisodeTransformGizmoHandle::TranslateXZ, YAxisMaterial);
	ApplyHandleMaterial(TranslateYZHandleComponent, EEpisodeTransformGizmoHandle::TranslateYZ, XAxisMaterial);
	ApplyHandleMaterial(RotateXHandleComponent, EEpisodeTransformGizmoHandle::RotateX, RotationXAxisMaterial);
	ApplyHandleMaterial(RotateYHandleComponent, EEpisodeTransformGizmoHandle::RotateY, RotationYAxisMaterial);
	ApplyHandleMaterial(RotateZHandleComponent, EEpisodeTransformGizmoHandle::RotateZ, RotationZAxisMaterial);
	ApplyHandleMaterial(ScaleXHandleComponent, EEpisodeTransformGizmoHandle::ScaleX, XAxisMaterial);
	ApplyHandleMaterial(ScaleYHandleComponent, EEpisodeTransformGizmoHandle::ScaleY, YAxisMaterial);
	ApplyHandleMaterial(ScaleZHandleComponent, EEpisodeTransformGizmoHandle::ScaleZ, ZAxisMaterial);
	ApplyHandleMaterial(ScaleXYHandleComponent, EEpisodeTransformGizmoHandle::ScaleXY, XYAxisMaterial);
	ApplyHandleMaterial(ScaleXZHandleComponent, EEpisodeTransformGizmoHandle::ScaleXZ, YAxisMaterial);
	ApplyHandleMaterial(ScaleYZHandleComponent, EEpisodeTransformGizmoHandle::ScaleYZ, XAxisMaterial);
	ApplyHandleMaterial(ScaleUniformHandleComponent, EEpisodeTransformGizmoHandle::ScaleUniform, XYAxisMaterial);
}

void AEpisodeTransformGizmoActor::ApplyHandleMaterial(
	UStaticMeshComponent* component,
	EEpisodeTransformGizmoHandle handle,
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

bool AEpisodeTransformGizmoActor::IsHandleVisibleInMode(EEpisodeTransformGizmoHandle handle) const
{
	switch (GizmoMode)
	{
	case EEpisodeTransformGizmoMode::Translate:
		return handle == EEpisodeTransformGizmoHandle::TranslateX
			|| handle == EEpisodeTransformGizmoHandle::TranslateY
			|| handle == EEpisodeTransformGizmoHandle::TranslateZ
			|| handle == EEpisodeTransformGizmoHandle::TranslateXY
			|| handle == EEpisodeTransformGizmoHandle::TranslateXZ
			|| handle == EEpisodeTransformGizmoHandle::TranslateYZ;
	case EEpisodeTransformGizmoMode::Rotate:
		return handle == EEpisodeTransformGizmoHandle::RotateX
			|| handle == EEpisodeTransformGizmoHandle::RotateY
			|| handle == EEpisodeTransformGizmoHandle::RotateZ;
	case EEpisodeTransformGizmoMode::Scale:
		return handle == EEpisodeTransformGizmoHandle::ScaleX
			|| handle == EEpisodeTransformGizmoHandle::ScaleY
			|| handle == EEpisodeTransformGizmoHandle::ScaleZ
			|| handle == EEpisodeTransformGizmoHandle::ScaleXY
			|| handle == EEpisodeTransformGizmoHandle::ScaleXZ
			|| handle == EEpisodeTransformGizmoHandle::ScaleYZ
			|| handle == EEpisodeTransformGizmoHandle::ScaleUniform;
	default:
		return false;
	}
}

void AEpisodeTransformGizmoActor::ApplyHandleState(
	UStaticMeshComponent* component,
	EEpisodeTransformGizmoHandle handle) const
{
	if (!component) return;

	const bool bVisibleInMode = IsHandleVisibleInMode(handle);
	component->SetVisibility(bVisibleInMode, true);
	component->SetHiddenInGame(!bVisibleInMode);
	component->SetCollisionEnabled(
		bVisibleInMode && IsHandleEnabled(handle)
			? ECollisionEnabled::QueryOnly
			: ECollisionEnabled::NoCollision);
}
