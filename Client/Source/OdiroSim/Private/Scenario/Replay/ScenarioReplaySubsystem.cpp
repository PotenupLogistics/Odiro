#include "Scenario/Replay/ScenarioReplaySubsystem.h"

#include "DeliveryBot/Actor/DeliveryBotLidarRayReviewActor.h"
#include "DeliveryBot/Actor/DeliveryBotPointCloudReviewActor.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Scenario/Actors/ScenarioCorridorRuntimeActor.h"
#include "Scenario/Actors/ScenarioStaticObstacle.h"
#include "Scenario/Data/ScenarioStaticObstaclePropCatalog.h"
#include "Scenario/ScenarioSampleWorldSpecAdapter.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Scenario/Replay/ScenarioReplayDeveloperSettings.h"
#include "Scenario/Replay/DeliveryBotReplayActor.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Shared/ScenarioCompileTypes.h"
#include "Shared/ScenarioSampleJson.h"
#include "Shared/ScenarioSpecTypes.h"
#include "Shared/ScenarioViewportPresentation.h"

DEFINE_LOG_CATEGORY_STATIC(LogScenarioReplay, Log, All);

namespace
{
	const TCHAR* ReplayScenarioFileName = TEXT("scenario.json");
	const TCHAR* ReplayArtifactDirectoryName = TEXT("replay");
	const TCHAR* ReplayManifestFileName = TEXT("replay.meta.json");
	const TCHAR* ReplayPointCloudDirectoryName = TEXT("lidar_point_cloud");
	const TCHAR* ReplayPointCloudMapFileName = TEXT("map_accumulated.xyz");
	const TCHAR* ReplayPointCloudManifestFileName = TEXT("manifest.json");
	const TCHAR* ReplayPointCloudCaptureSummaryFileName = TEXT("capture_summary.json");
	const TCHAR* ReplayLidarRayDirectoryName = TEXT("lidar_rays");
	const TCHAR* ReplayLidarRayManifestFileName = TEXT("rays.meta.json");

	// Carries validated point cloud import paths and coordinate metadata.
	struct FReplayPointCloudImportInfo
	{
		// Absolute path to the episode map_accumulated.xyz file.
		FString XyzFilePath;

		// Absolute path to the metadata file that provided coordinate settings.
		FString MetadataFilePath;

		// Capture-space origin used to restore map-local xyz points.
		FVector CaptureOriginCm = FVector::ZeroVector;

		// Y-axis sign used to restore map-local xyz points.
		float ImportYAxisSign = -1.0f;

		// True after all required import files and metadata are valid.
		bool bAvailable = false;
	};

	// Resolves the replay manifest path, preferring the replay folder and falling back to legacy root files.
	bool TryResolveEpisodeReplayManifestPath(
		const FString& EpisodeDirectory,
		FString& OutManifestPath)
	{
		const FString ManifestPath = FPaths::Combine(
			EpisodeDirectory,
			ReplayArtifactDirectoryName,
			ReplayManifestFileName);
		if (IFileManager::Get().FileExists(*ManifestPath))
		{
			OutManifestPath = ManifestPath;
			return true;
		}

		const FString LegacyManifestPath = FPaths::Combine(
			EpisodeDirectory,
			ReplayManifestFileName);
		if (IFileManager::Get().FileExists(*LegacyManifestPath))
		{
			OutManifestPath = LegacyManifestPath;
			return true;
		}

		OutManifestPath = ManifestPath;
		return false;
	}

	// Resolves the binary frame file relative to the manifest that declared it.
	FString ResolveEpisodeReplayFramePath(
		const FString& ManifestPath,
		const FString& FrameFile)
	{
		return FPaths::IsRelative(FrameFile)
			? FPaths::Combine(FPaths::GetPath(ManifestPath), FrameFile)
			: FrameFile;
	}

	// Counts all ray samples loaded for an optional LiDAR ray replay layer.
	int32 CountReplayLidarRaySamples(const TArray<FEpisodeLidarRayFrame>& Frames)
	{
		int32 TotalRayCount = 0;
		for (const FEpisodeLidarRayFrame& Frame : Frames)
		{
			TotalRayCount += Frame.Rays.Num();
		}

		return TotalRayCount;
	}

	// Returns true when loaded LiDAR ray frames are ordered for binary-search lookup.
	bool AreReplayLidarRayFramesSorted(const TArray<FEpisodeLidarRayFrame>& Frames)
	{
		for (int32 FrameIndex = 1; FrameIndex < Frames.Num(); ++FrameIndex)
		{
			if (Frames[FrameIndex].TimeSeconds < Frames[FrameIndex - 1].TimeSeconds)
			{
				return false;
			}
		}

		return true;
	}

	// Appends one optional LiDAR ray load diagnostic with layer context.
	void AppendReplayLidarRayLoadDiagnostic(
		const FString& Diagnostic,
		TArray<FString>& OutDiagnostics)
	{
		OutDiagnostics.Add(FString::Printf(
			TEXT("Replay LiDAR ray layer disabled: %s"),
			*Diagnostic));
	}

	// Appends optional LiDAR ray load diagnostics with layer context.
	void AppendReplayLidarRayLoadDiagnostics(
		const TArray<FString>& SourceDiagnostics,
		TArray<FString>& OutDiagnostics)
	{
		for (const FString& Diagnostic : SourceDiagnostics)
		{
			AppendReplayLidarRayLoadDiagnostic(Diagnostic, OutDiagnostics);
		}
	}

	// Reads a nested object field if it exists and is an object.
	bool TryGetReplayJsonObjectField(
		const FJsonObject& Object,
		const FString& FieldName,
		TSharedPtr<FJsonObject>& OutObject)
	{
		const TSharedPtr<FJsonObject>* ObjectPtr = nullptr;
		if (!Object.TryGetObjectField(FieldName, ObjectPtr)
			|| ObjectPtr == nullptr
			|| !ObjectPtr->IsValid())
		{
			OutObject.Reset();
			return false;
		}

		OutObject = *ObjectPtr;
		return true;
	}

	// Loads one JSON object file for replay point cloud metadata.
	bool LoadReplayJsonObject(
		const FString& JsonPath,
		TSharedPtr<FJsonObject>& OutRootObject,
		FString& OutErrorMessage)
	{
		OutRootObject.Reset();
		OutErrorMessage.Reset();

		FString JsonString;
		if (!FFileHelper::LoadFileToString(JsonString, *JsonPath))
		{
			OutErrorMessage = FString::Printf(TEXT("failed to read json file: %s"), *JsonPath);
			return false;
		}

		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		if (!FJsonSerializer::Deserialize(Reader, OutRootObject) || !OutRootObject.IsValid())
		{
			OutErrorMessage = FString::Printf(TEXT("failed to parse json file: %s"), *JsonPath);
			return false;
		}

		return true;
	}

	// Reads a centimeter vector object with x, y, and z number fields.
	bool TryReadReplayPointCloudVectorCm(
		const FJsonObject& JsonObject,
		const FString& FieldName,
		FVector& OutVector)
	{
		TSharedPtr<FJsonObject> VectorObject;
		if (!TryGetReplayJsonObjectField(JsonObject, FieldName, VectorObject))
		{
			return false;
		}

		double X = 0.0;
		double Y = 0.0;
		double Z = 0.0;
		if (!VectorObject->TryGetNumberField(TEXT("x"), X)
			|| !VectorObject->TryGetNumberField(TEXT("y"), Y)
			|| !VectorObject->TryGetNumberField(TEXT("z"), Z)
			|| !FMath::IsFinite(X)
			|| !FMath::IsFinite(Y)
			|| !FMath::IsFinite(Z))
		{
			return false;
		}

		OutVector = FVector(X, Y, Z);
		return true;
	}

	// Reads point cloud coordinate metadata from manifest.json or capture_summary.json.
	bool TryReadReplayPointCloudMetadata(
		const FString& MetadataPath,
		FReplayPointCloudImportInfo& InOutInfo,
		FString& OutErrorMessage)
	{
		OutErrorMessage.Reset();

		TSharedPtr<FJsonObject> RootObject;
		if (!LoadReplayJsonObject(MetadataPath, RootObject, OutErrorMessage))
		{
			return false;
		}

		FString PointUnit;
		if (!RootObject->TryGetStringField(TEXT("pointUnit"), PointUnit)
			|| !PointUnit.Equals(TEXT("centimeter"), ESearchCase::IgnoreCase))
		{
			OutErrorMessage = FString::Printf(
				TEXT("pointUnit must be centimeter: %s"),
				*MetadataPath);
			return false;
		}

		FString PointFormat;
		if (RootObject->TryGetStringField(TEXT("pointFormat"), PointFormat)
			&& !PointFormat.Equals(TEXT("xyzrgb_ascii"), ESearchCase::IgnoreCase))
		{
			OutErrorMessage = FString::Printf(
				TEXT("pointFormat must be xyzrgb_ascii: %s"),
				*MetadataPath);
			return false;
		}

		FVector CaptureOriginCm = FVector::ZeroVector;
		if (!TryReadReplayPointCloudVectorCm(*RootObject, TEXT("captureOriginCm"), CaptureOriginCm))
		{
			OutErrorMessage = FString::Printf(
				TEXT("captureOriginCm is missing or invalid: %s"),
				*MetadataPath);
			return false;
		}

		double ImportYAxisSign = 0.0;
		if (!RootObject->TryGetNumberField(TEXT("importYAxisSign"), ImportYAxisSign)
			|| !FMath::IsFinite(ImportYAxisSign)
			|| !FMath::IsNearlyEqual(FMath::Abs(ImportYAxisSign), 1.0, 0.001))
		{
			OutErrorMessage = FString::Printf(
				TEXT("importYAxisSign must be -1 or 1: %s"),
				*MetadataPath);
			return false;
		}

		InOutInfo.MetadataFilePath = MetadataPath;
		InOutInfo.CaptureOriginCm = CaptureOriginCm;
		InOutInfo.ImportYAxisSign = static_cast<float>(ImportYAxisSign);
		return true;
	}

	// Resolves the optional episode point cloud file and its coordinate metadata.
	bool TryResolveReplayPointCloudImportInfo(
		const FString& EpisodeDirectory,
		FReplayPointCloudImportInfo& OutImportInfo,
		TArray<FString>& OutDiagnostics)
	{
		OutImportInfo = FReplayPointCloudImportInfo{};

		FString PointCloudDirectory = FPaths::Combine(
			EpisodeDirectory,
			ReplayPointCloudDirectoryName);
		FPaths::NormalizeDirectoryName(PointCloudDirectory);

		FString XyzFilePath = FPaths::Combine(
			PointCloudDirectory,
			ReplayPointCloudMapFileName);
		FPaths::NormalizeFilename(XyzFilePath);

		if (!IFileManager::Get().FileExists(*XyzFilePath))
		{
			OutDiagnostics.Add(FString::Printf(
				TEXT("Replay point cloud file is missing; point cloud layer will be disabled: %s"),
				*XyzFilePath));
			return false;
		}

		TArray<FString> MetadataPaths;
		const FString ManifestPath = FPaths::Combine(
			PointCloudDirectory,
			ReplayPointCloudManifestFileName);
		if (IFileManager::Get().FileExists(*ManifestPath))
		{
			MetadataPaths.Add(ManifestPath);
		}

		const FString CaptureSummaryPath = FPaths::Combine(
			PointCloudDirectory,
			ReplayPointCloudCaptureSummaryFileName);
		if (IFileManager::Get().FileExists(*CaptureSummaryPath))
		{
			MetadataPaths.Add(CaptureSummaryPath);
		}

		if (MetadataPaths.IsEmpty())
		{
			OutDiagnostics.Add(FString::Printf(
				TEXT("Replay point cloud metadata is missing; point cloud layer will be disabled: %s"),
				*PointCloudDirectory));
			return false;
		}

		for (FString MetadataPath : MetadataPaths)
		{
			FPaths::NormalizeFilename(MetadataPath);

			FReplayPointCloudImportInfo CandidateInfo;
			CandidateInfo.XyzFilePath = XyzFilePath;

			FString ErrorMessage;
			if (TryReadReplayPointCloudMetadata(MetadataPath, CandidateInfo, ErrorMessage))
			{
				CandidateInfo.bAvailable = true;
				OutImportInfo = MoveTemp(CandidateInfo);
				return true;
			}

			OutDiagnostics.Add(FString::Printf(
				TEXT("Replay point cloud metadata is invalid; trying next metadata file if available: %s"),
				*ErrorMessage));
		}

		return false;
	}

	// Appends schema diagnostics to replay UI diagnostics.
	void AppendReplaySchemaDiagnostics(
		const TArray<FScenarioSchemaDiagnostic>& SourceDiagnostics,
		TArray<FString>& OutDiagnostics)
	{
		for (const FScenarioSchemaDiagnostic& Diagnostic : SourceDiagnostics)
		{
			OutDiagnostics.Add(Diagnostic.Message);
		}
	}

	// Appends compile diagnostics to replay UI diagnostics.
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

	FString ManifestPath;
	if (!TryResolveEpisodeReplayManifestPath(LoadedEpisodeDirectory, ManifestPath))
	{
		OutDiagnostics.Add(FString::Printf(TEXT("Replay manifest does not exist: %s"), *ManifestPath));
		PlaybackState = EScenarioReplayPlaybackState::Failed;
		return false;
	}

	if (!FEpisodeReplayManifestJson::LoadFromFile(ManifestPath, Manifest, OutDiagnostics))
	{
		PlaybackState = EScenarioReplayPlaybackState::Failed;
		return false;
	}

	const FString FramePath = ResolveEpisodeReplayFramePath(ManifestPath, Manifest.FrameFile);
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

	LoadEpisodePointCloudWorld(LoadedEpisodeDirectory, OutDiagnostics);
	LoadEpisodeLidarRayReplay(LoadedEpisodeDirectory, OutDiagnostics);

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
	if (!HasLoadedReplayFrames())
	{
		return false;
	}

	return PlaybackState == EScenarioReplayPlaybackState::Playing
		|| PlaybackState == EScenarioReplayPlaybackState::Ready
		|| (bAllowCameraInputWhilePaused
			&& PlaybackState == EScenarioReplayPlaybackState::Paused);
}

// Replay scenario map actors의 표시 상태를 바꾸고 capture 목록을 갱신한다.
void UScenarioReplaySubsystem::SetReplayMapVisible(const bool bVisible)
{
	bReplayMapVisible = bVisible;

	// Keep actor hidden state and SceneCapture show-only state aligned.
	for (const TObjectPtr<AActor>& ReplayScenarioActor : ReplayScenarioActors)
	{
		if (AActor* Actor = ReplayScenarioActor.Get())
		{
			Actor->SetActorHiddenInGame(!bVisible);
		}
	}

	RefreshReplayCaptureShowOnlyActors();
	CaptureReplayScene();

	UE_LOG(
		LogScenarioReplay,
		Log,
		TEXT("Replay map visibility changed | Visible=%s"),
		bReplayMapVisible ? TEXT("true") : TEXT("false"));
}

// Replay point cloud actor의 표시 상태를 바꾸고 capture 목록을 갱신한다.
void UScenarioReplaySubsystem::SetReplayPointCloudVisible(const bool bVisible)
{
	bReplayPointCloudVisible = bVisible;

	// Point cloud can be unavailable for episodes that have no lidar_point_cloud artifact.
	if (IsValid(ReplayPointCloudActor))
	{
		ReplayPointCloudActor->SetPointCloudVisible(bVisible);
	}

	RefreshReplayCaptureShowOnlyActors();
	CaptureReplayScene();

	UE_LOG(
		LogScenarioReplay,
		Log,
		TEXT("Replay point cloud visibility changed | Visible=%s Available=%s"),
		bReplayPointCloudVisible ? TEXT("true") : TEXT("false"),
		IsValid(ReplayPointCloudActor) ? TEXT("true") : TEXT("false"));
}

void UScenarioReplaySubsystem::SetReplayLidarRaysVisible(const bool bVisible)
{
	bReplayLidarRaysVisible = bVisible;

	if (bVisible && HasReplayLidarRays() && !IsValid(ReplayLidarRayActor))
	{
		TArray<FString> SpawnDiagnostics;
		SpawnReplayLidarRayActor(SpawnDiagnostics);
		for (const FString& Diagnostic : SpawnDiagnostics)
		{
			UE_LOG(LogScenarioReplay, Warning, TEXT("%s"), *Diagnostic);
		}
	}

	if (IsValid(ReplayLidarRayActor))
	{
		ReplayLidarRayActor->SetLidarRaysVisible(bVisible);
		if (bVisible)
		{
			RefreshReplayLidarRayActor();
		}
	}

	RefreshReplayCaptureShowOnlyActors();
	CaptureReplayScene();

	UE_LOG(
		LogScenarioReplay,
		Log,
		TEXT("Replay LiDAR ray visibility changed | Visible=%s Available=%s"),
		bReplayLidarRaysVisible ? TEXT("true") : TEXT("false"),
		IsValid(ReplayLidarRayActor) ? TEXT("true") : TEXT("false"));
}

// 현재 replay가 point cloud actor를 가지고 있는지 반환한다.
bool UScenarioReplaySubsystem::HasReplayPointCloud() const
{
	return IsValid(ReplayPointCloudActor);
}

bool UScenarioReplaySubsystem::HasReplayLidarRays() const
{
	return !LidarRayFrames.IsEmpty();
}

int32 UScenarioReplaySubsystem::GetLidarRayFrameCount() const
{
	return LidarRayFrames.Num();
}

int32 UScenarioReplaySubsystem::GetCurrentLidarRayFrameIndex() const
{
	return CurrentLidarRayFrameIndex;
}

int32 UScenarioReplaySubsystem::GetCurrentLidarRayCount() const
{
	const FEpisodeLidarRayFrame* CurrentLidarRayFrame = GetCurrentLidarRayFrame();
	return CurrentLidarRayFrame != nullptr
		? CurrentLidarRayFrame->Rays.Num()
		: 0;
}

const FEpisodeLidarRayFrame* UScenarioReplaySubsystem::GetCurrentLidarRayFrame() const
{
	return LidarRayFrames.IsValidIndex(CurrentLidarRayFrameIndex)
		? &LidarRayFrames[CurrentLidarRayFrameIndex]
		: nullptr;
}

bool UScenarioReplaySubsystem::HasLoadedReplayFrames() const
{
	return !Frames.IsEmpty()
		&& IsValid(ReplayRobotActor)
		&& IsValid(ReplayCaptureActor)
		&& IsValid(ReplayRenderTarget);
}

bool UScenarioReplaySubsystem::FocusFreeCameraOnReplayRobot()
{
	if (!HasLoadedReplayFrames())
	{
		return false;
	}

	CameraMode = EScenarioReplayCameraMode::Free;

	const FTransform RobotTransform = ReplayRobotActor->GetActorTransform();
	const FVector TargetLocation =
		RobotTransform.GetLocation()
		+ FVector::UpVector * FreeCameraFocusTargetHeightCm;
	const FVector CameraLocation =
		TargetLocation
		- RobotTransform.GetUnitAxis(EAxis::X) * FreeCameraFocusBackDistanceCm
		+ RobotTransform.GetUnitAxis(EAxis::Y) * FreeCameraFocusSideOffsetCm
		+ FVector::UpVector * FreeCameraFocusHeightCm;

	FreeCameraLocation = CameraLocation;
	FreeCameraRotation = (TargetLocation - FreeCameraLocation).Rotation();
	FreeCameraRotation.Roll = 0.0;

	UpdateFreeReplayCamera();
	CaptureReplayScene();
	return true;
}

void UScenarioReplaySubsystem::SetReplayCameraMode(EScenarioReplayCameraMode NewMode)
{
	if (!HasLoadedReplayFrames())
	{
		return;
	}

	if (CameraMode == NewMode)
	{
		return;
	}

	CameraMode = NewMode;
	if (CameraMode == EScenarioReplayCameraMode::Orbit)
	{
		OrbitCameraYawDegrees = ReplayRobotActor->GetActorRotation().Yaw;
		ApplyFrameAtTime(CurrentReplayTimeSeconds);
		return;
	}
	if (CameraMode == EScenarioReplayCameraMode::Free)
	{
		FocusFreeCameraOnReplayRobot();
		return;
	}

	ApplyFrameAtTime(CurrentReplayTimeSeconds);
}

void UScenarioReplaySubsystem::AddFreeCameraMovement(
	const FVector& LocalInput,
	float DeltaSeconds)
{
	if (CameraMode != EScenarioReplayCameraMode::Free
		|| !IsReplayCameraInputAllowed()
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
		|| !IsReplayCameraInputAllowed()
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

void UScenarioReplaySubsystem::AddOrbitCameraLook(const FVector2D& MouseDelta)
{
	if (CameraMode != EScenarioReplayCameraMode::Orbit
		|| !IsReplayCameraInputAllowed()
		|| MouseDelta.IsNearlyZero())
	{
		return;
	}

	OrbitCameraYawDegrees += MouseDelta.X * OrbitCameraLookSensitivity;
	OrbitCameraPitchDegrees = FMath::Clamp(
		OrbitCameraPitchDegrees - MouseDelta.Y * OrbitCameraLookSensitivity,
		MinOrbitCameraPitchDegrees,
		MaxOrbitCameraPitchDegrees);

	ApplyFrameAtTime(CurrentReplayTimeSeconds);
}

void UScenarioReplaySubsystem::AddOrbitCameraZoom(float ZoomDirection)
{
	if (CameraMode != EScenarioReplayCameraMode::Orbit
		|| !IsReplayCameraInputAllowed()
		|| FMath::IsNearlyZero(ZoomDirection))
	{
		return;
	}

	OrbitCameraDistanceCm = FMath::Clamp(
		OrbitCameraDistanceCm
			- static_cast<double>(ZoomDirection) * OrbitCameraZoomStepCm,
		MinOrbitCameraDistanceCm,
		MaxOrbitCameraDistanceCm);

	ApplyFrameAtTime(CurrentReplayTimeSeconds);
}

void UScenarioReplaySubsystem::AddTopDownZoom(float ZoomDirection)
{
	if (CameraMode != EScenarioReplayCameraMode::TopDown
		|| !IsReplayCameraInputAllowed()
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
	FreeCameraFocusBackDistanceCm =
		FMath::Max(0.0, Settings.FreeCameraFocusBackDistanceCm);
	FreeCameraFocusSideOffsetCm = Settings.FreeCameraFocusSideOffsetCm;
	FreeCameraFocusHeightCm =
		FMath::Max(0.0, Settings.FreeCameraFocusHeightCm);
	FreeCameraFocusTargetHeightCm =
		FMath::Max(0.0, Settings.FreeCameraFocusTargetHeightCm);

	OrbitCameraFovDegrees = FMath::Clamp(
		Settings.OrbitCameraFovDegrees,
		5.0,
		170.0);
	const double OrderedMinOrbitDistance =
		FMath::Max(1.0, FMath::Min(
			Settings.MinOrbitCameraDistanceCm,
			Settings.MaxOrbitCameraDistanceCm));
	const double OrderedMaxOrbitDistance =
		FMath::Max(OrderedMinOrbitDistance, FMath::Max(
			Settings.MinOrbitCameraDistanceCm,
			Settings.MaxOrbitCameraDistanceCm));
	MinOrbitCameraDistanceCm = OrderedMinOrbitDistance;
	MaxOrbitCameraDistanceCm = OrderedMaxOrbitDistance;
	OrbitCameraDistanceCm = FMath::Clamp(
		Settings.OrbitCameraDistanceCm,
		MinOrbitCameraDistanceCm,
		MaxOrbitCameraDistanceCm);
	OrbitCameraZoomStepCm = FMath::Max(1.0, Settings.OrbitCameraZoomStepCm);

	const double OrderedMinOrbitPitch = FMath::Clamp(
		FMath::Min(
			Settings.MinOrbitCameraPitchDegrees,
			Settings.MaxOrbitCameraPitchDegrees),
		-89.0,
		89.0);
	const double OrderedMaxOrbitPitch = FMath::Clamp(
		FMath::Max(
			Settings.MinOrbitCameraPitchDegrees,
			Settings.MaxOrbitCameraPitchDegrees),
		OrderedMinOrbitPitch,
		89.0);
	MinOrbitCameraPitchDegrees = OrderedMinOrbitPitch;
	MaxOrbitCameraPitchDegrees = OrderedMaxOrbitPitch;
	OrbitCameraPitchDegrees = FMath::Clamp(
		Settings.OrbitCameraPitchDegrees,
		MinOrbitCameraPitchDegrees,
		MaxOrbitCameraPitchDegrees);
	OrbitCameraLookSensitivity =
		FMath::Max(0.001, Settings.OrbitCameraLookSensitivity);
	OrbitCameraTargetHeightCm =
		FMath::Max(0.0, Settings.OrbitCameraTargetHeightCm);

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

	if (IsValid(ReplayPointCloudActor))
	{
		ReplayPointCloudActor->Destroy();
	}
	ReplayPointCloudActor = nullptr;

	if (IsValid(ReplayLidarRayActor))
	{
		ReplayLidarRayActor->Destroy();
	}
	ReplayLidarRayActor = nullptr;

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
	LidarRayFrames.Reset();
	LidarRayManifest = FEpisodeLidarRayReplayManifest{};
	LoadedEpisodeDirectory.Reset();
	CurrentReplayTimeSeconds = 0.0;
	CurrentFrameIndex = INDEX_NONE;
	CurrentLidarRayFrameIndex = INDEX_NONE;
	CurrentRobotSpeedKmh = 0.0;
	CurrentRobotPositionCm = FVector::ZeroVector;
	PlaybackState = EScenarioReplayPlaybackState::Stopped;
	CameraMode = EScenarioReplayCameraMode::TopDown;
	bReplayMapVisible = true;
	bReplayPointCloudVisible = true;
	bReplayLidarRaysVisible = false;
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

// Loads optional point cloud artifacts into the replay-only world layer.
bool UScenarioReplaySubsystem::LoadEpisodePointCloudWorld(
	const FString& EpisodeDirectory,
	TArray<FString>& OutDiagnostics)
{
	FReplayPointCloudImportInfo ImportInfo;
	if (!TryResolveReplayPointCloudImportInfo(EpisodeDirectory, ImportInfo, OutDiagnostics))
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		OutDiagnostics.Add(TEXT("Replay point cloud requires a valid world."));
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ADeliveryBotPointCloudReviewActor* PointCloudActor =
		World->SpawnActor<ADeliveryBotPointCloudReviewActor>(
			ADeliveryBotPointCloudReviewActor::StaticClass(),
			ReplayWorldOffset,
			FRotator::ZeroRotator,
			SpawnParameters);
	if (!IsValid(PointCloudActor))
	{
		OutDiagnostics.Add(TEXT("Failed to spawn replay point cloud actor."));
		return false;
	}

	PointCloudActor->Tags.AddUnique(FName(TEXT("ReplayOnly")));
	PointCloudActor->SetPointCloudVisible(bReplayPointCloudVisible);

	if (!PointCloudActor->LoadReplayMapPointCloudFromFile(
		ImportInfo.XyzFilePath,
		ImportInfo.CaptureOriginCm,
		ImportInfo.ImportYAxisSign))
	{
		OutDiagnostics.Add(FString::Printf(
			TEXT("Failed to load replay point cloud file; point cloud layer will be disabled: %s"),
			*ImportInfo.XyzFilePath));
		PointCloudActor->Destroy();
		return false;
	}

	ReplayPointCloudActor = PointCloudActor;
	RefreshReplayPointCloudRenderMode();
	RefreshReplayCaptureShowOnlyActors();

	UE_LOG(
		LogScenarioReplay,
		Log,
		TEXT("Replay point cloud loaded | File=%s Metadata=%s Points=%d Origin=(%.3f, %.3f, %.3f) YSign=%.1f"),
		*ImportInfo.XyzFilePath,
		*ImportInfo.MetadataFilePath,
		PointCloudActor->GetLoadedPointCount(),
		ImportInfo.CaptureOriginCm.X,
		ImportInfo.CaptureOriginCm.Y,
		ImportInfo.CaptureOriginCm.Z,
		ImportInfo.ImportYAxisSign);

	return true;
}

bool UScenarioReplaySubsystem::LoadEpisodeLidarRayReplay(
	const FString& EpisodeDirectory,
	TArray<FString>& OutDiagnostics)
{
	LidarRayManifest = FEpisodeLidarRayReplayManifest{};
	LidarRayFrames.Reset();
	CurrentLidarRayFrameIndex = INDEX_NONE;

	FString LidarRayDirectory = FPaths::Combine(
		EpisodeDirectory,
		ReplayArtifactDirectoryName,
		ReplayLidarRayDirectoryName);
	FPaths::NormalizeDirectoryName(LidarRayDirectory);

	FString ManifestPath = FPaths::Combine(
		LidarRayDirectory,
		ReplayLidarRayManifestFileName);
	FPaths::NormalizeFilename(ManifestPath);
	if (!IFileManager::Get().FileExists(*ManifestPath))
	{
		AppendReplayLidarRayLoadDiagnostic(
			FString::Printf(TEXT("manifest is missing: %s"), *ManifestPath),
			OutDiagnostics);
		return false;
	}

	FEpisodeLidarRayReplayManifest LoadedManifest;
	TArray<FString> LidarRayDiagnostics;
	if (!FEpisodeLidarRayReplayManifestJson::LoadFromFile(
		ManifestPath,
		LoadedManifest,
		LidarRayDiagnostics))
	{
		AppendReplayLidarRayLoadDiagnostics(LidarRayDiagnostics, OutDiagnostics);
		return false;
	}

	const FString FramePath = ResolveEpisodeReplayFramePath(ManifestPath, LoadedManifest.FrameFile);
	TArray<FEpisodeLidarRayFrame> LoadedFrames;
	if (!FEpisodeLidarRayReplayBinary::LoadFramesFromFile(
		FramePath,
		LoadedFrames,
		LidarRayDiagnostics))
	{
		AppendReplayLidarRayLoadDiagnostics(LidarRayDiagnostics, OutDiagnostics);
		return false;
	}

	if (LoadedFrames.Num() != LoadedManifest.FrameCount)
	{
		AppendReplayLidarRayLoadDiagnostic(
			TEXT("manifest frame count does not match binary frame count."),
			OutDiagnostics);
		return false;
	}

	const int32 LoadedRayCount = CountReplayLidarRaySamples(LoadedFrames);
	if (LoadedRayCount != LoadedManifest.TotalRayCount)
	{
		AppendReplayLidarRayLoadDiagnostic(
			TEXT("manifest total ray count does not match binary ray count."),
			OutDiagnostics);
		return false;
	}

	if (!AreReplayLidarRayFramesSorted(LoadedFrames))
	{
		AppendReplayLidarRayLoadDiagnostic(
			TEXT("frame times are not sorted."),
			OutDiagnostics);
		return false;
	}

	LidarRayManifest = MoveTemp(LoadedManifest);
	LidarRayFrames = MoveTemp(LoadedFrames);
	CurrentLidarRayFrameIndex = FEpisodeLidarRayReplayBinary::ResolveFrameIndex(
		0.0,
		LidarRayFrames);

	UE_LOG(
		LogScenarioReplay,
		Log,
		TEXT("Replay LiDAR ray layer loaded | Frames=%d Rays=%d FirstSequence=%d LastSequence=%d"),
		LidarRayFrames.Num(),
		LoadedRayCount,
		LidarRayManifest.FirstSensorSequence,
		LidarRayManifest.LastSensorSequence);
	return HasReplayLidarRays();
}

bool UScenarioReplaySubsystem::SpawnReplayLidarRayActor(TArray<FString>& OutDiagnostics)
{
	if (IsValid(ReplayLidarRayActor))
	{
		return true;
	}

	if (!HasReplayLidarRays())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		OutDiagnostics.Add(TEXT("Replay LiDAR ray actor requires a valid world."));
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ADeliveryBotLidarRayReviewActor* LidarRayActor =
		World->SpawnActor<ADeliveryBotLidarRayReviewActor>(
			ADeliveryBotLidarRayReviewActor::StaticClass(),
			ReplayWorldOffset,
			FRotator::ZeroRotator,
			SpawnParameters);
	if (!IsValid(LidarRayActor))
	{
		OutDiagnostics.Add(TEXT("Failed to spawn replay LiDAR ray actor; LiDAR ray layer will be disabled."));
		return false;
	}

	LidarRayActor->Tags.AddUnique(FName(TEXT("ReplayOnly")));
	LidarRayActor->SetLidarRaysVisible(bReplayLidarRaysVisible);
	ReplayLidarRayActor = LidarRayActor;
	RefreshReplayLidarRayActor();
	RefreshReplayCaptureShowOnlyActors();
	return true;
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
	Actor->SetActorHiddenInGame(!bReplayMapVisible);

	RefreshReplayCaptureShowOnlyActors();
}

// 현재 replay 레이어 상태를 기준으로 한 SceneCapture show-only 목록을 채운다.
void UScenarioReplaySubsystem::PopulateReplayCaptureShowOnlyActors(
	USceneCaptureComponent2D& CaptureComponent) const
{
	CaptureComponent.ShowOnlyActors.Reset();

	if (IsValid(ReplayRobotActor))
	{
		CaptureComponent.ShowOnlyActors.Add(ReplayRobotActor);
		if (AActor* ReplayVisualActor = ReplayRobotActor->GetReplayVisualActor())
		{
			CaptureComponent.ShowOnlyActors.Add(ReplayVisualActor);
		}
	}

	if (bReplayMapVisible)
	{
		for (const TObjectPtr<AActor>& ReplayScenarioActor : ReplayScenarioActors)
		{
			if (AActor* Actor = ReplayScenarioActor.Get())
			{
				CaptureComponent.ShowOnlyActors.Add(Actor);
			}
		}
	}

	if (bReplayPointCloudVisible && IsValid(ReplayPointCloudActor))
	{
		CaptureComponent.ShowOnlyActors.Add(ReplayPointCloudActor);
	}

	if (bReplayLidarRaysVisible && IsValid(ReplayLidarRayActor))
	{
		CaptureComponent.ShowOnlyActors.Add(ReplayLidarRayActor);
	}
}

// 현재 replay capture component의 show-only 목록을 다시 만든다.
void UScenarioReplaySubsystem::RefreshReplayCaptureShowOnlyActors()
{
	if (USceneCaptureComponent2D* CaptureComponent = GetReplayCaptureComponent())
	{
		PopulateReplayCaptureShowOnlyActors(*CaptureComponent);
	}
}

// Updates the point cloud renderer for the active replay camera mode.
void UScenarioReplaySubsystem::RefreshReplayPointCloudRenderMode()
{
	if (!IsValid(ReplayPointCloudActor))
	{
		return;
	}

	const EDeliveryBotPointCloudReviewRenderMode RenderMode =
		CameraMode == EScenarioReplayCameraMode::TopDown
			? EDeliveryBotPointCloudReviewRenderMode::TopDownProjection
			: EDeliveryBotPointCloudReviewRenderMode::Plugin3D;
	ReplayPointCloudActor->SetReviewRenderMode(RenderMode);
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
	ApplyLidarRayFrameAtTime(TimeSeconds);

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
	if (LowerFrame.Wheels.Num() == EpisodeReplayV2::WheelCount
		&& UpperFrame.Wheels.Num() == EpisodeReplayV2::WheelCount)
	{
		OutFrame.Wheels.SetNum(EpisodeReplayV2::WheelCount);
		for (int32 WheelIndex = 0; WheelIndex < EpisodeReplayV2::WheelCount; ++WheelIndex)
		{
			const FEpisodeReplayWheelFrame& LowerWheelFrame = LowerFrame.Wheels[WheelIndex];
			const FEpisodeReplayWheelFrame& UpperWheelFrame = UpperFrame.Wheels[WheelIndex];
			FEpisodeReplayWheelFrame& OutWheelFrame = OutFrame.Wheels[WheelIndex];
			OutWheelFrame.LocalLocationCm = FMath::Lerp(
				LowerWheelFrame.LocalLocationCm,
				UpperWheelFrame.LocalLocationCm,
				Alpha);
			OutWheelFrame.LocalRotation = FQuat::Slerp(
				LowerWheelFrame.LocalRotation,
				UpperWheelFrame.LocalRotation,
				Alpha).GetNormalized();
			OutWheelFrame.bInContact = Alpha < 0.5
				? LowerWheelFrame.bInContact
				: UpperWheelFrame.bInContact;
			OutWheelFrame.bHasVisualPose =
				LowerWheelFrame.bHasVisualPose
				&& UpperWheelFrame.bHasVisualPose;
		}
	}
	OutFrameIndex = FMath::Clamp(
		FMath::RoundToInt(FramePosition),
		0,
		Frames.Num() - 1);
	return true;
}

void UScenarioReplaySubsystem::ApplyLidarRayFrameAtTime(double TimeSeconds)
{
	CurrentLidarRayFrameIndex = FEpisodeLidarRayReplayBinary::ResolveFrameIndex(
		TimeSeconds,
		LidarRayFrames);
	RefreshReplayLidarRayActor();
}

void UScenarioReplaySubsystem::RefreshReplayLidarRayActor()
{
	if (!IsValid(ReplayLidarRayActor))
	{
		return;
	}

	ReplayLidarRayActor->ApplyLidarRayFrame(
		GetCurrentLidarRayFrame(),
		LidarRayManifest,
		ReplayWorldOffset);
}

void UScenarioReplaySubsystem::UpdateReplayCaptureView(
	const FEpisodeReplayRobotFrame& Frame)
{
	RefreshReplayPointCloudRenderMode();

	switch (CameraMode)
	{
	case EScenarioReplayCameraMode::Orbit:
		UpdateOrbitReplayCamera(Frame);
		break;

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

void UScenarioReplaySubsystem::UpdateOrbitReplayCamera(
	const FEpisodeReplayRobotFrame& Frame)
{
	USceneCaptureComponent2D* CaptureComponent = GetReplayCaptureComponent();
	if (!IsValid(ReplayCaptureActor) || !IsValid(CaptureComponent))
	{
		return;
	}

	const FVector TargetLocation =
		Frame.PositionCm
		+ ReplayWorldOffset
		+ FVector::UpVector * OrbitCameraTargetHeightCm;
	const FRotator OrbitRotation(
		OrbitCameraPitchDegrees,
		OrbitCameraYawDegrees,
		0.0);
	const FVector CameraLocation =
		TargetLocation
		- OrbitRotation.Vector() * OrbitCameraDistanceCm;

	CaptureComponent->ProjectionType = ECameraProjectionMode::Perspective;
	CaptureComponent->FOVAngle = static_cast<float>(OrbitCameraFovDegrees);
	ReplayCaptureActor->SetActorLocation(CameraLocation);
	ReplayCaptureActor->SetActorRotation((TargetLocation - CameraLocation).Rotation());
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
	RenderTarget->ClearColor = FLinearColor(0.026042f, 0.026042f, 0.026042f, 1.0f);
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
	CaptureComponent->ShowFlags.SetPostProcessing(true);
	CaptureComponent->ShowFlags.SetPostProcessMaterial(true);
	CaptureComponent->ShowFlags.SetAtmosphere(false);
	CaptureComponent->ShowFlags.SetFog(false);
	CaptureComponent->ShowFlags.SetCloud(false);
	CaptureComponent->ShowFlags.SetSkyLighting(false);
	PopulateReplayCaptureShowOnlyActors(*CaptureComponent);
	CaptureComponent->CaptureSource = SCS_FinalColorLDR;
	CaptureComponent->TextureTarget = ReplayRenderTarget;
	CaptureComponent->bCaptureEveryFrame = false;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->bAlwaysPersistRenderingState = false;
	CaptureComponent->bExcludeFromSceneTextureExtents = true;
	CaptureComponent->bUseRayTracingIfEnabled = false;
	if (!FScenarioViewportPresentation::ApplyGreyBackgroundPostProcess(CaptureComponent, 1.0f))
	{
		UE_LOG(
			LogScenarioReplay,
			Warning,
			TEXT("Failed to apply replay grey background post-process material."));
	}
	return CaptureActor;
}

USceneCaptureComponent2D* UScenarioReplaySubsystem::GetReplayCaptureComponent() const
{
	return IsValid(ReplayCaptureActor)
		? ReplayCaptureActor->GetCaptureComponent2D()
		: nullptr;
}
