#include "Episode/EpisodeLidarRayReplayRecorder.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"

namespace
{
	// Folder name for replay-only artifacts inside an episode directory.
	const TCHAR* LidarRayReplayArtifactDirectoryName = TEXT("replay");

	// Folder name for LiDAR ray replay artifacts inside the replay artifact directory.
	const TCHAR* LidarRayReplayDirectoryName = TEXT("lidar_rays");

	// Binary frame file name stored in the LiDAR ray artifact folder.
	const TCHAR* LidarRayReplayFrameFileName = TEXT("rays.frames.bin");

	// Manifest file name stored in the LiDAR ray artifact folder.
	const TCHAR* LidarRayReplayManifestFileName = TEXT("rays.meta.json");

	// Adds one recorder diagnostic and returns false for validation branches.
	bool AddLidarRayRecorderDiagnostic(TArray<FString>& OutDiagnostics, const FString& Message)
	{
		OutDiagnostics.Add(Message);
		return false;
	}

	// Counts all ray samples stored in a frame array.
	int32 CountRecordedRays(const TArray<FEpisodeLidarRayFrame>& Frames)
	{
		int32 RayCount = 0;
		for (const FEpisodeLidarRayFrame& Frame : Frames)
		{
			RayCount += Frame.Rays.Num();
		}

		return RayCount;
	}

	// Returns true when the lower-case text contains one semantic obstacle token.
	bool ContainsObstacleToken(const FString& LowerText)
	{
		return LowerText.Contains(TEXT("obstacle"))
			|| LowerText.Contains(TEXT("pedestrian"))
			|| LowerText.Contains(TEXT("vehicle"))
			|| LowerText.Contains(TEXT("prop"));
	}

	// Returns true when the lower-case text contains one semantic wall token.
	bool ContainsWallToken(const FString& LowerText)
	{
		return LowerText.Contains(TEXT("wall"))
			|| LowerText.Contains(TEXT("barrier"))
			|| LowerText.Contains(TEXT("fence"));
	}

	// Returns true when the lower-case text contains one semantic ground token.
	bool ContainsGroundToken(const FString& LowerText)
	{
		return LowerText.Contains(TEXT("ground"))
			|| LowerText.Contains(TEXT("floor"))
			|| LowerText.Contains(TEXT("road"))
			|| LowerText.Contains(TEXT("walkable"))
			|| LowerText.Contains(TEXT("lane"));
	}

	// Returns true when the lower-case text marks generated corridor geometry.
	bool ContainsCorridorToken(const FString& LowerText)
	{
		return LowerText.Contains(TEXT("corridor"));
	}
}

bool FEpisodeLidarRayReplayRecorder::Open(
	const FString& InEpisodeDirectory,
	TArray<FString>& OutDiagnostics)
{
	OutDiagnostics.Reset();
	Abort();

	EpisodeDirectory = InEpisodeDirectory.TrimStartAndEnd();
	FPaths::NormalizeDirectoryName(EpisodeDirectory);
	if (EpisodeDirectory.IsEmpty())
	{
		return AddLidarRayRecorderDiagnostic(
			OutDiagnostics,
			TEXT("LiDAR ray recorder episode directory must not be empty."));
	}

	LidarRayReplayDirectory = FPaths::Combine(
		EpisodeDirectory,
		LidarRayReplayArtifactDirectoryName,
		LidarRayReplayDirectoryName);
	FPaths::NormalizeDirectoryName(LidarRayReplayDirectory);
	if (!IFileManager::Get().MakeDirectory(*LidarRayReplayDirectory, true))
	{
		return AddLidarRayRecorderDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Failed to create LiDAR ray replay directory: %s"), *LidarRayReplayDirectory));
	}

	Frames.Reset();
	StartWorldTimeSeconds = 0.0;
	LastRecordedSensorSequence = INDEX_NONE;
	bHasStartTime = false;
	bOpen = true;
	return true;
}

bool FEpisodeLidarRayReplayRecorder::RecordSensorSnapshot(
	double WorldTimeSeconds,
	int32 SensorSequence,
	const FDeliveryBotLidarScanInfo& ScanInfo,
	TArray<FString>& OutDiagnostics)
{
	OutDiagnostics.Reset();
	if (!bOpen)
	{
		return false;
	}

	if (!FMath::IsFinite(WorldTimeSeconds))
	{
		return AddLidarRayRecorderDiagnostic(
			OutDiagnostics,
			TEXT("LiDAR ray recorder rejected a non-finite world time."));
	}

	if (SensorSequence <= 0 || SensorSequence == LastRecordedSensorSequence)
	{
		return true;
	}

	if (!bHasStartTime)
	{
		StartWorldTimeSeconds = WorldTimeSeconds;
		bHasStartTime = true;
	}

	FEpisodeLidarRayFrame Frame;
	Frame.TimeSeconds = static_cast<float>(FMath::Max(0.0, WorldTimeSeconds - StartWorldTimeSeconds));
	Frame.SensorSequence = SensorSequence;
	Frame.Rays.Reserve(ScanInfo.RayInfos.Num());
	for (const FDeliveryBotLidarRayInfo& RayInfo : ScanInfo.RayInfos)
	{
		Frame.Rays.Add(BuildReplayRaySample(RayInfo));
	}

	TArray<FString> FrameDiagnostics;
	if (!Frame.IsValidFrame(FrameDiagnostics))
	{
		OutDiagnostics.Append(FrameDiagnostics);
		return false;
	}

	Frames.Add(Frame);
	LastRecordedSensorSequence = SensorSequence;
	return true;
}

bool FEpisodeLidarRayReplayRecorder::Close(TArray<FString>& OutDiagnostics)
{
	OutDiagnostics.Reset();
	if (!bOpen)
	{
		return true;
	}

	if (Frames.IsEmpty())
	{
		Abort();
		return true;
	}

	const FString FramePath = FPaths::Combine(LidarRayReplayDirectory, LidarRayReplayFrameFileName);
	const FString ManifestPath = FPaths::Combine(LidarRayReplayDirectory, LidarRayReplayManifestFileName);

	bool bSuccess = FEpisodeLidarRayReplayBinary::SaveFramesToFile(FramePath, Frames, OutDiagnostics);
	if (bSuccess)
	{
		TArray<FString> ManifestDiagnostics;
		const bool bManifestSaved = FEpisodeLidarRayReplayManifestJson::SaveToFile(
			ManifestPath,
			BuildManifest(),
			ManifestDiagnostics);
		if (!bManifestSaved)
		{
			OutDiagnostics.Append(ManifestDiagnostics);
			bSuccess = false;
		}
	}

	Abort();
	return bSuccess;
}

void FEpisodeLidarRayReplayRecorder::Abort()
{
	EpisodeDirectory.Reset();
	LidarRayReplayDirectory.Reset();
	Frames.Reset();
	StartWorldTimeSeconds = 0.0;
	LastRecordedSensorSequence = INDEX_NONE;
	bHasStartTime = false;
	bOpen = false;
}

FEpisodeLidarRaySample FEpisodeLidarRayReplayRecorder::BuildReplayRaySample(
	const FDeliveryBotLidarRayInfo& RayInfo) const
{
	FEpisodeLidarRaySample Sample;
	Sample.DimensionType = static_cast<uint8>(RayInfo.RayDimensionType);
	Sample.Classification = ResolveRayClassification(RayInfo);
	Sample.bHit = RayInfo.bHit;
	Sample.RayIndex = RayInfo.RayIndex;
	Sample.RayYawDegree = RayInfo.RayYawDegree;
	Sample.RayPitchDegree = RayInfo.RayPitchDegree;
	Sample.DistanceM = RayInfo.DistanceM;
	Sample.StartLocationCm = RayInfo.StartLocationCm;
	Sample.EndLocationCm = RayInfo.EndLocationCm;
	Sample.HitLocationCm = RayInfo.HitLocationCm;
	return Sample;
}

ELidarRayReplayClassification FEpisodeLidarRayReplayRecorder::ResolveRayClassification(
	const FDeliveryBotLidarRayInfo& RayInfo) const
{
	if (!RayInfo.bHit)
	{
		return ELidarRayReplayClassification::Miss;
	}

	const ELidarRayReplayClassification TargetTagClassification =
		ResolveClassificationFromTags(RayInfo.TargetTags, RayInfo);
	if (TargetTagClassification != ELidarRayReplayClassification::Unknown)
	{
		return TargetTagClassification;
	}

	const ELidarRayReplayClassification ActorTagClassification =
		ResolveClassificationFromTags(RayInfo.ActorTags, RayInfo);
	if (ActorTagClassification != ELidarRayReplayClassification::Unknown)
	{
		return ActorTagClassification;
	}

	const ELidarRayReplayClassification ActorNameClassification =
		ResolveClassificationFromText(RayInfo.ActorName, RayInfo);
	if (ActorNameClassification != ELidarRayReplayClassification::Unknown)
	{
		return ActorNameClassification;
	}

	if (RayInfo.TargetTags.IsEmpty() && RayInfo.ActorTags.IsEmpty() && RayInfo.ActorName.TrimStartAndEnd().IsEmpty())
	{
		return ELidarRayReplayClassification::Ground;
	}

	return ELidarRayReplayClassification::Unknown;
}

ELidarRayReplayClassification FEpisodeLidarRayReplayRecorder::ResolveClassificationFromTags(
	const TArray<FName>& Tags,
	const FDeliveryBotLidarRayInfo& RayInfo) const
{
	for (const FName& Tag : Tags)
	{
		const ELidarRayReplayClassification Classification =
			ResolveClassificationFromText(Tag.ToString(), RayInfo);
		if (Classification != ELidarRayReplayClassification::Unknown)
		{
			return Classification;
		}
	}

	return ELidarRayReplayClassification::Unknown;
}

ELidarRayReplayClassification FEpisodeLidarRayReplayRecorder::ResolveClassificationFromText(
	const FString& Text,
	const FDeliveryBotLidarRayInfo& RayInfo) const
{
	const FString LowerText = Text.TrimStartAndEnd().ToLower();
	if (LowerText.IsEmpty())
	{
		return ELidarRayReplayClassification::Unknown;
	}

	if (ContainsObstacleToken(LowerText))
	{
		return ELidarRayReplayClassification::Obstacle;
	}

	if (ContainsWallToken(LowerText))
	{
		return ELidarRayReplayClassification::Wall;
	}

	if (ContainsGroundToken(LowerText))
	{
		return ELidarRayReplayClassification::Ground;
	}

	if (ContainsCorridorToken(LowerText))
	{
		return RayInfo.HitLocationCm.Z > 12.0
			? ELidarRayReplayClassification::Wall
			: ELidarRayReplayClassification::Ground;
	}

	return ELidarRayReplayClassification::Unknown;
}

FEpisodeLidarRayReplayManifest FEpisodeLidarRayReplayRecorder::BuildManifest() const
{
	FEpisodeLidarRayReplayManifest Manifest;
	Manifest.FrameFile = LidarRayReplayFrameFileName;
	Manifest.FrameCount = Frames.Num();
	Manifest.TotalRayCount = CountRecordedRays(Frames);
	if (!Frames.IsEmpty())
	{
		Manifest.FirstSensorSequence = Frames[0].SensorSequence;
		Manifest.LastSensorSequence = Frames.Last().SensorSequence;
		Manifest.FirstTimeSeconds = Frames[0].TimeSeconds;
		Manifest.LastTimeSeconds = Frames.Last().TimeSeconds;
	}

	return Manifest;
}
