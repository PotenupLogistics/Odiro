#pragma once

#include "CoreMinimal.h"
#include "Shared/Struct/DeliveryBot/Perception/DeliveryBotLidarSensorInfo.h"
#include "Shared/Struct/DeliveryBot/Perception/DeliveryBotPointCloudCaptureConfigInfo.h"

// Writes full-resolution LiDAR hit points into the standard episode point-cloud artifact layout.
class ODIROSIM_API FEpisodeFullLidarPointCloudRecorder
{
public:
	// Opens a full point-cloud recording session for one episode output directory.
	bool Open(
		const FString& InEpisodeDirectory,
		TArray<FString>& OutDiagnostics);

	// Adds one full LiDAR point-cloud frame when capture settings and sensor sequence allow it.
	bool RecordSensorSnapshot(
		double RunTimeSeconds,
		int32 SensorSequence,
		const FDeliveryBotLidarScanInfo& ScanInfo,
		const FDeliveryBotPointCloudCaptureConfigInfo& CaptureConfigInfo,
		TArray<FString>& OutDiagnostics);

	// Writes lidar_point_cloud artifacts for all buffered full-resolution frames.
	bool Close(TArray<FString>& OutDiagnostics);

	// Discards buffered point-cloud data without writing final artifacts.
	void Abort();

	// Returns true while the recorder owns an open episode session.
	bool IsOpen() const { return bOpen; }

	// Returns the number of buffered full point-cloud frames awaiting final write.
	int32 GetFrameCount() const { return Frames.Num(); }

private:
	// One buffered full point-cloud frame.
	struct FBufferedFrame
	{
		// Replay-time position of the captured sensor frame.
		double RunTimeSeconds = 0.0;

		// Source LiDAR sensor sequence.
		int32 SensorSequence = INDEX_NONE;

		// XYZRGB lines in replay map-local import coordinates.
		TArray<FString> ImportLines;
	};

	// Converts one runtime hit ray into an XYZRGB map-local line.
	bool TryBuildPointLines(
		const FDeliveryBotLidarRayInfo& RayInfo,
		const FDeliveryBotPointCloudCaptureConfigInfo& CaptureConfigInfo,
		FString& OutImportLine,
		FString& OutWorldLine) const;

	// Resolves one runtime sensor ray into a stable point-cloud classification name.
	FString ResolvePointClassification(const FDeliveryBotLidarRayInfo& RayInfo) const;

	// Resolves semantic tags into a stable point-cloud classification name.
	FString ResolveClassificationFromTags(
		const TArray<FName>& Tags,
		const FDeliveryBotLidarRayInfo& RayInfo) const;

	// Resolves a name or tag token into a stable point-cloud classification name.
	FString ResolveClassificationFromText(
		const FString& Text,
		const FDeliveryBotLidarRayInfo& RayInfo) const;

	// Returns the RGB color used by the existing Python point-cloud capture for a classification.
	FColor ResolvePointColor(const FString& Classification) const;

	// Writes the accumulated map-local and world-coordinate point files.
	bool WriteAccumulatedFiles(TArray<FString>& OutDiagnostics) const;

	// Writes per-frame XYZ files and their frames.jsonl index.
	bool WriteFrameFiles(TArray<FString>& OutDiagnostics) const;

	// Writes manifest.json and capture_summary.json metadata used by replay import.
	bool WriteMetadataFiles(TArray<FString>& OutDiagnostics) const;

	// Absolute episode directory that will receive lidar_point_cloud artifacts.
	FString EpisodeDirectory;

	// Absolute point-cloud artifact directory for the active session.
	FString PointCloudDirectory;

	// Absolute point-cloud frame directory for the active session.
	FString FramesDirectory;

	// Buffered full point-cloud frames for the active episode.
	TArray<FBufferedFrame> Frames;

	// Buffered accumulated map-local XYZRGB lines.
	TArray<FString> AccumulatedImportLines;

	// Buffered accumulated Unreal world-coordinate XYZRGB lines.
	TArray<FString> AccumulatedWorldLines;

	// Last sensor sequence accepted by the recorder.
	int32 LastRecordedSensorSequence = INDEX_NONE;

	// First sensor sequence accepted by the recorder.
	int32 FirstRecordedSensorSequence = INDEX_NONE;

	// Last sensor sequence written by the recorder.
	int32 LastWrittenSensorSequence = INDEX_NONE;

	// First replay-time sample written by the recorder.
	double FirstRunTimeSeconds = 0.0;

	// Last replay-time sample written by the recorder.
	double LastRunTimeSeconds = 0.0;

	// True after Open succeeds and before Close or Abort completes.
	bool bOpen = false;
};
