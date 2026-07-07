#include "Scenario/Replay/ScenarioReplaySubsystem.h"

#include "DeliveryBot/Actor/DeliveryBotLidarRayReviewActor.h"
#include "DeliveryBot/Actor/DeliveryBotPointCloudReviewActor.h"
#include "DeliveryBot/DeliveryBotSetupCompiler.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Scenario/Actors/ScenarioCorridorRuntimeActor.h"
#include "Scenario/Actors/ScenarioGroundRegion.h"
#include "Scenario/Actors/ScenarioStaticObstacle.h"
#include "Scenario/Data/ScenarioStaticObstaclePropCatalog.h"
#include "Scenario/ScenarioCityBlockMaterializer.h"
#include "Scenario/ScenarioSampleWorldSpecAdapter.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Scenario/Replay/ScenarioReplayDeveloperSettings.h"
#include "Scenario/Replay/DeliveryBotReplayActor.h"
#include "Scenario/Replay/ScenarioReplayRouteMarkerActor.h"
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
	const TCHAR* ReplayPointCloudFramesIndexFileName = TEXT("frames.jsonl");
	const TCHAR* ReplayPointCloudFramesDirectoryName = TEXT("frames");
	const TCHAR* ReplayLidarRayDirectoryName = TEXT("lidar_rays");
	const TCHAR* ReplayLidarRayManifestFileName = TEXT("rays.meta.json");
	const TCHAR* ReplayProfileFileName = TEXT("profile.json");
	const TCHAR* ReplayEventsFileName = TEXT("events.jsonl");
	const float ReplayPointCloudHighlightPointSizeCm = 24.0f;
	const float ReplayPointCloudHighlightSphereSizeCm = 3.0f;
	const float ReplayPointCloudHighlightSphereZOffsetCm = 8.0f;
	const float ReplayPointCloudHighlightColorBrightness = 10.f;

	// Allows terminal events recorded just after the last replay sample to remain visible.
	double GetReplayEventMarkerEndToleranceSeconds(const FEpisodeReplayManifest& Manifest)
	{
		const double SampleIntervalSeconds = Manifest.SampleRateHz > KINDA_SMALL_NUMBER
			? 1.0 / Manifest.SampleRateHz
			: 0.0;
		return FMath::Max(2.0, SampleIntervalSeconds * 2.0);
	}

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

	// Normalizes a profile path candidate before file existence checks.
	FString NormalizeReplayProfileCandidate(const FString& ProfilePath)
	{
		FString NormalizedPath = ProfilePath;
		FPaths::NormalizeFilename(NormalizedPath);
		FPaths::CollapseRelativeDirectories(NormalizedPath);
		return NormalizedPath;
	}

	// Resolves the profile snapshot that supplied the replay robot LiDAR settings.
	bool TryResolveEpisodeReplayProfilePath(
		const FString& EpisodeDirectory,
		FString& OutProfilePath)
	{
		const TArray<FString> CandidatePaths = {
			FPaths::Combine(EpisodeDirectory, ReplayProfileFileName),
			FPaths::Combine(EpisodeDirectory, TEXT(".."), ReplayProfileFileName),
			FPaths::Combine(EpisodeDirectory, TEXT(".."), TEXT("snapshot"), ReplayProfileFileName),
			FPaths::Combine(EpisodeDirectory, TEXT(".."), TEXT(".."), TEXT("snapshot"), ReplayProfileFileName),
			FPaths::Combine(EpisodeDirectory, TEXT(".."), TEXT(".."), ReplayProfileFileName)
		};

		for (const FString& CandidatePath : CandidatePaths)
		{
			const FString NormalizedPath = NormalizeReplayProfileCandidate(CandidatePath);
			if (IFileManager::Get().FileExists(*NormalizedPath))
			{
				OutProfilePath = NormalizedPath;
				return true;
			}
		}

		OutProfilePath = NormalizeReplayProfileCandidate(CandidatePaths.Last());
		return false;
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

	// Normalizes a point cloud frame path candidate before file existence checks.
	FString NormalizeReplayPointCloudFramePathCandidate(const FString& CandidatePath)
	{
		FString NormalizedPath = CandidatePath;
		FPaths::NormalizeFilename(NormalizedPath);
		FPaths::CollapseRelativeDirectories(NormalizedPath);
		return NormalizedPath;
	}

	// Adds one point cloud frame path candidate when it is not empty.
	void AddReplayPointCloudFramePathCandidate(
		TArray<FString>& InOutCandidates,
		const FString& CandidatePath)
	{
		FString NormalizedPath = NormalizeReplayPointCloudFramePathCandidate(CandidatePath);
		if (!NormalizedPath.IsEmpty())
		{
			InOutCandidates.AddUnique(NormalizedPath);
		}
	}

	// Resolves a point cloud frame path reference from frames.jsonl into an absolute xyz path.
	bool TryResolveReplayPointCloudFramePath(
		const FString& EpisodeDirectory,
		const FString& PathReference,
		const int32 SensorSequence,
		FString& OutFramePath)
	{
		OutFramePath.Reset();

		FString PointCloudDirectory = FPaths::Combine(
			EpisodeDirectory,
			ReplayPointCloudDirectoryName);
		FPaths::NormalizeDirectoryName(PointCloudDirectory);

		TArray<FString> Candidates;
		FString NormalizedReference = PathReference.TrimStartAndEnd();
		FPaths::NormalizeFilename(NormalizedReference);

		if (!NormalizedReference.IsEmpty())
		{
			if (FPaths::IsRelative(NormalizedReference))
			{
				AddReplayPointCloudFramePathCandidate(
					Candidates,
					FPaths::Combine(EpisodeDirectory, NormalizedReference));
				AddReplayPointCloudFramePathCandidate(
					Candidates,
					FPaths::Combine(PointCloudDirectory, NormalizedReference));

				const int32 PointCloudMarkerIndex =
					NormalizedReference.Find(ReplayPointCloudDirectoryName, ESearchCase::IgnoreCase);
				if (PointCloudMarkerIndex != INDEX_NONE)
				{
					AddReplayPointCloudFramePathCandidate(
						Candidates,
						FPaths::Combine(
							EpisodeDirectory,
							NormalizedReference.Mid(PointCloudMarkerIndex)));
				}
			}
			else
			{
				AddReplayPointCloudFramePathCandidate(Candidates, NormalizedReference);
			}
		}

		if (SensorSequence != INDEX_NONE)
		{
			AddReplayPointCloudFramePathCandidate(
				Candidates,
				FPaths::Combine(
					PointCloudDirectory,
					ReplayPointCloudFramesDirectoryName,
					FString::Printf(TEXT("frame_%06d.xyz"), SensorSequence)));
		}

		for (const FString& CandidatePath : Candidates)
		{
			if (IFileManager::Get().FileExists(*CandidatePath))
			{
				OutFramePath = CandidatePath;
				return true;
			}
		}

		return false;
	}

	// Parses one frames.jsonl object into a replay point cloud frame record.
	bool TryReadReplayPointCloudFrameRecord(
		const FString& EpisodeDirectory,
		const FJsonObject& FrameObject,
		FScenarioReplayPointCloudFrameRecord& OutRecord,
		FString& OutErrorMessage)
	{
		OutRecord = FScenarioReplayPointCloudFrameRecord{};
		OutErrorMessage.Reset();

		double TimeSeconds = 0.0;
		if (!FrameObject.TryGetNumberField(TEXT("runTimeSeconds"), TimeSeconds))
		{
			FrameObject.TryGetNumberField(TEXT("sensorTimeSeconds"), TimeSeconds);
		}
		if (!FMath::IsFinite(TimeSeconds) || TimeSeconds < 0.0)
		{
			OutErrorMessage = TEXT("runTimeSeconds is missing or invalid.");
			return false;
		}

		double SensorSequenceValue = INDEX_NONE;
		FrameObject.TryGetNumberField(TEXT("sensorSequence"), SensorSequenceValue);
		const int32 SensorSequence = FMath::RoundToInt(SensorSequenceValue);

		FString PathReference;
		FrameObject.TryGetStringField(TEXT("path"), PathReference);

		FString FramePath;
		if (!TryResolveReplayPointCloudFramePath(
			EpisodeDirectory,
			PathReference,
			SensorSequence,
			FramePath))
		{
			OutErrorMessage = FString::Printf(
				TEXT("frame xyz path is missing or does not exist: %s"),
				*PathReference);
			return false;
		}

		double PointCountValue = 0.0;
		FrameObject.TryGetNumberField(TEXT("pointCount"), PointCountValue);

		OutRecord.TimeSeconds = TimeSeconds;
		OutRecord.SensorSequence = SensorSequence;
		OutRecord.XyzFilePath = MoveTemp(FramePath);
		OutRecord.PointCount = FMath::Max(0, FMath::RoundToInt(PointCountValue));
		return true;
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

	LoadEpisodeEventMarkers(LoadedEpisodeDirectory, OutDiagnostics);

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
	LoadEpisodeLidarSensorConfig(LoadedEpisodeDirectory, OutDiagnostics);
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
	if (CameraMode == EScenarioReplayCameraMode::Orbit && !Frames.IsEmpty())
	{
		OrbitCameraYawDegrees = Frames[0].Rotation.Rotator().Yaw;
	}
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

	if (IsValid(ReplayRouteMarkerActor))
	{
		ReplayRouteMarkerActor->SetRouteMarkersVisible(bVisible);
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
	if (IsValid(ReplayPointCloudFrameHighlightActor))
	{
		ReplayPointCloudFrameHighlightActor->SetPointCloudVisible(
			bVisible && PointCloudFrameRecords.IsValidIndex(CurrentPointCloudFrameHighlightIndex));
		if (bVisible)
		{
			ApplyPointCloudFrameHighlightAtTime(CurrentReplayTimeSeconds);
		}
	}
	if (IsValid(ReplayPointCloudFrameHighlightBackBufferActor))
	{
		ReplayPointCloudFrameHighlightBackBufferActor->SetPointCloudVisible(false);
	}

	RefreshReplayCaptureShowOnlyActors();
	CaptureReplayScene();

	UE_LOG(
		LogScenarioReplay,
		Log,
		TEXT("Replay point cloud visibility changed | Visible=%s Available=%s"),
		bReplayPointCloudVisible ? TEXT("true") : TEXT("false"),
		IsValid(ReplayPointCloudActor)
			|| IsValid(ReplayPointCloudFrameHighlightActor)
			|| IsValid(ReplayPointCloudFrameHighlightBackBufferActor)
			? TEXT("true")
			: TEXT("false"));
}

void UScenarioReplaySubsystem::SetReplayLidarRaysVisible(const bool bVisible)
{
	bReplayLidarRaysVisible = bVisible;
	const bool bAnyLidarVisible = bReplayLidarRaysVisible || bReplayLidarDistanceVisible;

	if (bAnyLidarVisible && HasReplayLidarRays() && !IsValid(ReplayLidarRayActor))
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
		ReplayLidarRayActor->SetLidarSensorRaysVisible(bReplayLidarRaysVisible);
		ReplayLidarRayActor->SetLidarDistanceOverlayVisible(bReplayLidarDistanceVisible);
		if (bAnyLidarVisible)
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

// Shows or hides replay LiDAR distance overlays and refreshes the replay capture.
void UScenarioReplaySubsystem::SetReplayLidarDistanceVisible(const bool bVisible)
{
	bReplayLidarDistanceVisible = bVisible;
	const bool bAnyLidarVisible = bReplayLidarRaysVisible || bReplayLidarDistanceVisible;

	if (bAnyLidarVisible && HasReplayLidarRays() && !IsValid(ReplayLidarRayActor))
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
		ReplayLidarRayActor->SetLidarSensorRaysVisible(bReplayLidarRaysVisible);
		ReplayLidarRayActor->SetLidarDistanceOverlayVisible(bReplayLidarDistanceVisible);
		if (bAnyLidarVisible)
		{
			RefreshReplayLidarRayActor();
		}
	}

	RefreshReplayCaptureShowOnlyActors();
	CaptureReplayScene();

	UE_LOG(
		LogScenarioReplay,
		Log,
		TEXT("Replay LiDAR distance visibility changed | Visible=%s Available=%s"),
		bReplayLidarDistanceVisible ? TEXT("true") : TEXT("false"),
		IsValid(ReplayLidarRayActor) ? TEXT("true") : TEXT("false"));
}

// 현재 replay가 point cloud actor를 가지고 있는지 반환한다.
bool UScenarioReplaySubsystem::HasReplayPointCloud() const
{
	return IsValid(ReplayPointCloudActor);
}

bool UScenarioReplaySubsystem::HasReplayLidarRays() const
{
	return bHasReplayLidarSensorConfig || !LidarRayFrames.IsEmpty();
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

	if (IsValid(ReplayRouteMarkerActor))
	{
		ReplayRouteMarkerActor->Destroy();
	}
	ReplayRouteMarkerActor = nullptr;

	if (IsValid(ReplayPointCloudActor))
	{
		ReplayPointCloudActor->Destroy();
	}
	ReplayPointCloudActor = nullptr;

	if (IsValid(ReplayPointCloudFrameHighlightActor))
	{
		ReplayPointCloudFrameHighlightActor->Destroy();
	}
	ReplayPointCloudFrameHighlightActor = nullptr;

	if (IsValid(ReplayPointCloudFrameHighlightBackBufferActor))
	{
		ReplayPointCloudFrameHighlightBackBufferActor->Destroy();
	}
	ReplayPointCloudFrameHighlightBackBufferActor = nullptr;

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
	ReplayEventMarkers.Reset();
	PointCloudFrameRecords.Reset();
	ReplayLidarSensorConfig = FDeliveryBotLidarSensorConfigInfo{};
	bHasReplayLidarSensorConfig = false;
	LoadedEpisodeDirectory.Reset();
	CurrentReplayTimeSeconds = 0.0;
	CurrentFrameIndex = INDEX_NONE;
	CurrentLidarRayFrameIndex = INDEX_NONE;
	CurrentPointCloudFrameHighlightIndex = INDEX_NONE;
	BackBufferPointCloudFrameHighlightIndex = INDEX_NONE;
	CurrentRobotSpeedKmh = 0.0;
	CurrentRobotThrottle = 0.0;
	CurrentRobotSteering = 0.0;
	CurrentRobotBrake = 0.0;
	CurrentRobotTargetSpeedKmh = 0.0;
	CurrentRobotPositionCm = FVector::ZeroVector;
	PlaybackState = EScenarioReplayPlaybackState::Stopped;
	CameraMode = EScenarioReplayCameraMode::Orbit;
	bReplayMapVisible = true;
	bReplayPointCloudVisible = true;
	bReplayLidarRaysVisible = false;
	bReplayLidarDistanceVisible = false;
	ReplayPointCloudCaptureOriginCm = FVector::ZeroVector;
	ReplayPointCloudImportYAxisSign = -1.0f;
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
	PointCloudActor->SetReviewPluginRendererEnabled(false);
	PointCloudActor->SetPointCloudVisible(bReplayPointCloudVisible);
	ReplayPointCloudCaptureOriginCm = ImportInfo.CaptureOriginCm;
	ReplayPointCloudImportYAxisSign = ImportInfo.ImportYAxisSign;

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
	LoadEpisodePointCloudFrameIndex(EpisodeDirectory, OutDiagnostics);

	if (!PointCloudFrameRecords.IsEmpty())
	{
		auto SpawnFrameHighlightActor = [&]() -> ADeliveryBotPointCloudReviewActor*
		{
			ADeliveryBotPointCloudReviewActor* HighlightActor =
				World->SpawnActor<ADeliveryBotPointCloudReviewActor>(
					ADeliveryBotPointCloudReviewActor::StaticClass(),
					ReplayWorldOffset,
					FRotator::ZeroRotator,
					SpawnParameters);
			if (!IsValid(HighlightActor))
			{
				return nullptr;
			}

			HighlightActor->Tags.AddUnique(FName(TEXT("ReplayOnly")));
			HighlightActor->ConfigureReviewVisualStyle(
				ReplayPointCloudHighlightPointSizeCm,
				ReplayPointCloudHighlightSphereSizeCm,
				ReplayPointCloudHighlightColorBrightness);
			HighlightActor->SetReviewTopDownSphereZOffset(
				ReplayPointCloudHighlightSphereZOffsetCm);
			HighlightActor->SetReviewPluginRendererEnabled(false);
			HighlightActor->SetPointCloudVisible(false);
			return HighlightActor;
		};

		ADeliveryBotPointCloudReviewActor* ActiveHighlightActor =
			SpawnFrameHighlightActor();
		ADeliveryBotPointCloudReviewActor* BackBufferHighlightActor =
			SpawnFrameHighlightActor();
		if (IsValid(ActiveHighlightActor) && IsValid(BackBufferHighlightActor))
		{
			ReplayPointCloudFrameHighlightActor = ActiveHighlightActor;
			ReplayPointCloudFrameHighlightBackBufferActor = BackBufferHighlightActor;
			CurrentPointCloudFrameHighlightIndex = INDEX_NONE;
			BackBufferPointCloudFrameHighlightIndex = INDEX_NONE;
		}
		else
		{
			if (IsValid(ActiveHighlightActor))
			{
				ActiveHighlightActor->Destroy();
			}
			if (IsValid(BackBufferHighlightActor))
			{
				BackBufferHighlightActor->Destroy();
			}
			OutDiagnostics.Add(TEXT("Failed to spawn replay point cloud frame highlight actors."));
		}
	}

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

// Loads optional per-frame point cloud records used for current-frame highlight overlay.
void UScenarioReplaySubsystem::LoadEpisodePointCloudFrameIndex(
	const FString& EpisodeDirectory,
	TArray<FString>& OutDiagnostics)
{
	PointCloudFrameRecords.Reset();
	CurrentPointCloudFrameHighlightIndex = INDEX_NONE;
	BackBufferPointCloudFrameHighlightIndex = INDEX_NONE;

	FString FrameIndexPath = FPaths::Combine(
		EpisodeDirectory,
		ReplayPointCloudDirectoryName,
		ReplayPointCloudFramesIndexFileName);
	FPaths::NormalizeFilename(FrameIndexPath);
	if (!IFileManager::Get().FileExists(*FrameIndexPath))
	{
		return;
	}

	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *FrameIndexPath))
	{
		OutDiagnostics.Add(FString::Printf(
			TEXT("Replay point cloud frame highlight disabled; failed to read frame index: %s"),
			*FrameIndexPath));
		return;
	}

	int32 InvalidLineCount = 0;
	for (const FString& SourceLine : Lines)
	{
		FString Line = SourceLine;
		Line.TrimStartAndEndInline();
		if (Line.IsEmpty())
		{
			continue;
		}

		TSharedPtr<FJsonObject> FrameObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Line);
		if (!FJsonSerializer::Deserialize(Reader, FrameObject) || !FrameObject.IsValid())
		{
			++InvalidLineCount;
			continue;
		}

		FScenarioReplayPointCloudFrameRecord FrameRecord;
		FString ErrorMessage;
		if (!TryReadReplayPointCloudFrameRecord(
			EpisodeDirectory,
			*FrameObject,
			FrameRecord,
			ErrorMessage))
		{
			++InvalidLineCount;
			continue;
		}

		PointCloudFrameRecords.Add(MoveTemp(FrameRecord));
	}

	PointCloudFrameRecords.Sort(
		[](const FScenarioReplayPointCloudFrameRecord& Left, const FScenarioReplayPointCloudFrameRecord& Right)
		{
			return Left.TimeSeconds < Right.TimeSeconds;
		});

	if (InvalidLineCount > 0)
	{
		UE_LOG(
			LogScenarioReplay,
			Warning,
			TEXT("Replay point cloud frame index skipped invalid lines | Path=%s InvalidLines=%d"),
			*FrameIndexPath,
			InvalidLineCount);
	}

	UE_LOG(
		LogScenarioReplay,
		Log,
		TEXT("Replay point cloud frame index loaded | Path=%s Frames=%d"),
		*FrameIndexPath,
		PointCloudFrameRecords.Num());
}

void UScenarioReplaySubsystem::LoadEpisodeEventMarkers(
	const FString& EpisodeDirectory,
	TArray<FString>& OutDiagnostics)
{
	ReplayEventMarkers.Reset();

	FString EventsPath = FPaths::Combine(EpisodeDirectory, ReplayEventsFileName);
	FPaths::NormalizeFilename(EventsPath);
	if (!IFileManager::Get().FileExists(*EventsPath))
	{
		return;
	}

	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *EventsPath))
	{
		OutDiagnostics.Add(FString::Printf(
			TEXT("Replay event markers unavailable; failed to read events file: %s"),
			*EventsPath));
		return;
	}

	int32 InvalidLineCount = 0;
	int32 ClampedLineCount = 0;
	for (const FString& SourceLine : Lines)
	{
		FString Line = SourceLine;
		Line.TrimStartAndEndInline();
		if (Line.IsEmpty())
		{
			continue;
		}

		TSharedPtr<FJsonObject> EventObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Line);
		if (!FJsonSerializer::Deserialize(Reader, EventObject) || !EventObject.IsValid())
		{
			++InvalidLineCount;
			continue;
		}

		FString Schema;
		if (EventObject->TryGetStringField(TEXT("schema"), Schema)
			&& !Schema.Equals(TEXT("episode_event"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		double TimeSeconds = 0.0;
		if (!EventObject->TryGetNumberField(TEXT("run_time_seconds"), TimeSeconds)
			|| !FMath::IsFinite(TimeSeconds)
			|| TimeSeconds < 0.0)
		{
			++InvalidLineCount;
			continue;
		}

		if (Manifest.DurationSeconds > 0.0
			&& TimeSeconds > Manifest.DurationSeconds + KINDA_SMALL_NUMBER)
		{
			const double OverrunSeconds = TimeSeconds - Manifest.DurationSeconds;
			if (OverrunSeconds <= GetReplayEventMarkerEndToleranceSeconds(Manifest))
			{
				TimeSeconds = Manifest.DurationSeconds;
				++ClampedLineCount;
			}
			else
			{
				++InvalidLineCount;
				continue;
			}
		}

		double EventIndexValue = static_cast<double>(ReplayEventMarkers.Num());
		EventObject->TryGetNumberField(TEXT("event_index"), EventIndexValue);

		FScenarioReplayEventMarker Marker;
		Marker.TimeSeconds = TimeSeconds;
		Marker.EventIndex = FMath::RoundToInt(EventIndexValue);
		EventObject->TryGetStringField(TEXT("event_type"), Marker.EventType);
		EventObject->TryGetStringField(TEXT("reason"), Marker.Reason);
		EventObject->TryGetStringField(TEXT("message"), Marker.Message);
		ReplayEventMarkers.Add(MoveTemp(Marker));
	}

	ReplayEventMarkers.Sort(
		[](const FScenarioReplayEventMarker& Left, const FScenarioReplayEventMarker& Right)
		{
			return Left.TimeSeconds < Right.TimeSeconds;
		});

	if (InvalidLineCount > 0)
	{
		UE_LOG(
			LogScenarioReplay,
			Warning,
			TEXT("Replay event marker load skipped invalid lines | Path=%s InvalidLines=%d"),
			*EventsPath,
			InvalidLineCount);
	}

	if (ClampedLineCount > 0)
	{
		UE_LOG(
			LogScenarioReplay,
			Log,
			TEXT("Replay event marker load clamped end-of-replay lines | Path=%s ClampedLines=%d Duration=%.3f"),
			*EventsPath,
			ClampedLineCount,
			Manifest.DurationSeconds);
	}

	UE_LOG(
		LogScenarioReplay,
		Log,
		TEXT("Replay event markers loaded | Path=%s Markers=%d"),
		*EventsPath,
		ReplayEventMarkers.Num());
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
	return !LidarRayFrames.IsEmpty();
}

bool UScenarioReplaySubsystem::LoadEpisodeLidarSensorConfig(
	const FString& EpisodeDirectory,
	TArray<FString>& OutDiagnostics)
{
	ReplayLidarSensorConfig = FDeliveryBotLidarSensorConfigInfo{};
	bHasReplayLidarSensorConfig = false;

	FString ProfilePath;
	if (!TryResolveEpisodeReplayProfilePath(EpisodeDirectory, ProfilePath))
	{
		OutDiagnostics.Add(FString::Printf(
			TEXT("Replay LiDAR range layer using default config: profile.json not found near episode directory. Last checked: %s"),
			*ProfilePath));
		return false;
	}

	const UDeliveryBotSetupCompiler* DeliveryBotSetupCompiler =
		NewObject<UDeliveryBotSetupCompiler>(this);
	if (!IsValid(DeliveryBotSetupCompiler))
	{
		OutDiagnostics.Add(TEXT("Replay LiDAR range layer using default config: setup compiler unavailable."));
		return false;
	}

	const FDeliveryBotSetupCompileResult CompileResult =
		DeliveryBotSetupCompiler->CompileDeliveryBotSetupFromJsonFile(ProfilePath);
	for (const FScenarioCompileDiagnostic& Diagnostic : CompileResult.Diagnostics)
	{
		if (Diagnostic.Severity == EScenarioCompileDiagnosticSeverity::Error)
		{
			OutDiagnostics.Add(FString::Printf(
				TEXT("Replay LiDAR profile diagnostic: %s"),
				*Diagnostic.Message));
		}
	}

	if (!CompileResult.bSuccess)
	{
		OutDiagnostics.Add(FString::Printf(
			TEXT("Replay LiDAR range layer using default config: profile compile failed: %s"),
			*ProfilePath));
		return false;
	}

	ReplayLidarSensorConfig = CompileResult.SetupInfo.LidarSensorConfigInfo;
	bHasReplayLidarSensorConfig = true;
	UE_LOG(
		LogScenarioReplay,
		Log,
		TEXT("Replay LiDAR sensor config loaded | Profile: %s RangeM=%.2f SlowM=%.2f StopM=%.2f FrontHalfAngle=%.2f"),
		*ProfilePath,
		ReplayLidarSensorConfig.ScanRangeM,
		ReplayLidarSensorConfig.SlowDownDistanceM,
		ReplayLidarSensorConfig.StopDistanceM,
		ReplayLidarSensorConfig.FrontHalfAngleDegree);
	return true;
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
	LidarRayActor->SetLidarSensorRaysVisible(bReplayLidarRaysVisible);
	LidarRayActor->SetLidarDistanceOverlayVisible(bReplayLidarDistanceVisible);
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

	TArray<FScenarioGroundRegionSpec> ReplayGroundRegionSpecs;
	ReplayGroundRegionSpecs.Reserve(WorldSpec.GroundRegions.Num());
	for (const FScenarioGroundRegionSpec& GroundRegionSpec : WorldSpec.GroundRegions)
	{
		FScenarioGroundRegionSpec ReplayGroundRegionSpec = GroundRegionSpec;
		ReplayGroundRegionSpec.Center += ReplayWorldOffset;
		ReplayGroundRegionSpecs.Add(ReplayGroundRegionSpec);

		FString FailureReason;
		AScenarioGroundRegion* GroundRegion = AScenarioGroundRegion::SpawnConfigured(
			World,
			AScenarioGroundRegion::StaticClass(),
			ReplayGroundRegionSpec,
			FailureReason);
		if (!IsValid(GroundRegion))
		{
			OutDiagnostics.Add(FString::Printf(
				TEXT("Failed to spawn replay ground region '%s': %s"),
				*GroundRegionSpec.RegionId,
				*FailureReason));
			bAllSpawned = false;
			continue;
		}

		RegisterReplayScenarioActor(GroundRegion);
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

	if (!SpawnReplayRouteMarkerActor(WorldSpec, OutDiagnostics))
	{
		bAllSpawned = false;
	}

	const TSoftObjectPtr<UScenarioCityBlockCatalog> CityBlockCatalogRef =
		UScenarioCityBlockCatalog::MakeDefaultCatalogReference();
	const UScenarioCityBlockCatalog* CityBlockCatalog = CityBlockCatalogRef.LoadSynchronous();
	FScenarioCityBlockMaterializationOptions CityBlockOptions;
	CityBlockOptions.LogContext = TEXT("ScenarioReplay");
	CityBlockOptions.CatalogDebugName = CityBlockCatalogRef.ToSoftObjectPath().ToString();
	TArray<TObjectPtr<AActor>> SpawnedCityBlockActors;
	const FScenarioCityBlockMaterializationResult CityBlockResult =
		FScenarioCityBlockMaterializer::SpawnGeneratedCityBlocks(
			World,
			CityBlockCatalog,
			ReplayGroundRegionSpecs,
			SpawnedCityBlockActors,
			CityBlockOptions);
	for (const TObjectPtr<AActor>& CityBlockActor : SpawnedCityBlockActors)
	{
		RegisterReplayScenarioActor(CityBlockActor.Get());
	}

	UE_LOG(
		LogScenarioReplay,
		Log,
		TEXT("Replay scenario city blocks materialized | CandidateRegions=%d SpawnedActors=%d RoadSideComposites=%d Corners=%d SkippedNoEntry=%d"),
		CityBlockResult.CandidateRegionCount,
		CityBlockResult.SpawnedActorCount,
		CityBlockResult.SpawnedRoadSideCompositeCount,
		CityBlockResult.CornerCandidateCount,
		CityBlockResult.SkippedNoEntryCount);

	return bAllSpawned;
}

bool UScenarioReplaySubsystem::SpawnReplayRouteMarkerActor(
	const FScenarioWorldSpec& WorldSpec,
	TArray<FString>& OutDiagnostics)
{
	FVector StartLocationCm = FVector::ZeroVector;
	FVector GoalLocationCm = FVector::ZeroVector;
	bool bHasStartLocation = false;
	bool bHasGoalLocation = false;
	if (!TryResolveReplayRouteMarkerLocations(
		WorldSpec,
		StartLocationCm,
		bHasStartLocation,
		GoalLocationCm,
		bHasGoalLocation))
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		OutDiagnostics.Add(TEXT("Replay route markers require a valid world."));
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AScenarioReplayRouteMarkerActor* RouteMarkerActor =
		World->SpawnActor<AScenarioReplayRouteMarkerActor>(
			AScenarioReplayRouteMarkerActor::StaticClass(),
			FTransform::Identity,
			SpawnParameters);
	if (!IsValid(RouteMarkerActor))
	{
		OutDiagnostics.Add(TEXT("Failed to spawn replay route marker actor."));
		return false;
	}

	RouteMarkerActor->Tags.AddUnique(FName(TEXT("ReplayOnly")));
	RouteMarkerActor->ConfigureRouteMarkers(
		StartLocationCm + ReplayWorldOffset,
		bHasStartLocation,
		GoalLocationCm + ReplayWorldOffset,
		bHasGoalLocation);
	RouteMarkerActor->SetRouteMarkersVisible(bReplayMapVisible);
	ReplayRouteMarkerActor = RouteMarkerActor;
	RefreshReplayCaptureShowOnlyActors();

	return true;
}

bool UScenarioReplaySubsystem::TryResolveReplayRouteMarkerLocations(
	const FScenarioWorldSpec& WorldSpec,
	FVector& OutStartLocationCm,
	bool& bOutHasStartLocation,
	FVector& OutGoalLocationCm,
	bool& bOutHasGoalLocation) const
{
	OutStartLocationCm = FVector::ZeroVector;
	OutGoalLocationCm = FVector::ZeroVector;
	bOutHasStartLocation = false;
	bOutHasGoalLocation = false;

	for (const FScenarioPlaceableInstanceSpec& PlaceableSpec : WorldSpec.Placeables)
	{
		if (PlaceableSpec.Category != EScenarioActorCategory::DeliveryBot)
		{
			continue;
		}

		const FDeliveryBotLocationSetupInfo& LocationSetupInfo =
			PlaceableSpec.DeliveryBot.SetupInfo.LocationSetupInfo;
		OutStartLocationCm = PlaceableSpec.DeliveryBot.bHasStartLocation
			? LocationSetupInfo.StartLocationCm
			: PlaceableSpec.Transform.GetLocation();
		bOutHasStartLocation = true;

		if (PlaceableSpec.DeliveryBot.bHasGoalLocation || LocationSetupInfo.bHasGoal)
		{
			OutGoalLocationCm = LocationSetupInfo.GoalLocationCm;
			bOutHasGoalLocation = true;
		}

		return bOutHasStartLocation || bOutHasGoalLocation;
	}

	return false;
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

		if (IsValid(ReplayRouteMarkerActor))
		{
			CaptureComponent.ShowOnlyActors.Add(ReplayRouteMarkerActor);
		}
	}

	if (bReplayPointCloudVisible && IsValid(ReplayPointCloudActor))
	{
		CaptureComponent.ShowOnlyActors.Add(ReplayPointCloudActor);
	}
	if (bReplayPointCloudVisible
		&& PointCloudFrameRecords.IsValidIndex(CurrentPointCloudFrameHighlightIndex)
		&& IsValid(ReplayPointCloudFrameHighlightActor))
	{
		CaptureComponent.ShowOnlyActors.Add(ReplayPointCloudFrameHighlightActor);
	}

	if ((bReplayLidarRaysVisible || bReplayLidarDistanceVisible) && IsValid(ReplayLidarRayActor))
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
	const EDeliveryBotPointCloudReviewRenderMode RenderMode =
		CameraMode == EScenarioReplayCameraMode::TopDown
			? EDeliveryBotPointCloudReviewRenderMode::TopDownProjection
			: EDeliveryBotPointCloudReviewRenderMode::Plugin3D;

	if (IsValid(ReplayPointCloudActor))
	{
		ReplayPointCloudActor->SetReviewRenderMode(RenderMode);
	}
	if (IsValid(ReplayPointCloudFrameHighlightActor))
	{
		ReplayPointCloudFrameHighlightActor->SetReviewRenderMode(RenderMode);
	}
	if (IsValid(ReplayPointCloudFrameHighlightBackBufferActor))
	{
		ReplayPointCloudFrameHighlightBackBufferActor->SetReviewRenderMode(RenderMode);
	}
}

// Updates the current-frame point cloud highlight for the requested replay time.
void UScenarioReplaySubsystem::ApplyPointCloudFrameHighlightAtTime(const double TimeSeconds)
{
	if ((!IsValid(ReplayPointCloudFrameHighlightActor)
			&& !IsValid(ReplayPointCloudFrameHighlightBackBufferActor))
		|| PointCloudFrameRecords.IsEmpty())
	{
		CurrentPointCloudFrameHighlightIndex = INDEX_NONE;
		BackBufferPointCloudFrameHighlightIndex = INDEX_NONE;
		return;
	}
	if (!bReplayPointCloudVisible)
	{
		if (IsValid(ReplayPointCloudFrameHighlightActor))
		{
			ReplayPointCloudFrameHighlightActor->SetPointCloudVisible(false);
		}
		if (IsValid(ReplayPointCloudFrameHighlightBackBufferActor))
		{
			ReplayPointCloudFrameHighlightBackBufferActor->SetPointCloudVisible(false);
		}
		return;
	}

	const int32 FrameIndex = ResolvePointCloudFrameHighlightIndexAtTime(TimeSeconds);
	if (FrameIndex == CurrentPointCloudFrameHighlightIndex)
	{
		if (IsValid(ReplayPointCloudFrameHighlightActor))
		{
			ReplayPointCloudFrameHighlightActor->SetPointCloudVisible(
				PointCloudFrameRecords.IsValidIndex(CurrentPointCloudFrameHighlightIndex));
		}
		if (IsValid(ReplayPointCloudFrameHighlightBackBufferActor))
		{
			ReplayPointCloudFrameHighlightBackBufferActor->SetPointCloudVisible(false);
		}
		return;
	}

	if (!PointCloudFrameRecords.IsValidIndex(FrameIndex))
	{
		if (IsValid(ReplayPointCloudFrameHighlightActor))
		{
			ReplayPointCloudFrameHighlightActor->SetPointCloudVisible(false);
		}
		if (IsValid(ReplayPointCloudFrameHighlightBackBufferActor))
		{
			ReplayPointCloudFrameHighlightBackBufferActor->SetPointCloudVisible(false);
		}
		CurrentPointCloudFrameHighlightIndex = INDEX_NONE;
		RefreshReplayCaptureShowOnlyActors();
		return;
	}

	if (FrameIndex == BackBufferPointCloudFrameHighlightIndex
		&& IsValid(ReplayPointCloudFrameHighlightBackBufferActor))
	{
		ADeliveryBotPointCloudReviewActor* PreviousActiveActor =
			ReplayPointCloudFrameHighlightActor.Get();
		ADeliveryBotPointCloudReviewActor* NewActiveActor =
			ReplayPointCloudFrameHighlightBackBufferActor.Get();
		const int32 PreviousActiveFrameIndex = CurrentPointCloudFrameHighlightIndex;

		ReplayPointCloudFrameHighlightActor = NewActiveActor;
		ReplayPointCloudFrameHighlightBackBufferActor = PreviousActiveActor;
		CurrentPointCloudFrameHighlightIndex = FrameIndex;
		BackBufferPointCloudFrameHighlightIndex = PreviousActiveFrameIndex;

		ReplayPointCloudFrameHighlightActor->SetPointCloudVisible(true);
		if (IsValid(ReplayPointCloudFrameHighlightBackBufferActor))
		{
			ReplayPointCloudFrameHighlightBackBufferActor->SetPointCloudVisible(false);
		}
		RefreshReplayPointCloudRenderMode();
		RefreshReplayCaptureShowOnlyActors();
		return;
	}

	ADeliveryBotPointCloudReviewActor* LoadTargetActor =
		ReplayPointCloudFrameHighlightBackBufferActor.Get();
	if (!IsValid(LoadTargetActor))
	{
		LoadTargetActor = ReplayPointCloudFrameHighlightActor.Get();
	}
	if (!IsValid(LoadTargetActor))
	{
		return;
	}

	LoadTargetActor->SetPointCloudVisible(false);

	const FScenarioReplayPointCloudFrameRecord& FrameRecord =
		PointCloudFrameRecords[FrameIndex];
	if (!LoadTargetActor->LoadReplayMapPointCloudFromFile(
		FrameRecord.XyzFilePath,
		ReplayPointCloudCaptureOriginCm,
		ReplayPointCloudImportYAxisSign))
	{
		UE_LOG(
			LogScenarioReplay,
			Warning,
			TEXT("Replay point cloud frame highlight load failed | Index=%d Path=%s"),
			FrameIndex,
			*FrameRecord.XyzFilePath);
		return;
	}

	ADeliveryBotPointCloudReviewActor* PreviousActiveActor =
		ReplayPointCloudFrameHighlightActor.Get();
	const int32 PreviousActiveFrameIndex = CurrentPointCloudFrameHighlightIndex;

	ReplayPointCloudFrameHighlightActor = LoadTargetActor;
	ReplayPointCloudFrameHighlightBackBufferActor =
		LoadTargetActor == PreviousActiveActor ? nullptr : PreviousActiveActor;
	CurrentPointCloudFrameHighlightIndex = FrameIndex;
	BackBufferPointCloudFrameHighlightIndex =
		IsValid(ReplayPointCloudFrameHighlightBackBufferActor)
			? PreviousActiveFrameIndex
			: INDEX_NONE;

	RefreshReplayPointCloudRenderMode();
	ReplayPointCloudFrameHighlightActor->SetPointCloudVisible(bReplayPointCloudVisible);
	if (IsValid(ReplayPointCloudFrameHighlightBackBufferActor))
	{
		ReplayPointCloudFrameHighlightBackBufferActor->SetPointCloudVisible(false);
	}
	RefreshReplayCaptureShowOnlyActors();
}

// Returns the point cloud frame index active at the requested replay time.
int32 UScenarioReplaySubsystem::ResolvePointCloudFrameHighlightIndexAtTime(
	const double TimeSeconds) const
{
	if (PointCloudFrameRecords.IsEmpty() || !FMath::IsFinite(TimeSeconds))
	{
		return INDEX_NONE;
	}

	int32 LowerIndex = 0;
	int32 UpperIndex = PointCloudFrameRecords.Num() - 1;
	int32 BestIndex = INDEX_NONE;
	while (LowerIndex <= UpperIndex)
	{
		const int32 MidIndex = LowerIndex + (UpperIndex - LowerIndex) / 2;
		if (PointCloudFrameRecords[MidIndex].TimeSeconds <= TimeSeconds)
		{
			BestIndex = MidIndex;
			LowerIndex = MidIndex + 1;
		}
		else
		{
			UpperIndex = MidIndex - 1;
		}
	}

	return BestIndex;
}

// Updates the route marker mesh and billboard facing for the active replay camera mode.
void UScenarioReplaySubsystem::RefreshReplayRouteMarkerPresentation()
{
	if (!IsValid(ReplayRouteMarkerActor))
	{
		return;
	}

	const bool bUseBillboardMarkers =
		CameraMode != EScenarioReplayCameraMode::TopDown;
	ReplayRouteMarkerActor->SetBillboardMarkersEnabled(bUseBillboardMarkers);

	if (bUseBillboardMarkers && IsValid(ReplayCaptureActor))
	{
		ReplayRouteMarkerActor->FaceCameraLocation(ReplayCaptureActor->GetActorLocation());
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
	CurrentRobotThrottle = Frame.Throttle;
	CurrentRobotSteering = Frame.Steering;
	CurrentRobotBrake = Frame.Brake;
	CurrentRobotTargetSpeedKmh = Frame.TargetSpeedKmh;
	CurrentRobotPositionCm = Frame.PositionCm;
	ApplyLidarRayFrameAtTime(TimeSeconds);
	ApplyPointCloudFrameHighlightAtTime(TimeSeconds);

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

	FEpisodeReplayRobotFrame RobotFrame;
	int32 RobotFrameIndex = INDEX_NONE;
	if (!BuildInterpolatedFrameAtTime(CurrentReplayTimeSeconds, RobotFrame, RobotFrameIndex))
	{
		ReplayLidarRayActor->ClearLidarRays();
		return;
	}

	ReplayLidarRayActor->ApplyLidarRayFrame(
		RobotFrame,
		ReplayLidarSensorConfig,
		GetCurrentLidarRayFrame(),
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
		RefreshReplayRouteMarkerPresentation();
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
	RenderTarget->InitAutoFormat(2560, 1440);
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
	// Replay capture favors readable map/point-cloud layers over scene shadow fidelity.
	CaptureComponent->ShowFlags.SetDynamicShadows(false);
	CaptureComponent->ShowFlags.SetContactShadows(false);
	CaptureComponent->ShowFlags.SetCapsuleShadows(false);
	CaptureComponent->ShowFlags.SetRayTracedDistanceFieldShadows(false);
	PopulateReplayCaptureShowOnlyActors(*CaptureComponent);
	CaptureComponent->CaptureSource = SCS_FinalColorLDR;
	CaptureComponent->TextureTarget = ReplayRenderTarget;
	CaptureComponent->bCaptureEveryFrame = false;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->bAlwaysPersistRenderingState = false;
	CaptureComponent->bExcludeFromSceneTextureExtents = true;
	CaptureComponent->bUseRayTracingIfEnabled = false;
	if (FScenarioViewportPresentation::bUseGreyBackgroundPostProcess
		&& !FScenarioViewportPresentation::ApplyGreyBackgroundPostProcess(CaptureComponent, 1.0f))
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
