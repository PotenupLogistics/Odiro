#pragma once

#include "CoreMinimal.h"
#include "EpisodeLidarRayReplayDataTypes.generated.h"

namespace EpisodeLidarRayReplayV1
{
	// Schema version supported by the first LiDAR ray replay artifact.
	constexpr int32 Version = 1;
	// Binary endianness marker used to reject unsupported ray replay files.
	constexpr uint16 EndianMarker = 0x1234;
	// Fixed binary header byte size before variable-length ray frames.
	constexpr int32 BinaryHeaderSizeBytes = 22;
	// Fixed byte size for one frame prefix before its variable ray payload.
	constexpr int32 FrameHeaderSizeBytes = 12;
	// Fixed byte size for one serialized LiDAR ray sample.
	constexpr int32 RaySizeBytes = 56;
}

// Replay-time semantic bucket assigned to one recorded LiDAR ray.
UENUM(BlueprintType)
enum class ELidarRayReplayClassification : uint8
{
	Miss,
	Ground,
	Wall,
	Obstacle,
	Unknown
};

// One LiDAR ray sample stored in a replay frame.
USTRUCT(BlueprintType)
struct ODIROSIM_API FEpisodeLidarRaySample
{
	GENERATED_BODY()

	// Source LiDAR ray dimension encoded from EDeliveryBotLidarRayDimensionType.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	uint8 DimensionType = 0;

	// Semantic replay color bucket resolved when the ray is recorded.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	ELidarRayReplayClassification Classification = ELidarRayReplayClassification::Unknown;

	// True when the ray hit a blocking target in the source simulation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	bool bHit = false;

	// Source scan ray index.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	int32 RayIndex = INDEX_NONE;

	// Source ray yaw angle in degrees.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	float RayYawDegree = 0.0f;

	// Source ray pitch angle in degrees.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	float RayPitchDegree = 0.0f;

	// Source measured distance in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	float DistanceM = 0.0f;

	// Source simulation world-space start position in centimeters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	FVector StartLocationCm = FVector::ZeroVector;

	// Source simulation world-space ray end position in centimeters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	FVector EndLocationCm = FVector::ZeroVector;

	// Source simulation world-space hit position in centimeters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	FVector HitLocationCm = FVector::ZeroVector;

	// Returns true when the ray can be serialized safely.
	bool IsValidRay(TArray<FString>& OutDiagnostics) const;
};

// Variable-length LiDAR ray frame keyed by episode-relative sensor time.
USTRUCT(BlueprintType)
struct ODIROSIM_API FEpisodeLidarRayFrame
{
	GENERATED_BODY()

	// Episode-relative sensor frame time in seconds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	float TimeSeconds = 0.0f;

	// DeliveryBot sensor snapshot sequence that produced this frame.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	int32 SensorSequence = 0;

	// LiDAR rays captured for this sensor frame.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	TArray<FEpisodeLidarRaySample> Rays;

	// Returns true when the frame can be serialized safely.
	bool IsValidFrame(TArray<FString>& OutDiagnostics) const;
};

// Manifest for one LiDAR ray replay artifact set.
USTRUCT(BlueprintType)
struct ODIROSIM_API FEpisodeLidarRayReplayManifest
{
	GENERATED_BODY()

	// Manifest schema identifier.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	FString Schema = TEXT("episode_lidar_rays");

	// Manifest schema version.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	int32 Version = EpisodeLidarRayReplayV1::Version;

	// Binary frame file name relative to the ray manifest directory.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	FString FrameFile = TEXT("rays.frames.bin");

	// Runtime source that produced the ray frames.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	FString Source = TEXT("deliverybot_sensor_snapshot");

	// Coordinate frame stored in the binary file.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	FString CoordinateFrame = TEXT("unreal_world_cm");

	// Number of sensor frames stored in the binary file.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	int32 FrameCount = 0;

	// Total number of ray samples stored across all frames.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	int32 TotalRayCount = 0;

	// First DeliveryBot sensor sequence stored in the binary file.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	int32 FirstSensorSequence = 0;

	// Last DeliveryBot sensor sequence stored in the binary file.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	int32 LastSensorSequence = 0;

	// First episode-relative sensor frame time in seconds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	double FirstTimeSeconds = 0.0;

	// Last episode-relative sensor frame time in seconds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	double LastTimeSeconds = 0.0;

	// Color used for miss rays in replay review.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	FColor MissColor = FColor(120, 120, 120);

	// Color used for ground rays in replay review.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	FColor GroundColor = FColor(120, 120, 120);

	// Color used for wall rays in replay review.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	FColor WallColor = FColor(80, 180, 255);

	// Color used for obstacle rays in replay review.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	FColor ObstacleColor = FColor(255, 80, 60);

	// Color used for unclassified hit rays in replay review.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay|LiDAR")
	FColor UnknownColor = FColor(160, 120, 255);

	// Returns true when the manifest matches the V1 ray replay contract.
	bool IsValidManifest(TArray<FString>& OutDiagnostics) const;
};

// JSON reader and writer for rays.meta.json.
struct ODIROSIM_API FEpisodeLidarRayReplayManifestJson
{
	// Writes a formatted LiDAR ray replay manifest JSON file.
	static bool SaveToFile(
		const FString& ManifestPath,
		const FEpisodeLidarRayReplayManifest& Manifest,
		TArray<FString>& OutDiagnostics);

	// Reads and validates a LiDAR ray replay manifest JSON file.
	static bool LoadFromFile(
		const FString& ManifestPath,
		FEpisodeLidarRayReplayManifest& OutManifest,
		TArray<FString>& OutDiagnostics);
};

// Variable-frame binary reader and writer for rays.frames.bin.
struct ODIROSIM_API FEpisodeLidarRayReplayBinary
{
	// Writes all LiDAR ray replay frames to a variable-frame binary file.
	static bool SaveFramesToFile(
		const FString& FramePath,
		const TArray<FEpisodeLidarRayFrame>& Frames,
		TArray<FString>& OutDiagnostics);

	// Reads and validates all LiDAR ray replay frames from a binary file.
	static bool LoadFramesFromFile(
		const FString& FramePath,
		TArray<FEpisodeLidarRayFrame>& OutFrames,
		TArray<FString>& OutDiagnostics);

	// Resolves the nearest LiDAR ray frame index for a requested replay time.
	static int32 ResolveFrameIndex(
		double TimeSeconds,
		const TArray<FEpisodeLidarRayFrame>& Frames);
};
