#include "Episode/EpisodeReplayRecorder.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"

namespace
{
	// Folder name for replay-only artifacts inside an episode directory.
	const TCHAR* ReplayArtifactDirectoryName = TEXT("replay");

	// Binary frame file name stored in the replay artifact folder.
	const TCHAR* ReplayFrameFileName = TEXT("replay.frames.bin");

	// Manifest file name stored in the replay artifact folder.
	const TCHAR* ReplayManifestFileName = TEXT("replay.meta.json");

	// Converts absolute scenario paths to a compact manifest source when possible.
	FString NormalizeReplaySourcePath(const FString& ScenarioSamplePath)
	{
		FString NormalizedPath = ScenarioSamplePath.TrimStartAndEnd();
		FPaths::NormalizeFilename(NormalizedPath);
		return FPaths::GetCleanFilename(NormalizedPath).IsEmpty()
			? FString(TEXT("scenario-sample.json"))
			: FPaths::GetCleanFilename(NormalizedPath);
	}

	// Adds one recorder diagnostic and returns false for validation branches.
	bool AddRecorderDiagnostic(TArray<FString>& OutDiagnostics, const FString& Message)
	{
		OutDiagnostics.Add(Message);
		return false;
	}
}

bool FEpisodeReplayRecorder::Open(
	const FString& InEpisodeDirectory,
	const FString& InScenarioSamplePath,
	const FString& InScenarioHash,
	TArray<FString>& OutDiagnostics)
{
	OutDiagnostics.Reset();
	Abort();

	EpisodeDirectory = InEpisodeDirectory.TrimStartAndEnd();
	FPaths::NormalizeDirectoryName(EpisodeDirectory);
	if (EpisodeDirectory.IsEmpty())
	{
		return AddRecorderDiagnostic(OutDiagnostics, TEXT("Replay recorder episode directory must not be empty."));
	}

	if (!IFileManager::Get().MakeDirectory(*EpisodeDirectory, true))
	{
		return AddRecorderDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Failed to create replay episode directory: %s"), *EpisodeDirectory));
	}

	ScenarioSamplePath = InScenarioSamplePath.TrimStartAndEnd();
	ScenarioHash = InScenarioHash.TrimStartAndEnd();
	Frames.Reset();
	StartWorldTimeSeconds = 0.0;
	NextSampleWorldTimeSeconds = 0.0;
	bHasStartTime = false;
	bOpen = true;
	return true;
}

bool FEpisodeReplayRecorder::RecordSample(
	double WorldTimeSeconds,
	FEpisodeReplayRobotFrame Frame,
	TArray<FString>& OutDiagnostics)
{
	OutDiagnostics.Reset();
	if (!bOpen)
	{
		return false;
	}

	if (!FMath::IsFinite(WorldTimeSeconds))
	{
		return AddRecorderDiagnostic(OutDiagnostics, TEXT("Replay recorder rejected a non-finite world time."));
	}

	if (!bHasStartTime)
	{
		StartWorldTimeSeconds = WorldTimeSeconds;
		NextSampleWorldTimeSeconds = WorldTimeSeconds;
		bHasStartTime = true;
	}

	constexpr double SampleIntervalSeconds = 1.0 / EpisodeReplayV1::SampleRateHz;
	if (WorldTimeSeconds + KINDA_SMALL_NUMBER < NextSampleWorldTimeSeconds)
	{
		return true;
	}

	Frame.TimeSeconds = static_cast<float>(FMath::Max(0.0, WorldTimeSeconds - StartWorldTimeSeconds));
	if (!Frame.IsValidFrame())
	{
		return AddRecorderDiagnostic(OutDiagnostics, TEXT("Replay recorder rejected an invalid frame."));
	}

	Frames.Add(Frame);
	NextSampleWorldTimeSeconds = WorldTimeSeconds + SampleIntervalSeconds;
	return true;
}

bool FEpisodeReplayRecorder::Close(TArray<FString>& OutDiagnostics)
{
	OutDiagnostics.Reset();
	if (!bOpen)
	{
		return true;
	}

	const FString ReplayDirectory = FPaths::Combine(EpisodeDirectory, ReplayArtifactDirectoryName);
	const FString FramePath = FPaths::Combine(ReplayDirectory, ReplayFrameFileName);
	const FString ManifestPath = FPaths::Combine(ReplayDirectory, ReplayManifestFileName);

	if (Frames.IsEmpty())
	{
		Abort();
		return AddRecorderDiagnostic(OutDiagnostics, TEXT("Replay recorder has no frames to write."));
	}

	bool bSuccess = FEpisodeReplayBinary::SaveFramesToFile(FramePath, Frames, OutDiagnostics);

	FEpisodeReplayManifest Manifest;
	Manifest.FrameFile = ReplayFrameFileName;
	Manifest.ScenarioSample = NormalizeReplaySourcePath(ScenarioSamplePath);
	Manifest.ScenarioHash = ScenarioHash;
	Manifest.FrameCount = Frames.Num();
	Manifest.DurationSeconds = Frames.Last().TimeSeconds;
	Manifest.SampleRateHz = EpisodeReplayV1::SampleRateHz;
	Manifest.Version = FEpisodeReplayBinary::ResolveFrameVersion(Frames);
	Manifest.FrameSizeBytes = FEpisodeReplayBinary::GetFrameSizeBytesForVersion(Manifest.Version);
	Manifest.FirstFrameOffsetBytes = EpisodeReplayV1::BinaryHeaderSizeBytes;
	Manifest.Features.bRobotBody = true;
	Manifest.Features.bControl = true;
	Manifest.Features.bWheels = Manifest.Version == EpisodeReplayV2::Version;
	Manifest.Features.bMovingActors = false;

	TArray<FString> ManifestDiagnostics;
	const bool bManifestSaved = FEpisodeReplayManifestJson::SaveToFile(
		ManifestPath,
		Manifest,
		ManifestDiagnostics);
	if (!bManifestSaved)
	{
		OutDiagnostics.Append(ManifestDiagnostics);
		bSuccess = false;
	}

	Abort();
	return bSuccess;
}

void FEpisodeReplayRecorder::Abort()
{
	EpisodeDirectory.Reset();
	ScenarioSamplePath.Reset();
	ScenarioHash.Reset();
	Frames.Reset();
	StartWorldTimeSeconds = 0.0;
	NextSampleWorldTimeSeconds = 0.0;
	bHasStartTime = false;
	bOpen = false;
}
