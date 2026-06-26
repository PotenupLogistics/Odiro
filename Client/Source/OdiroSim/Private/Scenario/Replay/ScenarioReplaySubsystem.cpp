#include "Scenario/Replay/ScenarioReplaySubsystem.h"

#include "HAL/FileManager.h"
#include "Scenario/Actors/ScenarioCorridorRuntimeActor.h"
#include "Scenario/Actors/ScenarioStaticObstacle.h"
#include "Scenario/Data/ScenarioStaticObstaclePropCatalog.h"
#include "Scenario/ScenarioSampleWorldSpecAdapter.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/Paths.h"
#include "Scenario/Replay/ScenarioReplayDeveloperSettings.h"
#include "Scenario/Replay/DeliveryBotReplayActor.h"
#include "Shared/ScenarioCompileTypes.h"
#include "Shared/ScenarioSampleJson.h"
#include "Shared/ScenarioSpecTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogScenarioReplay, Log, All);

namespace
{
	const TCHAR* ReplayScenarioFileName = TEXT("scenario.json");

	void AppendReplaySchemaDiagnostics(
		const TArray<FScenarioSchemaDiagnostic>& SourceDiagnostics,
		TArray<FString>& OutDiagnostics)
	{
		for (const FScenarioSchemaDiagnostic& Diagnostic : SourceDiagnostics)
		{
			OutDiagnostics.Add(Diagnostic.Message);
		}
	}

	void AppendReplayCompileDiagnostics(
		const TArray<FScenarioCompileDiagnostic>& SourceDiagnostics,
		TArray<FString>& OutDiagnostics)
	{
		for (const FScenarioCompileDiagnostic& Diagnostic : SourceDiagnostics)
		{
			OutDiagnostics.Add(Diagnostic.Message);
		}
	}
}

bool UScenarioReplaySubsystem::LoadEpisodeReplay(
	const FString& EpisodeDirectory,
	TArray<FString>& OutDiagnostics)
{
	OutDiagnostics.Reset();
	CleanupReplayWorld();
	ApplyCameraSettingsFromDefaults();
	PlaybackState = EScenarioReplayPlaybackState::Loading;

	LoadedEpisodeDirectory = EpisodeDirectory.TrimStartAndEnd();
	FPaths::NormalizeDirectoryName(LoadedEpisodeDirectory);
	if (LoadedEpisodeDirectory.IsEmpty())
	{
		OutDiagnostics.Add(TEXT("Replay episode directory must not be empty."));
		PlaybackState = EScenarioReplayPlaybackState::Failed;
		return false;
	}

	const FString ManifestPath = FPaths::Combine(LoadedEpisodeDirectory, TEXT("replay.meta.json"));
	if (!FEpisodeReplayManifestJson::LoadFromFile(ManifestPath, Manifest, OutDiagnostics))
	{
		PlaybackState = EScenarioReplayPlaybackState::Failed;
		return false;
	}

	const FString FramePath = FPaths::Combine(LoadedEpisodeDirectory, Manifest.FrameFile);
	FEpisodeReplayBinaryHeader Header;
	if (!FEpisodeReplayBinary::LoadFramesFromFile(FramePath, Frames, Header, OutDiagnostics))
	{
		PlaybackState = EScenarioReplayPlaybackState::Failed;
		return false;
	}

	if (Header.FrameCount != Manifest.FrameCount)
	{
		OutDiagnostics.Add(TEXT("Replay manifest frame count does not match binary frame count."));
		PlaybackState = EScenarioReplayPlaybackState::Failed;
		return false;
	}

	if (!LoadEpisodeScenarioWorld(LoadedEpisodeDirectory, OutDiagnostics))
	{
		CleanupReplayWorld();
		PlaybackState = EScenarioReplayPlaybackState::Failed;
		return false;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		OutDiagnostics.Add(TEXT("Replay load requires a valid world."));
		PlaybackState = EScenarioReplayPlaybackState::Failed;
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ReplayRobotActor = World->SpawnActor<ADeliveryBotReplayActor>(
		ADeliveryBotReplayActor::StaticClass(),
		ReplayWorldOffset,
		FRotator::ZeroRotator,
		SpawnParameters);
	if (!IsValid(ReplayRobotActor))
	{
		OutDiagnostics.Add(TEXT("Failed to spawn replay robot actor."));
		PlaybackState = EScenarioReplayPlaybackState::Failed;
		return false;
	}

	ReplayRenderTarget = CreateReplayRenderTarget();
	if (!IsValid(ReplayRenderTarget))
	{
		OutDiagnostics.Add(TEXT("Failed to create replay render target."));
		PlaybackState = EScenarioReplayPlaybackState::Failed;
		return false;
	}

	ReplayCaptureActor = SpawnReplayCaptureActor();
	if (!IsValid(ReplayCaptureActor))
	{
		OutDiagnostics.Add(TEXT("Failed to spawn replay capture actor."));
		PlaybackState = EScenarioReplayPlaybackState::Failed;
		return false;
	}

	CurrentReplayTimeSeconds = 0.0;
	if (!ApplyFrameAtTime(CurrentReplayTimeSeconds))
	{
		OutDiagnostics.Add(TEXT("Failed to apply the first replay frame."));
		PlaybackState = EScenarioReplayPlaybackState::Failed;
		return false;
	}

	PlaybackState = EScenarioReplayPlaybackState::Ready;
	UE_LOG(
		LogScenarioReplay,
		Log,
		TEXT("Replay loaded | EpisodeDirectory: %s, Frames: %d, Duration: %.3f"),
		*LoadedEpisodeDirectory,
		Frames.Num(),
		Manifest.DurationSeconds);
	return true;
}

void UScenarioReplaySubsystem::UnloadReplay()
{
	CleanupReplayWorld();
}

void UScenarioReplaySubsystem::Play()
{
	if (PlaybackState == EScenarioReplayPlaybackState::Ready
		|| PlaybackState == EScenarioReplayPlaybackState::Paused)
	{
		PlaybackState = EScenarioReplayPlaybackState::Playing;
	}
}

void UScenarioReplaySubsystem::Pause()
{
	if (PlaybackState == EScenarioReplayPlaybackState::Playing)
	{
		PlaybackState = EScenarioReplayPlaybackState::Paused;
	}
}

void UScenarioReplaySubsystem::Stop()
{
	if (Frames.IsEmpty())
	{
		PlaybackState = EScenarioReplayPlaybackState::Stopped;
		CurrentReplayTimeSeconds = 0.0;
		return;
	}

	CurrentReplayTimeSeconds = 0.0;
	ApplyFrameAtTime(CurrentReplayTimeSeconds);
	PlaybackState = EScenarioReplayPlaybackState::Ready;
}

bool UScenarioReplaySubsystem::Seek(double TimeSeconds)
{
	if (Frames.IsEmpty())
	{
		return false;
	}

	CurrentReplayTimeSeconds = FMath::Clamp(TimeSeconds, 0.0, Manifest.DurationSeconds);
	return ApplyFrameAtTime(CurrentReplayTimeSeconds);
}

double UScenarioReplaySubsystem::GetPlaybackProgress() const
{
	if (Manifest.DurationSeconds <= 0.0)
	{
		return 0.0;
	}

	return FMath::Clamp(
		CurrentReplayTimeSeconds / Manifest.DurationSeconds,
		0.0,
		1.0);
}

bool UScenarioReplaySubsystem::IsReplayCameraInputAllowed() const
{
	return PlaybackState == EScenarioReplayPlaybackState::Playing
		|| (bAllowCameraInputWhilePaused
			&& PlaybackState == EScenarioReplayPlaybackState::Paused);
}

void UScenarioReplaySubsystem::SetReplayCameraMode(EScenarioReplayCameraMode NewMode)
{
	if (CameraMode == NewMode)
	{
		return;
	}

	CameraMode = NewMode;
	if (CameraMode == EScenarioReplayCameraMode::Free)
	{
		const FVector RobotLocation =
			IsValid(ReplayRobotActor)
				? ReplayRobotActor->GetActorLocation()
				: ReplayWorldOffset;
		FreeCameraLocation = RobotLocation + FVector(-900.0, -900.0, 650.0);
		FreeCameraRotation = (RobotLocation - FreeCameraLocation).Rotation();
	}

	if (!Frames.IsEmpty())
	{
		ApplyFrameAtTime(CurrentReplayTimeSeconds);
	}
}

void UScenarioReplaySubsystem::AddFreeCameraMovement(
	const FVector& LocalInput,
	float DeltaSeconds)
{
	if (CameraMode != EScenarioReplayCameraMode::Free
		|| !IsValid(ReplayCaptureActor)
		|| DeltaSeconds <= 0.0f)
	{
		return;
	}

	const FVector ClampedInput = LocalInput.GetClampedToMaxSize(1.0);
	if (ClampedInput.IsNearlyZero())
	{
		return;
	}

	const FRotationMatrix RotationMatrix(FreeCameraRotation);
	const FVector Forward = RotationMatrix.GetUnitAxis(EAxis::X);
	const FVector Right = RotationMatrix.GetUnitAxis(EAxis::Y);
	const FVector Up = FVector::UpVector;

	FreeCameraLocation +=
		(Forward * ClampedInput.X
			+ Right * ClampedInput.Y
			+ Up * ClampedInput.Z)
		* FreeCameraSpeedCmPerSecond
		* static_cast<double>(DeltaSeconds);
	FreeCameraLocation.Z = FMath::Max(FreeCameraLocation.Z, ReplayWorldOffset.Z + 50.0);

	UpdateFreeReplayCamera();
	CaptureReplayScene();
}

void UScenarioReplaySubsystem::AddFreeCameraLook(const FVector2D& MouseDelta)
{
	if (CameraMode != EScenarioReplayCameraMode::Free
		|| !IsValid(ReplayCaptureActor)
		|| MouseDelta.IsNearlyZero())
	{
		return;
	}

	FreeCameraRotation.Yaw += MouseDelta.X * FreeCameraLookSensitivity;
	FreeCameraRotation.Pitch = FMath::Clamp(
		FreeCameraRotation.Pitch - MouseDelta.Y * FreeCameraLookSensitivity,
		MinFreeCameraPitchDegrees,
		MaxFreeCameraPitchDegrees);
	FreeCameraRotation.Roll = 0.0;

	UpdateFreeReplayCamera();
	CaptureReplayScene();
}

void UScenarioReplaySubsystem::AddTopDownZoom(float ZoomDirection)
{
	if (CameraMode != EScenarioReplayCameraMode::TopDown
		|| FMath::IsNearlyZero(ZoomDirection))
	{
		return;
	}

	CaptureOrthoWidthCm = FMath::Clamp(
		CaptureOrthoWidthCm
			- static_cast<double>(ZoomDirection) * TopDownZoomStepCm,
		MinTopDownOrthoWidthCm,
		MaxTopDownOrthoWidthCm);

	if (!Frames.IsEmpty())
	{
		ApplyFrameAtTime(CurrentReplayTimeSeconds);
	}
}

void UScenarioReplaySubsystem::Tick(float DeltaTime)
{
	if (PlaybackState != EScenarioReplayPlaybackState::Playing)
	{
		return;
	}

	CurrentReplayTimeSeconds += FMath::Max(0.0f, DeltaTime) * PlaybackSpeed;
	if (CurrentReplayTimeSeconds >= Manifest.DurationSeconds)
	{
		CurrentReplayTimeSeconds = Manifest.DurationSeconds;
		ApplyFrameAtTime(CurrentReplayTimeSeconds);
		PlaybackState = EScenarioReplayPlaybackState::Ready;
		return;
	}

	ApplyFrameAtTime(CurrentReplayTimeSeconds);
}

bool UScenarioReplaySubsystem::IsTickable() const
{
	return PlaybackState == EScenarioReplayPlaybackState::Playing;
}

TStatId UScenarioReplaySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UScenarioReplaySubsystem, STATGROUP_Tickables);
}

void UScenarioReplaySubsystem::Deinitialize()
{
	CleanupReplayWorld();
	Super::Deinitialize();
}

void UScenarioReplaySubsystem::ApplyCameraSettingsFromDefaults()
{
	if (const UScenarioReplayDeveloperSettings* Settings =
		GetDefault<UScenarioReplayDeveloperSettings>())
	{
		ApplyCameraSettings(*Settings);
	}
}

void UScenarioReplaySubsystem::ApplyCameraSettings(
	const UScenarioReplayDeveloperSettings& Settings)
{
	bAllowCameraInputWhilePaused = Settings.bAllowCameraInputWhilePaused;

	CaptureHeightCm = FMath::Max(1.0, Settings.TopDownCaptureHeightCm);

	const double OrderedMinOrthoWidth =
		FMath::Max(1.0, FMath::Min(
			Settings.MinTopDownOrthoWidthCm,
			Settings.MaxTopDownOrthoWidthCm));
	const double OrderedMaxOrthoWidth =
		FMath::Max(OrderedMinOrthoWidth, FMath::Max(
			Settings.MinTopDownOrthoWidthCm,
			Settings.MaxTopDownOrthoWidthCm));
	MinTopDownOrthoWidthCm = OrderedMinOrthoWidth;
	MaxTopDownOrthoWidthCm = OrderedMaxOrthoWidth;
	CaptureOrthoWidthCm = FMath::Clamp(
		Settings.TopDownOrthoWidthCm,
		MinTopDownOrthoWidthCm,
		MaxTopDownOrthoWidthCm);
	TopDownZoomStepCm = FMath::Max(1.0, Settings.TopDownZoomStepCm);

	FreeCameraSpeedCmPerSecond =
		FMath::Max(1.0, Settings.FreeCameraSpeedCmPerSecond);
	FreeCameraFovDegrees = FMath::Clamp(
		Settings.FreeCameraFovDegrees,
		5.0,
		170.0);
	FreeCameraLookSensitivity =
		FMath::Max(0.001, Settings.FreeCameraLookSensitivity);

	const double OrderedMinPitch = FMath::Clamp(
		FMath::Min(
			Settings.MinFreeCameraPitchDegrees,
			Settings.MaxFreeCameraPitchDegrees),
		-89.0,
		89.0);
	const double OrderedMaxPitch = FMath::Clamp(
		FMath::Max(
			Settings.MinFreeCameraPitchDegrees,
			Settings.MaxFreeCameraPitchDegrees),
		OrderedMinPitch,
		89.0);
	MinFreeCameraPitchDegrees = OrderedMinPitch;
	MaxFreeCameraPitchDegrees = OrderedMaxPitch;

	VehicleFrontCameraLocalOffsetCm =
		Settings.VehicleFrontCameraLocalOffsetCm;
	VehicleFrontCameraLocalRotation =
		Settings.VehicleFrontCameraLocalRotation;
	VehicleFrontCameraFovDegrees = FMath::Clamp(
		Settings.VehicleFrontCameraFovDegrees,
		5.0,
		170.0);
}

void UScenarioReplaySubsystem::CleanupReplayWorld()
{
	if (IsValid(ReplayCaptureActor))
	{
		ReplayCaptureActor->Destroy();
	}
	ReplayCaptureActor = nullptr;

	if (IsValid(ReplayRobotActor))
	{
		ReplayRobotActor->Destroy();
	}
	ReplayRobotActor = nullptr;

	for (int32 Index = ReplayScenarioActors.Num() - 1; Index >= 0; --Index)
	{
		if (AActor* Actor = ReplayScenarioActors[Index].Get())
		{
			Actor->Destroy();
		}
	}
	ReplayScenarioActors.Reset();

	if (IsValid(ReplayRenderTarget))
	{
		ReplayRenderTarget->ReleaseResource();
	}
	ReplayRenderTarget = nullptr;

	Frames.Reset();
	Manifest = FEpisodeReplayManifest{};
	LoadedEpisodeDirectory.Reset();
	CurrentReplayTimeSeconds = 0.0;
	CurrentFrameIndex = INDEX_NONE;
	CurrentRobotSpeedKmh = 0.0;
	CurrentRobotPositionCm = FVector::ZeroVector;
	PlaybackState = EScenarioReplayPlaybackState::Stopped;
	CameraMode = EScenarioReplayCameraMode::TopDown;
	FreeCameraLocation = FVector::ZeroVector;
	FreeCameraRotation = FRotator(-35.0, 0.0, 0.0);
}

bool UScenarioReplaySubsystem::LoadEpisodeScenarioWorld(
	const FString& EpisodeDirectory,
	TArray<FString>& OutDiagnostics)
{
	FString ScenarioPath = Manifest.ScenarioSample.TrimStartAndEnd();
	if (ScenarioPath.IsEmpty())
	{
		ScenarioPath = ReplayScenarioFileName;
	}
	ScenarioPath = FPaths::IsRelative(ScenarioPath)
		? FPaths::Combine(EpisodeDirectory, ScenarioPath)
		: ScenarioPath;
	FPaths::NormalizeFilename(ScenarioPath);

	if (!IFileManager::Get().FileExists(*ScenarioPath))
	{
		OutDiagnostics.Add(FString::Printf(TEXT("Replay scenario map is missing; robot-only replay will be used: %s"), *ScenarioPath));
		return true;
	}

	const FScenarioSampleParseResult ParseResult = FScenarioSampleJson::ParseFromFile(ScenarioPath);
	if (!ParseResult.bSuccess)
	{
		AppendReplaySchemaDiagnostics(ParseResult.Diagnostics, OutDiagnostics);
		return false;
	}

	const FScenarioCompileResult CompileResult =
		FScenarioSampleWorldSpecAdapter::CompileScenarioWorldSpecFromSampleDocument(ParseResult.Document);
	AppendReplayCompileDiagnostics(CompileResult.Diagnostics, OutDiagnostics);
	if (!CompileResult.bSuccess)
	{
		return false;
	}

	return SpawnReplayScenarioWorld(CompileResult.WorldSpec, OutDiagnostics);
}

bool UScenarioReplaySubsystem::SpawnReplayScenarioWorld(
	const FScenarioWorldSpec& WorldSpec,
	TArray<FString>& OutDiagnostics)
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		OutDiagnostics.Add(TEXT("Replay scenario map requires a valid world."));
		return false;
	}

	bool bAllSpawned = true;
	for (const FScenarioRuntimeCorridorSpec& CorridorSpec : WorldSpec.Corridors)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags |= RF_Transient;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AScenarioCorridorRuntimeActor* CorridorActor = World->SpawnActor<AScenarioCorridorRuntimeActor>(
			AScenarioCorridorRuntimeActor::StaticClass(),
			FTransform(FRotator::ZeroRotator, ReplayWorldOffset),
			SpawnParameters);
		if (!IsValid(CorridorActor))
		{
			OutDiagnostics.Add(FString::Printf(TEXT("Failed to spawn replay corridor: %s"), *CorridorSpec.CorridorId));
			bAllSpawned = false;
			continue;
		}

		CorridorActor->ConfigureCorridor(CorridorSpec);
		RegisterReplayScenarioActor(CorridorActor);
	}

	for (const FScenarioPlaceableInstanceSpec& PlaceableSpec : WorldSpec.Placeables)
	{
		if (PlaceableSpec.Category != EScenarioActorCategory::StaticObstacle)
		{
			continue;
		}

		if (!SpawnReplayStaticObstacle(PlaceableSpec, OutDiagnostics))
		{
			bAllSpawned = false;
		}
	}

	return bAllSpawned;
}

bool UScenarioReplaySubsystem::SpawnReplayStaticObstacle(
	const FScenarioPlaceableInstanceSpec& PlaceableSpec,
	TArray<FString>& OutDiagnostics)
{
	FScenarioStaticObstaclePropEntry PropEntry;
	if (!TryFindStaticObstacleProp(FName(*PlaceableSpec.AssetId), PropEntry, OutDiagnostics))
	{
		return false;
	}

	FString FailureReason;
	AScenarioStaticObstacle* StaticObstacle = AScenarioStaticObstacle::SpawnConfigured(
		GetWorld(),
		AScenarioStaticObstacle::StaticClass(),
		MakeReplayWorldTransform(PlaceableSpec.Transform),
		PropEntry,
		FailureReason);
	if (!IsValid(StaticObstacle))
	{
		OutDiagnostics.Add(FString::Printf(
			TEXT("Failed to spawn replay static obstacle '%s': %s"),
			*PlaceableSpec.InstanceId,
			*FailureReason));
		return false;
	}

	RegisterReplayScenarioActor(StaticObstacle);
	return true;
}

bool UScenarioReplaySubsystem::TryFindStaticObstacleProp(
	FName PropId,
	FScenarioStaticObstaclePropEntry& OutPropEntry,
	TArray<FString>& OutDiagnostics) const
{
	if (PropId.IsNone())
	{
		OutDiagnostics.Add(TEXT("Replay static obstacle prop id is empty."));
		return false;
	}

	TSoftObjectPtr<UScenarioStaticObstaclePropCatalog> CatalogRef =
		UScenarioStaticObstaclePropCatalog::MakeDefaultCatalogReference();
	const UScenarioStaticObstaclePropCatalog* Catalog = CatalogRef.LoadSynchronous();
	if (!IsValid(Catalog))
	{
		OutDiagnostics.Add(FString::Printf(
			TEXT("Replay static obstacle prop catalog failed to load: %s"),
			*CatalogRef.ToSoftObjectPath().ToString()));
		return false;
	}

	if (!Catalog->FindPropEntryById(PropId, OutPropEntry))
	{
		OutDiagnostics.Add(FString::Printf(TEXT("Replay static obstacle prop not found: %s"), *PropId.ToString()));
		return false;
	}

	return true;
}

FTransform UScenarioReplaySubsystem::MakeReplayWorldTransform(const FTransform& SourceTransform) const
{
	FTransform Result = SourceTransform;
	Result.SetLocation(SourceTransform.GetLocation() + ReplayWorldOffset);
	return Result;
}

void UScenarioReplaySubsystem::RegisterReplayScenarioActor(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return;
	}

	Actor->Tags.AddUnique(FName(TEXT("ReplayOnly")));
	ReplayScenarioActors.Add(Actor);

	if (USceneCaptureComponent2D* CaptureComponent = GetReplayCaptureComponent())
	{
		CaptureComponent->ShowOnlyActors.Add(Actor);
	}
}

bool UScenarioReplaySubsystem::ApplyFrameAtTime(double TimeSeconds)
{
	if (!IsValid(ReplayRobotActor) || Frames.IsEmpty())
	{
		return false;
	}

	FEpisodeReplayRobotFrame Frame;
	int32 FrameIndex = INDEX_NONE;
	if (!BuildInterpolatedFrameAtTime(TimeSeconds, Frame, FrameIndex))
	{
		return false;
	}

	ReplayRobotActor->ApplyReplayFrame(Frame, ReplayWorldOffset);
	CurrentFrameIndex = FrameIndex;
	CurrentRobotSpeedKmh = Frame.SpeedKmh;
	CurrentRobotPositionCm = Frame.PositionCm;

	UpdateReplayCaptureView(Frame);
	CaptureReplayScene();

	return true;
}

bool UScenarioReplaySubsystem::BuildInterpolatedFrameAtTime(
	double TimeSeconds,
	FEpisodeReplayRobotFrame& OutFrame,
	int32& OutFrameIndex) const
{
	OutFrame = FEpisodeReplayRobotFrame{};
	OutFrameIndex = INDEX_NONE;

	if (Frames.IsEmpty())
	{
		return false;
	}

	if (Frames.Num() == 1)
	{
		OutFrame = Frames[0];
		OutFrameIndex = 0;
		return true;
	}

	const double SampleRateHz = Manifest.SampleRateHz > 0.0
		? Manifest.SampleRateHz
		: EpisodeReplayV1::SampleRateHz;
	const double ClampedTimeSeconds =
		FMath::Clamp(TimeSeconds, 0.0, Manifest.DurationSeconds);
	const double FramePosition =
		FMath::Clamp(
			ClampedTimeSeconds * SampleRateHz,
			0.0,
			static_cast<double>(Frames.Num() - 1));

	const int32 LowerFrameIndex =
		FMath::Clamp(
			FMath::FloorToInt(FramePosition),
			0,
			Frames.Num() - 1);
	const int32 UpperFrameIndex =
		FMath::Clamp(
			LowerFrameIndex + 1,
			0,
			Frames.Num() - 1);
	const double Alpha = FMath::Clamp(
		FramePosition - static_cast<double>(LowerFrameIndex),
		0.0,
		1.0);

	const FEpisodeReplayRobotFrame& LowerFrame = Frames[LowerFrameIndex];
	const FEpisodeReplayRobotFrame& UpperFrame = Frames[UpperFrameIndex];
	if (LowerFrameIndex == UpperFrameIndex || Alpha <= UE_SMALL_NUMBER)
	{
		OutFrame = LowerFrame;
		OutFrameIndex = LowerFrameIndex;
		return true;
	}

	OutFrame.TimeSeconds =
		static_cast<float>(FMath::Lerp(
			static_cast<double>(LowerFrame.TimeSeconds),
			static_cast<double>(UpperFrame.TimeSeconds),
			Alpha));
	OutFrame.PositionCm =
		FMath::Lerp(LowerFrame.PositionCm, UpperFrame.PositionCm, Alpha);
	OutFrame.Rotation =
		FQuat::Slerp(LowerFrame.Rotation, UpperFrame.Rotation, Alpha)
			.GetNormalized();
	OutFrame.VelocityCmPerSecond =
		FMath::Lerp(
			LowerFrame.VelocityCmPerSecond,
			UpperFrame.VelocityCmPerSecond,
			Alpha);
	OutFrame.SpeedKmh =
		static_cast<float>(FMath::Lerp(
			static_cast<double>(LowerFrame.SpeedKmh),
			static_cast<double>(UpperFrame.SpeedKmh),
			Alpha));
	OutFrame.Steering =
		static_cast<float>(FMath::Lerp(
			static_cast<double>(LowerFrame.Steering),
			static_cast<double>(UpperFrame.Steering),
			Alpha));
	OutFrame.Throttle =
		static_cast<float>(FMath::Lerp(
			static_cast<double>(LowerFrame.Throttle),
			static_cast<double>(UpperFrame.Throttle),
			Alpha));
	OutFrame.Brake =
		static_cast<float>(FMath::Lerp(
			static_cast<double>(LowerFrame.Brake),
			static_cast<double>(UpperFrame.Brake),
			Alpha));
	OutFrame.TargetSpeedKmh =
		static_cast<float>(FMath::Lerp(
			static_cast<double>(LowerFrame.TargetSpeedKmh),
			static_cast<double>(UpperFrame.TargetSpeedKmh),
			Alpha));
	OutFrame.Direction = Alpha < 0.5
		? LowerFrame.Direction
		: UpperFrame.Direction;
	OutFrameIndex = FMath::Clamp(
		FMath::RoundToInt(FramePosition),
		0,
		Frames.Num() - 1);
	return true;
}

void UScenarioReplaySubsystem::UpdateReplayCaptureView(
	const FEpisodeReplayRobotFrame& Frame)
{
	switch (CameraMode)
	{
	case EScenarioReplayCameraMode::Free:
		UpdateFreeReplayCamera();
		break;

	case EScenarioReplayCameraMode::VehicleFront:
		UpdateVehicleFrontReplayCamera(Frame);
		break;

	case EScenarioReplayCameraMode::TopDown:
	default:
		UpdateTopDownReplayCamera(Frame);
		break;
	}
}

void UScenarioReplaySubsystem::UpdateTopDownReplayCamera(
	const FEpisodeReplayRobotFrame& Frame)
{
	USceneCaptureComponent2D* CaptureComponent = GetReplayCaptureComponent();
	if (!IsValid(ReplayCaptureActor) || !IsValid(CaptureComponent))
	{
		return;
	}

	CaptureComponent->ProjectionType = ECameraProjectionMode::Orthographic;
	CaptureComponent->OrthoWidth = static_cast<float>(CaptureOrthoWidthCm);
	ReplayCaptureActor->SetActorLocation(
		Frame.PositionCm
		+ ReplayWorldOffset
		+ FVector(0.0, 0.0, CaptureHeightCm));
	ReplayCaptureActor->SetActorRotation(FRotator(-90.0, -90.0, 0.0));
}

void UScenarioReplaySubsystem::UpdateFreeReplayCamera()
{
	USceneCaptureComponent2D* CaptureComponent = GetReplayCaptureComponent();
	if (!IsValid(ReplayCaptureActor) || !IsValid(CaptureComponent))
	{
		return;
	}

	CaptureComponent->ProjectionType = ECameraProjectionMode::Perspective;
	CaptureComponent->FOVAngle = static_cast<float>(FreeCameraFovDegrees);
	ReplayCaptureActor->SetActorLocation(FreeCameraLocation);
	ReplayCaptureActor->SetActorRotation(FreeCameraRotation);
}

void UScenarioReplaySubsystem::UpdateVehicleFrontReplayCamera(
	const FEpisodeReplayRobotFrame& Frame)
{
	USceneCaptureComponent2D* CaptureComponent = GetReplayCaptureComponent();
	if (!IsValid(ReplayCaptureActor) || !IsValid(CaptureComponent))
	{
		return;
	}

	const FTransform RobotWorldTransform(
		Frame.Rotation,
		Frame.PositionCm + ReplayWorldOffset);
	const FVector CameraLocation =
		RobotWorldTransform.TransformPosition(VehicleFrontCameraLocalOffsetCm);
	const FQuat CameraRotation =
		RobotWorldTransform.GetRotation()
		* VehicleFrontCameraLocalRotation.Quaternion();

	CaptureComponent->ProjectionType = ECameraProjectionMode::Perspective;
	CaptureComponent->FOVAngle =
		static_cast<float>(VehicleFrontCameraFovDegrees);
	ReplayCaptureActor->SetActorLocation(CameraLocation);
	ReplayCaptureActor->SetActorRotation(CameraRotation);
}

void UScenarioReplaySubsystem::CaptureReplayScene()
{
	if (USceneCaptureComponent2D* CaptureComponent = GetReplayCaptureComponent())
	{
		CaptureComponent->CaptureScene();
	}
}

UTextureRenderTarget2D* UScenarioReplaySubsystem::CreateReplayRenderTarget()
{
	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(
		this,
		NAME_None,
		RF_Transient);
	if (!IsValid(RenderTarget))
	{
		return nullptr;
	}

	RenderTarget->RenderTargetFormat = RTF_RGBA8_SRGB;
	RenderTarget->ClearColor = FLinearColor(0.04f, 0.05f, 0.06f, 1.0f);
	RenderTarget->bAutoGenerateMips = false;
	RenderTarget->InitAutoFormat(1024, 576);
	RenderTarget->UpdateResourceImmediate(true);
	return RenderTarget;
}

ASceneCapture2D* UScenarioReplaySubsystem::SpawnReplayCaptureActor()
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || !IsValid(ReplayRenderTarget) || !IsValid(ReplayRobotActor))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ASceneCapture2D* CaptureActor = World->SpawnActor<ASceneCapture2D>(
		ReplayWorldOffset + FVector(0.0, 0.0, CaptureHeightCm),
		FRotator(-90.0, -90.0, 0.0),
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

	CaptureActor->Tags.Add(TEXT("ReplayOnly"));
	CaptureComponent->ProjectionType = ECameraProjectionMode::Orthographic;
	CaptureComponent->OrthoWidth = static_cast<float>(CaptureOrthoWidthCm);
	CaptureComponent->PrimitiveRenderMode =
		ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	CaptureComponent->ShowOnlyActors.Reset();
	CaptureComponent->ShowOnlyActors.Add(ReplayRobotActor);
	for (const TObjectPtr<AActor>& ReplayScenarioActor : ReplayScenarioActors)
	{
		if (AActor* Actor = ReplayScenarioActor.Get())
		{
			CaptureComponent->ShowOnlyActors.Add(Actor);
		}
	}
	CaptureComponent->CaptureSource = SCS_FinalColorLDR;
	CaptureComponent->TextureTarget = ReplayRenderTarget;
	CaptureComponent->bCaptureEveryFrame = false;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->bAlwaysPersistRenderingState = false;
	CaptureComponent->bExcludeFromSceneTextureExtents = true;
	CaptureComponent->bUseRayTracingIfEnabled = false;
	return CaptureActor;
}

USceneCaptureComponent2D* UScenarioReplaySubsystem::GetReplayCaptureComponent() const
{
	return IsValid(ReplayCaptureActor)
		? ReplayCaptureActor->GetCaptureComponent2D()
		: nullptr;
}
