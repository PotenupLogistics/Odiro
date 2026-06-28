#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodeLidarRayReplayDataTypes.h"
#include "Shared/Struct/DeliveryBot/Perception/DeliveryBotLidarSensorInfo.h"

// Buffers one episode's LiDAR ray frames and writes replay-only ray artifacts at episode end.
class ODIROSIM_API FEpisodeLidarRayReplayRecorder
{
public:
	// Opens a LiDAR ray recording session for one episode output directory.
	bool Open(
		const FString& InEpisodeDirectory,
		TArray<FString>& OutDiagnostics);

	// Adds one LiDAR sensor frame when SensorSequence changes.
	bool RecordSensorSnapshot(
		double WorldTimeSeconds,
		int32 SensorSequence,
		const FDeliveryBotLidarScanInfo& ScanInfo,
		TArray<FString>& OutDiagnostics);

	// Writes replay/lidar_rays/rays.frames.bin and rays.meta.json for buffered frames.
	bool Close(TArray<FString>& OutDiagnostics);

	// Discards any buffered LiDAR ray data without writing files.
	void Abort();

	// Returns true while the recorder owns an open episode session.
	bool IsOpen() const { return bOpen; }

	// Returns the number of buffered sensor frames awaiting final write.
	int32 GetFrameCount() const { return Frames.Num(); }

private:
	// Converts one runtime sensor ray into its replay binary sample shape.
	FEpisodeLidarRaySample BuildReplayRaySample(const FDeliveryBotLidarRayInfo& RayInfo) const;

	// Resolves one runtime sensor ray into a stable replay color classification.
	ELidarRayReplayClassification ResolveRayClassification(const FDeliveryBotLidarRayInfo& RayInfo) const;

	// Resolves semantic tags into a stable replay color classification.
	ELidarRayReplayClassification ResolveClassificationFromTags(
		const TArray<FName>& Tags,
		const FDeliveryBotLidarRayInfo& RayInfo) const;

	// Resolves a name or tag token into a stable replay color classification.
	ELidarRayReplayClassification ResolveClassificationFromText(
		const FString& Text,
		const FDeliveryBotLidarRayInfo& RayInfo) const;

	// Builds the manifest that describes the buffered ray frames.
	FEpisodeLidarRayReplayManifest BuildManifest() const;

	// Absolute episode directory that will receive replay/lidar_rays artifacts.
	FString EpisodeDirectory;

	// Absolute LiDAR ray replay artifact directory for the active session.
	FString LidarRayReplayDirectory;

	// Buffered variable-length LiDAR ray frames for the active episode.
	TArray<FEpisodeLidarRayFrame> Frames;

	// First world-time sample used as LiDAR ray replay time zero.
	double StartWorldTimeSeconds = 0.0;

	// Last sensor sequence accepted by the recorder.
	int32 LastRecordedSensorSequence = INDEX_NONE;

	// True after Open succeeds and before Close or Abort completes.
	bool bOpen = false;

	// True after the first accepted sensor frame establishes StartWorldTimeSeconds.
	bool bHasStartTime = false;
};
