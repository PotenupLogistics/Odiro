#pragma once

#include "CoreMinimal.h"
#include "EpisodeReplayDataTypes.generated.h"

namespace EpisodeReplayV1
{
	// Schema version supported by the first embedded replay implementation.
	constexpr int32 Version = 1;
	// Fixed binary frame byte size for V1 robot-body frames.
	constexpr int32 FixedFrameSizeBytes = 68;
	// Fixed binary header byte size before the first frame.
	constexpr int32 BinaryHeaderSizeBytes = 28;
	// Default sampling rate used by the recorder and manifest.
	constexpr double SampleRateHz = 30.0;
	// Binary endianness marker used to reject unsupported replay files.
	constexpr uint16 EndianMarker = 0x1234;
}

namespace EpisodeReplayV2
{
	// Schema version that adds replay-only wheel visual poses.
	constexpr int32 Version = 2;
	// Fixed wheel count used by the DeliveryBot replay visual rig.
	constexpr int32 WheelCount = 4;
	// Serialized byte count for one wheel visual pose.
	constexpr int32 FixedWheelFrameSizeBytes = 32;
	// Fixed binary frame byte size for V2 robot body and wheel visual frames.
	constexpr int32 FixedFrameSizeBytes =
		EpisodeReplayV1::FixedFrameSizeBytes
		+ WheelCount * FixedWheelFrameSizeBytes;
}

// Direction state stored with a replay frame for visual playback and overlays.
UENUM(BlueprintType)
enum class EEpisodeReplayDirection : uint8
{
	Stopped,
	Forward,
	Reverse
};

// Fixed wheel slot order used by replay recording and visual playback.
UENUM(BlueprintType)
enum class EEpisodeReplayWheelSlot : uint8
{
	FrontLeft,
	FrontRight,
	RearLeft,
	RearRight
};

// Playback lifecycle state exposed to temporary debug UI and later formal UI.
UENUM(BlueprintType)
enum class EScenarioReplayPlaybackState : uint8
{
	Stopped,
	Loading,
	Ready,
	Playing,
	Paused,
	Failed
};

// Per-frame wheel visual pose recorded from the runtime DeliveryBot.
USTRUCT(BlueprintType)
struct ODIROSIM_API FEpisodeReplayWheelFrame
{
	GENERATED_BODY()

	// Wheel component local position relative to the robot actor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	FVector LocalLocationCm = FVector::ZeroVector;

	// Wheel component local rotation relative to the robot actor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	FQuat LocalRotation = FQuat::Identity;

	// True when a later recorder can prove this wheel is touching the ground.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	bool bInContact = false;

	// True when a runtime visual component was found for this wheel.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	bool bHasVisualPose = false;

	// Returns true when the wheel visual pose can be serialized safely.
	bool IsValidFrame() const;
};

// Per-frame robot body, control state, and optional Replay V2 wheel visual state.
USTRUCT(BlueprintType)
struct ODIROSIM_API FEpisodeReplayRobotFrame
{
	GENERATED_BODY()

	// Episode-relative playback time in seconds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	float TimeSeconds = 0.0f;

	// Robot body world position in centimeters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	FVector PositionCm = FVector::ZeroVector;

	// Robot body world rotation as a normalized quaternion.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	FQuat Rotation = FQuat::Identity;

	// Robot body linear velocity in centimeters per second.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	FVector VelocityCmPerSecond = FVector::ZeroVector;

	// Derived robot speed in kilometers per hour.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	float SpeedKmh = 0.0f;

	// Applied or requested steering input in the -1..1 range.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	float Steering = 0.0f;

	// Applied throttle input in the 0..1 range when available.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	float Throttle = 0.0f;

	// Applied or requested brake input in the 0..1 range.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	float Brake = 0.0f;

	// Requested or smoothed target speed in kilometers per hour.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	float TargetSpeedKmh = 0.0f;

	// Requested movement direction for the recorded frame.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	EEpisodeReplayDirection Direction = EEpisodeReplayDirection::Stopped;

	// Optional fixed-order wheel visual poses; present for Replay V2 frames.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	TArray<FEpisodeReplayWheelFrame> Wheels;

	// Returns true when the frame can be serialized and replayed safely.
	bool IsValidFrame() const;
};

// Binary header mirrored in replay.frames.bin before fixed-size frames.
USTRUCT(BlueprintType)
struct ODIROSIM_API FEpisodeReplayBinaryHeader
{
	GENERATED_BODY()

	// Binary file format version.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	int32 Version = EpisodeReplayV1::Version;

	// Number of fixed-size frames in the file.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	int32 FrameCount = 0;

	// Byte size of one serialized frame.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	int32 FrameSizeBytes = EpisodeReplayV1::FixedFrameSizeBytes;

	// Capture sample rate recorded in the file.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	float SampleRateHz = static_cast<float>(EpisodeReplayV1::SampleRateHz);

	// Byte offset of the first fixed-size frame.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	int32 FirstFrameOffsetBytes = EpisodeReplayV1::BinaryHeaderSizeBytes;

	// Returns true when the binary header matches the V1 contract.
	bool IsValidHeader(TArray<FString>& OutDiagnostics) const;
};

// Feature switches recorded in the manifest so later versions can extend the payload safely.
USTRUCT(BlueprintType)
struct ODIROSIM_API FEpisodeReplayManifestFeatures
{
	GENERATED_BODY()

	// True when V1 robot body frames are present.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	bool bRobotBody = true;

	// True when control values are present in each frame.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	bool bControl = true;

	// Wheel visual state is intentionally not recorded in V1.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	bool bWheels = false;

	// Moving actor state is intentionally not recorded in V1.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	bool bMovingActors = false;
};

// Manifest for one episode replay artifact set.
USTRUCT(BlueprintType)
struct ODIROSIM_API FEpisodeReplayManifest
{
	GENERATED_BODY()

	// Manifest schema identifier.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	FString Schema = TEXT("episode_replay");

	// Manifest schema version.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	int32 Version = EpisodeReplayV1::Version;

	// Binary frame file name relative to the episode directory.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	FString FrameFile = TEXT("replay.frames.bin");

	// Source scenario sample file name or path recorded for diagnostics.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	FString ScenarioSample = TEXT("scenario-sample.json");

	// Source scenario hash when the runner provides one.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	FString ScenarioHash;

	// Total replay duration in seconds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	double DurationSeconds = 0.0;

	// Number of frames stored in the binary file.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	int32 FrameCount = 0;

	// Sampling rate in hertz.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	double SampleRateHz = EpisodeReplayV1::SampleRateHz;

	// Fixed byte size of one frame.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	int32 FrameSizeBytes = EpisodeReplayV1::FixedFrameSizeBytes;

	// Byte offset where the first frame starts.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	int32 FirstFrameOffsetBytes = EpisodeReplayV1::BinaryHeaderSizeBytes;

	// Feature flags available in this replay file set.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Replay")
	FEpisodeReplayManifestFeatures Features;

	// Returns true when the manifest matches the V1 fixed-size frame contract.
	bool IsValidManifest(TArray<FString>& OutDiagnostics) const;
};

// JSON reader and writer for replay.meta.json.
struct ODIROSIM_API FEpisodeReplayManifestJson
{
	// Writes a formatted manifest JSON file.
	static bool SaveToFile(
		const FString& ManifestPath,
		const FEpisodeReplayManifest& Manifest,
		TArray<FString>& OutDiagnostics);

	// Reads and validates a manifest JSON file.
	static bool LoadFromFile(
		const FString& ManifestPath,
		FEpisodeReplayManifest& OutManifest,
		TArray<FString>& OutDiagnostics);
};

// Fixed-size binary reader and writer for replay.frames.bin.
struct ODIROSIM_API FEpisodeReplayBinary
{
	// Resolves the binary schema version needed for the given frames.
	static int32 ResolveFrameVersion(const TArray<FEpisodeReplayRobotFrame>& Frames);

	// Returns the fixed binary frame size for one supported replay version.
	static int32 GetFrameSizeBytesForVersion(int32 Version);

	// Writes a fixed-size robot replay frame file.
	static bool SaveFramesToFile(
		const FString& FramePath,
		const TArray<FEpisodeReplayRobotFrame>& Frames,
		TArray<FString>& OutDiagnostics);

	// Reads and validates all fixed-size robot replay frames.
	static bool LoadFramesFromFile(
		const FString& FramePath,
		TArray<FEpisodeReplayRobotFrame>& OutFrames,
		FEpisodeReplayBinaryHeader& OutHeader,
		TArray<FString>& OutDiagnostics);

	// Resolves a nearest-frame index for a requested replay time.
	static int32 ResolveFrameIndex(
		double TimeSeconds,
		const FEpisodeReplayManifest& Manifest);
};
