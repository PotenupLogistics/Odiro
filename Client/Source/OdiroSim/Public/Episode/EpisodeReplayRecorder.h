#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodeReplayDataTypes.h"

// Buffers one episode's replay frames and writes replay artifacts at episode end.
class ODIROSIM_API FEpisodeReplayRecorder
{
public:
	// Opens a recording session for one episode output directory.
	bool Open(
		const FString& InEpisodeDirectory,
		const FString& InScenarioSamplePath,
		const FString& InScenarioHash,
		TArray<FString>& OutDiagnostics);

	// Adds one world-time frame if the V1 sampling interval has elapsed.
	bool RecordSample(
		double WorldTimeSeconds,
		FEpisodeReplayRobotFrame Frame,
		TArray<FString>& OutDiagnostics);

	// Writes replay.frames.bin and replay.meta.json for the buffered frames.
	bool Close(TArray<FString>& OutDiagnostics);

	// Discards any buffered replay data without writing files.
	void Abort();

	// Returns true while the recorder owns an open episode session.
	bool IsOpen() const { return bOpen; }

	// Returns the number of buffered frames awaiting final write.
	int32 GetFrameCount() const { return Frames.Num(); }

private:
	// Absolute episode directory that will receive replay artifacts.
	FString EpisodeDirectory;

	// Source scenario_sample path recorded in the manifest.
	FString ScenarioSamplePath;

	// Source scenario hash recorded in the manifest when available.
	FString ScenarioHash;

	// Buffered V1 frames for the active episode.
	TArray<FEpisodeReplayRobotFrame> Frames;

	// First world-time sample used as episode replay time zero.
	double StartWorldTimeSeconds = 0.0;

	// Next world-time threshold at which a sample should be accepted.
	double NextSampleWorldTimeSeconds = 0.0;

	// True after Open succeeds and before Close or Abort completes.
	bool bOpen = false;

	// True after the first frame establishes StartWorldTimeSeconds.
	bool bHasStartTime = false;
};
