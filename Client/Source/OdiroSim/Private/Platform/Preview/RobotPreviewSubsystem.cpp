#include "Platform/Preview/RobotPreviewSubsystem.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Platform/Preview/RobotPreviewSceneActor.h"
#include "TimerManager.h"

namespace
{
	constexpr float RobotPreviewMinCameraPitchDegrees = 8.0f;
	constexpr float RobotPreviewMaxCameraPitchDegrees = 74.0f;
	constexpr float RobotPreviewOrbitDegreesPerPixel = 0.18f;
	constexpr float RobotPreviewZoomStepRatio = 0.88f;
	constexpr float RobotPreviewCameraFovDegrees = 48.0f;

	const TCHAR* ResolveRobotPreviewDensityLabel(const ERobotPreviewLidarDisplayDensity Density)
	{
		switch (Density)
		{
		case ERobotPreviewLidarDisplayDensity::Sparse:
			return TEXT("Sparse");
		case ERobotPreviewLidarDisplayDensity::Dense:
			return TEXT("Dense");
		case ERobotPreviewLidarDisplayDensity::Standard:
		default:
			return TEXT("Standard");
		}
	}
}

void URobotPreviewSubsystem::Deinitialize()
{
	CleanupPreviewResources();
	Super::Deinitialize();
}

bool URobotPreviewSubsystem::StartPreview(UObject* Owner, const FRobotProfileSettings& Settings)
{
	if (!Owner)
	{
		StatusText = TEXT("Preview owner is unavailable.");
		return false;
	}

	if (PreviewOwner.IsValid() && PreviewOwner.Get() != Owner)
	{
		CleanupPreviewResources();
	}

	PreviewOwner = Owner;
	if (IsUsingSceneCaptureRenderTarget() && !IsValid(PreviewRenderTarget))
	{
		PreviewRenderTarget = CreatePreviewRenderTarget();
	}

	if (IsUsingSceneCaptureRenderTarget() && !IsValid(PreviewRenderTarget))
	{
		StatusText = TEXT("Preview RenderTarget 생성 실패");
		return false;
	}

	CurrentSettings = Settings;
	if (!EnsurePreviewScene())
	{
		return false;
	}

	PreviewSceneActor->ApplySettings(CurrentSettings);
	PreviewSceneActor->SetLidarDisplayOptions(LidarDisplayOptions);
	PreviewSceneActor->SetRobotYawDegrees(RobotYawDegrees);
	bCameraViewInitialized = false;
	InitializeCameraViewFromPreviewBounds();
	if (!EnsurePreviewRenderBackend())
	{
		return false;
	}
	RefreshPreviewView();
	if (IsUsingSceneCaptureRenderTarget())
	{
		ScheduleDeferredCaptures();
	}
	RefreshStatusText();
	return true;
}

void URobotPreviewSubsystem::StopPreview(const UObject* Owner)
{
	if (!IsCurrentOwner(Owner))
	{
		return;
	}

	CleanupPreviewResources();
}

bool URobotPreviewSubsystem::ApplyPreviewSettings(const FRobotProfileSettings& Settings)
{
	CurrentSettings = Settings;
	const bool bHadCameraViewInitialized = bCameraViewInitialized;
	if (!EnsurePreviewScene())
	{
		return false;
	}

	PreviewSceneActor->ApplySettings(CurrentSettings);
	PreviewSceneActor->SetLidarDisplayOptions(LidarDisplayOptions);
	PreviewSceneActor->SetRobotYawDegrees(RobotYawDegrees);
	if (!bHadCameraViewInitialized)
	{
		bCameraViewInitialized = false;
	}
	InitializeCameraViewFromPreviewBounds();
	if (!EnsurePreviewRenderBackend())
	{
		return false;
	}
	RefreshPreviewView();
	RefreshStatusText();
	return true;
}

bool URobotPreviewSubsystem::DrawLidarPreviewRays()
{
	if (!EnsurePreviewScene())
	{
		return false;
	}

	PreviewSceneActor->DrawLidarPreviewRays();
	RefreshPreviewView();
	RefreshStatusText();
	return true;
}

void URobotPreviewSubsystem::SetLidarDisplayOptions(const FRobotPreviewLidarDisplayOptions& Options)
{
	LidarDisplayOptions = Options;
	if (IsValid(PreviewSceneActor))
	{
		PreviewSceneActor->SetLidarDisplayOptions(LidarDisplayOptions);
		RefreshPreviewView();
	}
	RefreshStatusText();
}

void URobotPreviewSubsystem::ClearLidarPreviewRays()
{
	if (IsValid(PreviewSceneActor))
	{
		PreviewSceneActor->ClearLidarPreviewRays();
		RefreshPreviewView();
	}
	RefreshStatusText();
}

void URobotPreviewSubsystem::AddCameraZoom(const float WheelDelta)
{
	if (!IsValid(PreviewSceneActor))
	{
		return;
	}

	InitializeCameraViewFromPreviewBounds();

	const float PreviewRadiusCm = FMath::Max(PreviewSceneActor->GetPreviewRadiusCm(), 25.0f);
	const float MinDistanceCm = FMath::Max(PreviewRadiusCm * 1.25f, 55.0f);
	const float MaxDistanceCm = FMath::Max(PreviewRadiusCm * 12.0f, 900.0f);
	CameraDistanceCm = FMath::Clamp(
		CameraDistanceCm * FMath::Pow(RobotPreviewZoomStepRatio, WheelDelta),
		MinDistanceCm,
		MaxDistanceCm);

	RefreshPreviewView();
	RefreshStatusText();
}

void URobotPreviewSubsystem::AddCameraOrbit(const FVector2D& CursorDelta)
{
	if (!IsValid(PreviewSceneActor))
	{
		return;
	}

	InitializeCameraViewFromPreviewBounds();

	CameraOrbitYawDegrees =
		FMath::UnwindDegrees(CameraOrbitYawDegrees + CursorDelta.X * RobotPreviewOrbitDegreesPerPixel);
	CameraOrbitPitchDegrees = FMath::Clamp(
		CameraOrbitPitchDegrees + CursorDelta.Y * RobotPreviewOrbitDegreesPerPixel,
		RobotPreviewMinCameraPitchDegrees,
		RobotPreviewMaxCameraPitchDegrees);

	RefreshPreviewView();
	RefreshStatusText();
}

void URobotPreviewSubsystem::AddRobotYawDegrees(const float DeltaDegrees)
{
	RobotYawDegrees = FMath::UnwindDegrees(RobotYawDegrees + DeltaDegrees);
	if (IsValid(PreviewSceneActor))
	{
		PreviewSceneActor->SetRobotYawDegrees(RobotYawDegrees);
		RefreshPreviewView();
	}
	RefreshStatusText();
}

void URobotPreviewSubsystem::ResetRobotYaw()
{
	RobotYawDegrees = 0.0f;
	if (IsValid(PreviewSceneActor))
	{
		PreviewSceneActor->SetRobotYawDegrees(RobotYawDegrees);
		RefreshPreviewView();
	}
	RefreshStatusText();
}

bool URobotPreviewSubsystem::IsUsingSceneCaptureRenderTarget() const
{
	return RenderMode == ERobotPreviewRenderMode::SceneCaptureRenderTarget;
}

void URobotPreviewSubsystem::SetRenderMode(const ERobotPreviewRenderMode NewRenderMode)
{
	if (RenderMode == NewRenderMode)
	{
		return;
	}

	CleanupPreviewResources();
	RenderMode = NewRenderMode;
}

UTextureRenderTarget2D* URobotPreviewSubsystem::CreatePreviewRenderTarget()
{
	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(
		this,
		TEXT("RobotPreviewRenderTarget"),
		RF_Transient);
	if (!IsValid(RenderTarget))
	{
		return nullptr;
	}

	RenderTarget->RenderTargetFormat = RTF_RGBA8_SRGB;
	RenderTarget->ClearColor = FLinearColor(0.015f, 0.025f, 0.035f, 1.0f);
	RenderTarget->bAutoGenerateMips = false;
	RenderTarget->InitAutoFormat(1024, 576);
	RenderTarget->UpdateResourceImmediate(true);
	return RenderTarget;
}

bool URobotPreviewSubsystem::EnsurePreviewScene()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		StatusText = TEXT("Preview world 없음");
		return false;
	}

	if (!IsValid(PreviewSceneActor))
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags |= RF_Transient;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		PreviewSceneActor = World->SpawnActor<ARobotPreviewSceneActor>(
			PreviewWorldOffset,
			FRotator::ZeroRotator,
			SpawnParameters);
		if (!IsValid(PreviewSceneActor))
		{
			StatusText = TEXT("Preview scene actor 생성 실패");
			return false;
		}
	}

	if (IsUsingSceneCaptureRenderTarget() && !IsValid(PreviewCaptureActor))
	{
		PreviewCaptureActor = SpawnPreviewCaptureActor();
		if (!IsValid(PreviewCaptureActor))
		{
			StatusText = TEXT("Preview capture actor 생성 실패");
			return false;
		}
	}

	RefreshCaptureShowOnlyActors();
	UpdatePreviewCaptureView();
	return true;
}

bool URobotPreviewSubsystem::EnsurePreviewRenderBackend()
{
	return IsUsingSceneCaptureRenderTarget()
		? EnsureSceneCaptureBackend()
		: EnsurePlayerViewportBackend();
}

bool URobotPreviewSubsystem::EnsurePlayerViewportBackend()
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || !IsValid(PreviewSceneActor))
	{
		StatusText = TEXT("Preview viewport backend is unavailable.");
		return false;
	}

	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!IsValid(PlayerController))
	{
		StatusText = TEXT("Preview player controller is unavailable.");
		return false;
	}

	if (PreviewPlayerController != PlayerController)
	{
		RestorePreviousViewTarget();
		PreviewPlayerController = PlayerController;
		PreviousViewTarget = PlayerController->GetViewTarget();
	}
	else if (!PreviousViewTarget.IsValid())
	{
		PreviousViewTarget = PlayerController->GetViewTarget();
	}

	if (!IsValid(PreviewCameraActor))
	{
		PreviewCameraActor = SpawnPreviewCameraActor();
		if (!IsValid(PreviewCameraActor))
		{
			StatusText = TEXT("Preview camera actor creation failed.");
			return false;
		}
	}

	UpdatePlayerViewportView();
	if (PlayerController->GetViewTarget() != PreviewCameraActor)
	{
		PlayerController->SetViewTarget(PreviewCameraActor);
	}
	return true;
}

bool URobotPreviewSubsystem::EnsureSceneCaptureBackend()
{
	if (!IsValid(PreviewRenderTarget))
	{
		PreviewRenderTarget = CreatePreviewRenderTarget();
	}

	if (!IsValid(PreviewRenderTarget))
	{
		StatusText = TEXT("Preview RenderTarget creation failed.");
		return false;
	}

	if (!IsValid(PreviewSceneActor))
	{
		StatusText = TEXT("Preview scene actor is unavailable.");
		return false;
	}

	if (!IsValid(PreviewCaptureActor))
	{
		PreviewCaptureActor = SpawnPreviewCaptureActor();
		if (!IsValid(PreviewCaptureActor))
		{
			StatusText = TEXT("Preview capture actor creation failed.");
			return false;
		}
	}

	RefreshCaptureShowOnlyActors();
	UpdatePreviewCaptureView();
	return true;
}

ACameraActor* URobotPreviewSubsystem::SpawnPreviewCameraActor()
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || !IsValid(PreviewSceneActor))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ACameraActor* CameraActor = World->SpawnActor<ACameraActor>(
		PreviewWorldOffset + FVector(-400.0, -280.0, 180.0),
		FRotator::ZeroRotator,
		SpawnParameters);
	if (!IsValid(CameraActor))
	{
		return nullptr;
	}

	CameraActor->Tags.Add(TEXT("RobotPreviewOnly"));
	if (UCameraComponent* CameraComponent = CameraActor->GetCameraComponent())
	{
		CameraComponent->FieldOfView = RobotPreviewCameraFovDegrees;
	}
	const FTransform CameraTransform = CalculatePreviewCameraTransform();
	CameraActor->SetActorLocationAndRotation(CameraTransform.GetLocation(), CameraTransform.GetRotation());
	return CameraActor;
}

ASceneCapture2D* URobotPreviewSubsystem::SpawnPreviewCaptureActor()
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || !IsValid(PreviewRenderTarget) || !IsValid(PreviewSceneActor))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ASceneCapture2D* CaptureActor = World->SpawnActor<ASceneCapture2D>(
		PreviewWorldOffset + FVector(-400.0, -280.0, 180.0),
		FRotator::ZeroRotator,
		SpawnParameters);
	if (!IsValid(CaptureActor))
	{
		return nullptr;
	}

	USceneCaptureComponent2D* CaptureComponent = CaptureActor->GetCaptureComponent2D();
	if (!IsValid(CaptureComponent))
	{
		CaptureActor->Destroy();
		return nullptr;
	}

	CaptureActor->Tags.Add(TEXT("RobotPreviewOnly"));
	CaptureComponent->ProjectionType = ECameraProjectionMode::Perspective;
	CaptureComponent->FOVAngle = RobotPreviewCameraFovDegrees;
	CaptureComponent->PrimitiveRenderMode =
		ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	CaptureComponent->ShowFlags.SetAtmosphere(false);
	CaptureComponent->ShowFlags.SetFog(false);
	CaptureComponent->ShowFlags.SetCloud(false);
	CaptureComponent->ShowFlags.SetSkyLighting(false);
	CaptureComponent->CaptureSource = SCS_FinalColorLDR;
	CaptureComponent->TextureTarget = PreviewRenderTarget;
	CaptureComponent->bCaptureEveryFrame = false;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->bAlwaysPersistRenderingState = false;
	CaptureComponent->bExcludeFromSceneTextureExtents = true;
	CaptureComponent->bUseRayTracingIfEnabled = false;
	RefreshCaptureShowOnlyActors();
	UpdatePreviewCaptureView();
	return CaptureActor;
}

void URobotPreviewSubsystem::RefreshCaptureShowOnlyActors()
{
	if (USceneCaptureComponent2D* CaptureComponent = GetPreviewCaptureComponent())
	{
		CaptureComponent->ShowOnlyActors.Reset();
		if (IsValid(PreviewSceneActor))
		{
			TArray<AActor*> ShowOnlyActors;
			PreviewSceneActor->AddShowOnlyActors(ShowOnlyActors);
			for (AActor* ShowOnlyActor : ShowOnlyActors)
			{
				if (IsValid(ShowOnlyActor))
				{
					CaptureComponent->ShowOnlyActors.Add(ShowOnlyActor);
				}
			}
		}
	}
}

void URobotPreviewSubsystem::UpdatePreviewCaptureView()
{
	if (!IsValid(PreviewCaptureActor) || !IsValid(PreviewSceneActor))
	{
		return;
	}

	const FTransform CameraTransform = CalculatePreviewCameraTransform();
	PreviewCaptureActor->SetActorLocationAndRotation(CameraTransform.GetLocation(), CameraTransform.GetRotation());
}

void URobotPreviewSubsystem::UpdatePlayerViewportView()
{
	if (!IsValid(PreviewCameraActor) || !IsValid(PreviewSceneActor))
	{
		return;
	}

	const FTransform CameraTransform = CalculatePreviewCameraTransform();
	PreviewCameraActor->SetActorLocationAndRotation(CameraTransform.GetLocation(), CameraTransform.GetRotation());
}

FTransform URobotPreviewSubsystem::CalculatePreviewCameraTransform()
{
	if (!IsValid(PreviewSceneActor))
	{
		return FTransform::Identity;
	}

	const FVector FocusLocation = PreviewSceneActor->GetPreviewFocusLocation();
	InitializeCameraViewFromPreviewBounds();

	const float YawRadians = FMath::DegreesToRadians(CameraOrbitYawDegrees);
	const float PitchRadians = FMath::DegreesToRadians(CameraOrbitPitchDegrees);
	const float CosPitch = FMath::Cos(PitchRadians);
	const FVector OrbitDirection(
		FMath::Cos(YawRadians) * CosPitch,
		FMath::Sin(YawRadians) * CosPitch,
		FMath::Sin(PitchRadians));
	const FVector CameraLocation = FocusLocation + OrbitDirection * CameraDistanceCm;
	return FTransform((FocusLocation - CameraLocation).Rotation(), CameraLocation);
}

void URobotPreviewSubsystem::RefreshPreviewView()
{
	if (IsUsingSceneCaptureRenderTarget())
	{
		CapturePreviewScene();
		return;
	}

	UpdatePlayerViewportView();
}

void URobotPreviewSubsystem::InitializeCameraViewFromPreviewBounds()
{
	if (bCameraViewInitialized || !IsValid(PreviewSceneActor))
	{
		return;
	}

	const float PreviewRadiusCm = FMath::Max(PreviewSceneActor->GetPreviewRadiusCm(), 25.0f);
	CameraDistanceCm = FMath::Clamp(PreviewRadiusCm * 3.6f, 110.0f, 900.0f);
	CameraOrbitYawDegrees = -150.0f;
	CameraOrbitPitchDegrees = 24.0f;
	bCameraViewInitialized = true;
}

void URobotPreviewSubsystem::CapturePreviewScene()
{
	RefreshCaptureShowOnlyActors();
	UpdatePreviewCaptureView();
	if (USceneCaptureComponent2D* CaptureComponent = GetPreviewCaptureComponent())
	{
		CaptureComponent->CaptureScene();
	}
}

void URobotPreviewSubsystem::ScheduleDeferredCaptures()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	const float DeferredCaptureDelaysSeconds[] = { 0.05f, 0.16f, 0.35f };
	for (const float DelaySeconds : DeferredCaptureDelaysSeconds)
	{
		FTimerHandle TimerHandle;
		World->GetTimerManager().SetTimer(
			TimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (IsValid(PreviewRenderTarget)
					&& IsValid(PreviewSceneActor)
					&& IsValid(PreviewCaptureActor))
				{
					CapturePreviewScene();
				}
			}),
			DelaySeconds,
			false);
	}
}

USceneCaptureComponent2D* URobotPreviewSubsystem::GetPreviewCaptureComponent() const
{
	return IsValid(PreviewCaptureActor)
		? PreviewCaptureActor->GetCaptureComponent2D()
		: nullptr;
}

void URobotPreviewSubsystem::RestorePreviousViewTarget()
{
	if (!IsValid(PreviewPlayerController))
	{
		PreviewPlayerController = nullptr;
		PreviousViewTarget.Reset();
		return;
	}

	AActor* RestoreTarget = PreviousViewTarget.Get();
	if (IsValid(RestoreTarget) && PreviewPlayerController->GetViewTarget() == PreviewCameraActor)
	{
		PreviewPlayerController->SetViewTarget(RestoreTarget);
	}

	PreviewPlayerController = nullptr;
	PreviousViewTarget.Reset();
}

void URobotPreviewSubsystem::CleanupPreviewResources()
{
	RestorePreviousViewTarget();

	if (IsValid(PreviewCaptureActor))
	{
		PreviewCaptureActor->Destroy();
	}
	PreviewCaptureActor = nullptr;

	if (IsValid(PreviewCameraActor))
	{
		PreviewCameraActor->Destroy();
	}
	PreviewCameraActor = nullptr;

	if (IsValid(PreviewSceneActor))
	{
		PreviewSceneActor->Destroy();
	}
	PreviewSceneActor = nullptr;

	if (IsValid(PreviewRenderTarget))
	{
		PreviewRenderTarget->ReleaseResource();
	}
	PreviewRenderTarget = nullptr;
	PreviewOwner.Reset();
	RobotYawDegrees = 0.0f;
	bCameraViewInitialized = false;
	CameraOrbitYawDegrees = -150.0f;
	CameraOrbitPitchDegrees = 24.0f;
	CameraDistanceCm = 320.0f;
	StatusText = TEXT("Preview 정리됨");
}

bool URobotPreviewSubsystem::IsCurrentOwner(const UObject* Owner) const
{
	return Owner && PreviewOwner.IsValid() && PreviewOwner.Get() == Owner;
}

void URobotPreviewSubsystem::RefreshStatusText()
{
	const int32 RenderedRayCount = IsValid(PreviewSceneActor)
		? PreviewSceneActor->GetRenderedLidarPreviewRayCount()
		: 0;
	const int32 ActualRayCount = IsValid(PreviewSceneActor)
		? PreviewSceneActor->GetActualLidarPreviewRayCount()
		: 0;
	const int32 PointCount = IsValid(PreviewSceneActor)
		? PreviewSceneActor->GetRenderedLidarPreviewPointCount()
		: 0;
	StatusText = FString::Printf(
		TEXT("Preview 표시 중 | %s | LiDAR %.2fm | Yaw %.0f"),
		*CurrentSettings.Lidar.LidarMode,
		CurrentSettings.Lidar.SensorHeightM,
		RobotYawDegrees);
	if (ActualRayCount > 0)
	{
		StatusText += FString::Printf(TEXT(" | Rays %d/%d shown"), RenderedRayCount, ActualRayCount);
	}
	if (PointCount > 0)
	{
		StatusText += FString::Printf(TEXT(" | Points %d"), PointCount);
	}
	StatusText += FString::Printf(TEXT(" | %s"), ResolveRobotPreviewDensityLabel(LidarDisplayOptions.Density));
}
