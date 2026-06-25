#include "Shared/EpisodeReplayDataTypes.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/MemoryReader.h"

namespace
{
	// Fixed magic prefix for replay.frames.bin.
	constexpr uint8 ReplayMagicBytes[8] = { 'O', 'D', 'R', 'R', 'E', 'P', '1', 0 };

	// Converts a double-backed UE vector component to the V1 float payload.
	float ToReplayFloat(const double Value)
	{
		return static_cast<float>(Value);
	}

	// Returns true when all vector components are finite.
	bool IsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X)
			&& FMath::IsFinite(Value.Y)
			&& FMath::IsFinite(Value.Z);
	}

	// Returns true when all quaternion components are finite.
	bool IsFiniteQuat(const FQuat& Value)
	{
		return FMath::IsFinite(Value.X)
			&& FMath::IsFinite(Value.Y)
			&& FMath::IsFinite(Value.Z)
			&& FMath::IsFinite(Value.W);
	}

	// Serializes the fixed V1 binary header.
	void SerializeReplayHeader(FArchive& Archive, FEpisodeReplayBinaryHeader& Header)
	{
		uint8 MagicBytes[UE_ARRAY_COUNT(ReplayMagicBytes)] = {};
		if (Archive.IsSaving())
		{
			FMemory::Memcpy(MagicBytes, ReplayMagicBytes, UE_ARRAY_COUNT(ReplayMagicBytes));
		}

		Archive.Serialize(MagicBytes, UE_ARRAY_COUNT(MagicBytes));
		if (Archive.IsLoading()
			&& FMemory::Memcmp(MagicBytes, ReplayMagicBytes, UE_ARRAY_COUNT(ReplayMagicBytes)) != 0)
		{
			Archive.SetError();
			return;
		}

		uint16 Version = static_cast<uint16>(Header.Version);
		uint16 EndianMarker = EpisodeReplayV1::EndianMarker;
		uint32 FrameCount = static_cast<uint32>(FMath::Max(Header.FrameCount, 0));
		uint32 FrameSizeBytes = static_cast<uint32>(Header.FrameSizeBytes);
		float SampleRateHz = Header.SampleRateHz;
		uint32 FirstFrameOffsetBytes = static_cast<uint32>(Header.FirstFrameOffsetBytes);

		Archive << Version;
		Archive << EndianMarker;
		Archive << FrameCount;
		Archive << FrameSizeBytes;
		Archive << SampleRateHz;
		Archive << FirstFrameOffsetBytes;

		if (Archive.IsLoading())
		{
			if (EndianMarker != EpisodeReplayV1::EndianMarker)
			{
				Archive.SetError();
				return;
			}

			Header.Version = static_cast<int32>(Version);
			Header.FrameCount = static_cast<int32>(FrameCount);
			Header.FrameSizeBytes = static_cast<int32>(FrameSizeBytes);
			Header.SampleRateHz = SampleRateHz;
			Header.FirstFrameOffsetBytes = static_cast<int32>(FirstFrameOffsetBytes);
		}
	}

	// Serializes one fixed-size V1 robot replay frame.
	void SerializeReplayFrame(FArchive& Archive, FEpisodeReplayRobotFrame& Frame)
	{
		float TimeSeconds = Frame.TimeSeconds;
		float PositionX = ToReplayFloat(Frame.PositionCm.X);
		float PositionY = ToReplayFloat(Frame.PositionCm.Y);
		float PositionZ = ToReplayFloat(Frame.PositionCm.Z);
		float RotationX = ToReplayFloat(Frame.Rotation.X);
		float RotationY = ToReplayFloat(Frame.Rotation.Y);
		float RotationZ = ToReplayFloat(Frame.Rotation.Z);
		float RotationW = ToReplayFloat(Frame.Rotation.W);
		float VelocityX = ToReplayFloat(Frame.VelocityCmPerSecond.X);
		float VelocityY = ToReplayFloat(Frame.VelocityCmPerSecond.Y);
		float VelocityZ = ToReplayFloat(Frame.VelocityCmPerSecond.Z);
		float SpeedKmh = Frame.SpeedKmh;
		float Steering = Frame.Steering;
		float Throttle = Frame.Throttle;
		float Brake = Frame.Brake;
		float TargetSpeedKmh = Frame.TargetSpeedKmh;
		uint8 Direction = static_cast<uint8>(Frame.Direction);
		uint8 Reserved0 = 0;
		uint8 Reserved1 = 0;
		uint8 Reserved2 = 0;

		Archive << TimeSeconds;
		Archive << PositionX;
		Archive << PositionY;
		Archive << PositionZ;
		Archive << RotationX;
		Archive << RotationY;
		Archive << RotationZ;
		Archive << RotationW;
		Archive << VelocityX;
		Archive << VelocityY;
		Archive << VelocityZ;
		Archive << SpeedKmh;
		Archive << Steering;
		Archive << Throttle;
		Archive << Brake;
		Archive << TargetSpeedKmh;
		Archive << Direction;
		Archive << Reserved0;
		Archive << Reserved1;
		Archive << Reserved2;

		if (Archive.IsLoading())
		{
			Frame.TimeSeconds = TimeSeconds;
			Frame.PositionCm = FVector(PositionX, PositionY, PositionZ);
			Frame.Rotation = FQuat(RotationX, RotationY, RotationZ, RotationW).GetNormalized();
			Frame.VelocityCmPerSecond = FVector(VelocityX, VelocityY, VelocityZ);
			Frame.SpeedKmh = SpeedKmh;
			Frame.Steering = Steering;
			Frame.Throttle = Throttle;
			Frame.Brake = Brake;
			Frame.TargetSpeedKmh = TargetSpeedKmh;
			Frame.Direction = static_cast<EEpisodeReplayDirection>(Direction);
		}
	}

	// Adds a diagnostic message and returns false for compact validation branches.
	bool AddReplayDiagnostic(TArray<FString>& OutDiagnostics, const FString& Message)
	{
		OutDiagnostics.Add(Message);
		return false;
	}

	// Serializes a JSON object to formatted UTF-8 text.
	bool SerializeJsonObject(const TSharedRef<FJsonObject>& Object, FString& OutJson)
	{
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
		return FJsonSerializer::Serialize(Object, Writer);
	}

	// Reads a nested object field if it exists and is an object.
	bool TryGetObjectField(
		const FJsonObject& Object,
		const FString& FieldName,
		TSharedPtr<FJsonObject>& OutObject)
	{
		const TSharedPtr<FJsonObject>* ObjectPtr = nullptr;
		if (!Object.TryGetObjectField(FieldName, ObjectPtr) || ObjectPtr == nullptr || !ObjectPtr->IsValid())
		{
			OutObject.Reset();
			return false;
		}

		OutObject = *ObjectPtr;
		return true;
	}
}

bool FEpisodeReplayRobotFrame::IsValidFrame() const
{
	return FMath::IsFinite(TimeSeconds)
		&& TimeSeconds >= 0.0f
		&& IsFiniteVector(PositionCm)
		&& IsFiniteQuat(Rotation)
		&& IsFiniteVector(VelocityCmPerSecond)
		&& FMath::IsFinite(SpeedKmh)
		&& FMath::IsFinite(Steering)
		&& FMath::IsFinite(Throttle)
		&& FMath::IsFinite(Brake)
		&& FMath::IsFinite(TargetSpeedKmh);
}

bool FEpisodeReplayBinaryHeader::IsValidHeader(TArray<FString>& OutDiagnostics) const
{
	bool bValid = true;
	if (Version != EpisodeReplayV1::Version)
	{
		bValid = AddReplayDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Unsupported replay binary version: %d"), Version));
	}

	if (FrameCount < 0)
	{
		bValid = AddReplayDiagnostic(OutDiagnostics, TEXT("Replay binary frame count must not be negative."));
	}

	if (FrameSizeBytes != EpisodeReplayV1::FixedFrameSizeBytes)
	{
		bValid = AddReplayDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Replay binary frame size mismatch: %d"), FrameSizeBytes));
	}

	if (!FMath::IsFinite(SampleRateHz) || SampleRateHz <= 0.0f)
	{
		bValid = AddReplayDiagnostic(OutDiagnostics, TEXT("Replay binary sample rate must be positive."));
	}

	if (FirstFrameOffsetBytes != EpisodeReplayV1::BinaryHeaderSizeBytes)
	{
		bValid = AddReplayDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Replay binary first frame offset mismatch: %d"), FirstFrameOffsetBytes));
	}

	return bValid;
}

bool FEpisodeReplayManifest::IsValidManifest(TArray<FString>& OutDiagnostics) const
{
	bool bValid = true;
	if (!Schema.Equals(TEXT("episode_replay"), ESearchCase::CaseSensitive))
	{
		bValid = AddReplayDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Unsupported replay manifest schema: %s"), *Schema));
	}

	if (Version != EpisodeReplayV1::Version)
	{
		bValid = AddReplayDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Unsupported replay manifest version: %d"), Version));
	}

	if (FrameFile.TrimStartAndEnd().IsEmpty())
	{
		bValid = AddReplayDiagnostic(OutDiagnostics, TEXT("Replay manifest frame_file must not be empty."));
	}

	if (FrameCount <= 0)
	{
		bValid = AddReplayDiagnostic(OutDiagnostics, TEXT("Replay manifest frame_count must be positive."));
	}

	if (!FMath::IsFinite(DurationSeconds) || DurationSeconds < 0.0)
	{
		bValid = AddReplayDiagnostic(OutDiagnostics, TEXT("Replay manifest duration_s must be non-negative."));
	}

	if (!FMath::IsFinite(SampleRateHz) || SampleRateHz <= 0.0)
	{
		bValid = AddReplayDiagnostic(OutDiagnostics, TEXT("Replay manifest sample_rate_hz must be positive."));
	}

	if (FrameSizeBytes != EpisodeReplayV1::FixedFrameSizeBytes)
	{
		bValid = AddReplayDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Replay manifest frame_size_bytes mismatch: %d"), FrameSizeBytes));
	}

	if (FirstFrameOffsetBytes != EpisodeReplayV1::BinaryHeaderSizeBytes)
	{
		bValid = AddReplayDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Replay manifest first_frame_offset_bytes mismatch: %d"), FirstFrameOffsetBytes));
	}

	if (!Features.bRobotBody)
	{
		bValid = AddReplayDiagnostic(OutDiagnostics, TEXT("Replay V1 requires robot_body frames."));
	}

	if (Features.bWheels || Features.bMovingActors)
	{
		bValid = AddReplayDiagnostic(OutDiagnostics, TEXT("Replay V1 does not support wheels or moving actors."));
	}

	return bValid;
}

bool FEpisodeReplayManifestJson::SaveToFile(
	const FString& ManifestPath,
	const FEpisodeReplayManifest& Manifest,
	TArray<FString>& OutDiagnostics)
{
	OutDiagnostics.Reset();
	TArray<FString> ValidationDiagnostics;
	if (!Manifest.IsValidManifest(ValidationDiagnostics))
	{
		OutDiagnostics.Append(ValidationDiagnostics);
		return false;
	}

	const FString ManifestDirectory = FPaths::GetPath(ManifestPath);
	if (!ManifestDirectory.IsEmpty() && !IFileManager::Get().MakeDirectory(*ManifestDirectory, true))
	{
		return AddReplayDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Failed to create replay manifest directory: %s"), *ManifestDirectory));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("schema"), Manifest.Schema);
	RootObject->SetNumberField(TEXT("version"), Manifest.Version);
	RootObject->SetStringField(TEXT("frame_file"), Manifest.FrameFile);

	const TSharedRef<FJsonObject> SourceObject = MakeShared<FJsonObject>();
	SourceObject->SetStringField(TEXT("scenario_sample"), Manifest.ScenarioSample);
	SourceObject->SetStringField(TEXT("scenario_hash"), Manifest.ScenarioHash);
	RootObject->SetObjectField(TEXT("source"), SourceObject);

	const TSharedRef<FJsonObject> TimelineObject = MakeShared<FJsonObject>();
	TimelineObject->SetNumberField(TEXT("duration_s"), Manifest.DurationSeconds);
	TimelineObject->SetNumberField(TEXT("frame_count"), Manifest.FrameCount);
	TimelineObject->SetNumberField(TEXT("sample_rate_hz"), Manifest.SampleRateHz);
	TimelineObject->SetNumberField(TEXT("fixed_frame_size_bytes"), Manifest.FrameSizeBytes);
	RootObject->SetObjectField(TEXT("timeline"), TimelineObject);

	const TSharedRef<FJsonObject> FeaturesObject = MakeShared<FJsonObject>();
	FeaturesObject->SetBoolField(TEXT("robot_body"), Manifest.Features.bRobotBody);
	FeaturesObject->SetBoolField(TEXT("control"), Manifest.Features.bControl);
	FeaturesObject->SetBoolField(TEXT("wheels"), Manifest.Features.bWheels);
	FeaturesObject->SetBoolField(TEXT("moving_actors"), Manifest.Features.bMovingActors);
	RootObject->SetObjectField(TEXT("features"), FeaturesObject);

	const TSharedRef<FJsonObject> IndexObject = MakeShared<FJsonObject>();
	IndexObject->SetStringField(TEXT("type"), TEXT("fixed_size"));
	IndexObject->SetNumberField(TEXT("first_frame_offset_bytes"), Manifest.FirstFrameOffsetBytes);
	IndexObject->SetNumberField(TEXT("frame_size_bytes"), Manifest.FrameSizeBytes);
	RootObject->SetObjectField(TEXT("index"), IndexObject);

	FString JsonString;
	if (!SerializeJsonObject(RootObject, JsonString))
	{
		return AddReplayDiagnostic(OutDiagnostics, TEXT("Failed to serialize replay manifest JSON."));
	}

	if (!FFileHelper::SaveStringToFile(JsonString, *ManifestPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		return AddReplayDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Failed to write replay manifest: %s"), *ManifestPath));
	}

	return true;
}

bool FEpisodeReplayManifestJson::LoadFromFile(
	const FString& ManifestPath,
	FEpisodeReplayManifest& OutManifest,
	TArray<FString>& OutDiagnostics)
{
	OutDiagnostics.Reset();
	OutManifest = FEpisodeReplayManifest{};

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *ManifestPath))
	{
		return AddReplayDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Failed to read replay manifest: %s"), *ManifestPath));
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		return AddReplayDiagnostic(OutDiagnostics, TEXT("Failed to parse replay manifest JSON."));
	}

	OutManifest.Schema = RootObject->GetStringField(TEXT("schema"));
	OutManifest.Version = static_cast<int32>(RootObject->GetIntegerField(TEXT("version")));
	OutManifest.FrameFile = RootObject->GetStringField(TEXT("frame_file"));

	TSharedPtr<FJsonObject> SourceObject;
	if (TryGetObjectField(*RootObject, TEXT("source"), SourceObject))
	{
		OutManifest.ScenarioSample = SourceObject->GetStringField(TEXT("scenario_sample"));
		OutManifest.ScenarioHash = SourceObject->GetStringField(TEXT("scenario_hash"));
	}

	TSharedPtr<FJsonObject> TimelineObject;
	if (TryGetObjectField(*RootObject, TEXT("timeline"), TimelineObject))
	{
		OutManifest.DurationSeconds = TimelineObject->GetNumberField(TEXT("duration_s"));
		OutManifest.FrameCount = static_cast<int32>(TimelineObject->GetIntegerField(TEXT("frame_count")));
		OutManifest.SampleRateHz = TimelineObject->GetNumberField(TEXT("sample_rate_hz"));
		OutManifest.FrameSizeBytes = static_cast<int32>(TimelineObject->GetIntegerField(TEXT("fixed_frame_size_bytes")));
	}

	TSharedPtr<FJsonObject> FeaturesObject;
	if (TryGetObjectField(*RootObject, TEXT("features"), FeaturesObject))
	{
		OutManifest.Features.bRobotBody = FeaturesObject->GetBoolField(TEXT("robot_body"));
		OutManifest.Features.bControl = FeaturesObject->GetBoolField(TEXT("control"));
		OutManifest.Features.bWheels = FeaturesObject->GetBoolField(TEXT("wheels"));
		OutManifest.Features.bMovingActors = FeaturesObject->GetBoolField(TEXT("moving_actors"));
	}

	TSharedPtr<FJsonObject> IndexObject;
	if (TryGetObjectField(*RootObject, TEXT("index"), IndexObject))
	{
		OutManifest.FirstFrameOffsetBytes =
			static_cast<int32>(IndexObject->GetIntegerField(TEXT("first_frame_offset_bytes")));
		OutManifest.FrameSizeBytes =
			static_cast<int32>(IndexObject->GetIntegerField(TEXT("frame_size_bytes")));
	}

	return OutManifest.IsValidManifest(OutDiagnostics);
}

bool FEpisodeReplayBinary::SaveFramesToFile(
	const FString& FramePath,
	const TArray<FEpisodeReplayRobotFrame>& Frames,
	TArray<FString>& OutDiagnostics)
{
	OutDiagnostics.Reset();
	if (Frames.IsEmpty())
	{
		return AddReplayDiagnostic(OutDiagnostics, TEXT("Replay binary writer requires at least one frame."));
	}

	const FString FrameDirectory = FPaths::GetPath(FramePath);
	if (!FrameDirectory.IsEmpty() && !IFileManager::Get().MakeDirectory(*FrameDirectory, true))
	{
		return AddReplayDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Failed to create replay binary directory: %s"), *FrameDirectory));
	}

	FBufferArchive Archive;
	FEpisodeReplayBinaryHeader Header;
	Header.FrameCount = Frames.Num();
	SerializeReplayHeader(Archive, Header);

	if (Archive.IsError() || Archive.Num() != EpisodeReplayV1::BinaryHeaderSizeBytes)
	{
		return AddReplayDiagnostic(OutDiagnostics, TEXT("Replay binary header serialization failed."));
	}

	for (const FEpisodeReplayRobotFrame& SourceFrame : Frames)
	{
		if (!SourceFrame.IsValidFrame())
		{
			return AddReplayDiagnostic(OutDiagnostics, TEXT("Replay binary writer rejected an invalid frame."));
		}

		const int64 FrameStartOffset = Archive.Num();
		FEpisodeReplayRobotFrame Frame = SourceFrame;
		SerializeReplayFrame(Archive, Frame);
		const int64 FrameByteCount = Archive.Num() - FrameStartOffset;
		if (FrameByteCount != EpisodeReplayV1::FixedFrameSizeBytes)
		{
			return AddReplayDiagnostic(
				OutDiagnostics,
				FString::Printf(TEXT("Replay frame serialized to %lld bytes."), FrameByteCount));
		}
	}

	if (!FFileHelper::SaveArrayToFile(Archive, *FramePath))
	{
		return AddReplayDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Failed to write replay binary: %s"), *FramePath));
	}

	return true;
}

bool FEpisodeReplayBinary::LoadFramesFromFile(
	const FString& FramePath,
	TArray<FEpisodeReplayRobotFrame>& OutFrames,
	FEpisodeReplayBinaryHeader& OutHeader,
	TArray<FString>& OutDiagnostics)
{
	OutDiagnostics.Reset();
	OutFrames.Reset();
	OutHeader = FEpisodeReplayBinaryHeader{};

	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *FramePath))
	{
		return AddReplayDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Failed to read replay binary: %s"), *FramePath));
	}

	FMemoryReader Reader(Bytes, true);
	SerializeReplayHeader(Reader, OutHeader);
	if (Reader.IsError())
	{
		return AddReplayDiagnostic(OutDiagnostics, TEXT("Replay binary header magic or endian marker is invalid."));
	}

	TArray<FString> HeaderDiagnostics;
	if (!OutHeader.IsValidHeader(HeaderDiagnostics))
	{
		OutDiagnostics.Append(HeaderDiagnostics);
		return false;
	}

	const int64 ExpectedSize =
		static_cast<int64>(OutHeader.FirstFrameOffsetBytes)
		+ static_cast<int64>(OutHeader.FrameCount) * static_cast<int64>(OutHeader.FrameSizeBytes);
	if (Bytes.Num() != ExpectedSize)
	{
		return AddReplayDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Replay binary size mismatch. Expected %lld bytes, found %d bytes."),
				ExpectedSize,
				Bytes.Num()));
	}

	OutFrames.Reserve(OutHeader.FrameCount);
	for (int32 FrameIndex = 0; FrameIndex < OutHeader.FrameCount; ++FrameIndex)
	{
		const int64 FrameStartOffset = Reader.Tell();
		FEpisodeReplayRobotFrame Frame;
		SerializeReplayFrame(Reader, Frame);
		if (Reader.IsError())
		{
			return AddReplayDiagnostic(
				OutDiagnostics,
				FString::Printf(TEXT("Replay binary frame %d failed to deserialize."), FrameIndex));
		}

		const int64 FrameByteCount = Reader.Tell() - FrameStartOffset;
		if (FrameByteCount != OutHeader.FrameSizeBytes)
		{
			return AddReplayDiagnostic(
				OutDiagnostics,
				FString::Printf(TEXT("Replay binary frame %d byte size mismatch."), FrameIndex));
		}

		if (!Frame.IsValidFrame())
		{
			return AddReplayDiagnostic(
				OutDiagnostics,
				FString::Printf(TEXT("Replay binary frame %d is invalid."), FrameIndex));
		}

		OutFrames.Add(Frame);
	}

	return true;
}

int32 FEpisodeReplayBinary::ResolveFrameIndex(
	double TimeSeconds,
	const FEpisodeReplayManifest& Manifest)
{
	if (Manifest.FrameCount <= 0 || Manifest.SampleRateHz <= 0.0)
	{
		return INDEX_NONE;
	}

	const int32 RawIndex = FMath::RoundToInt(TimeSeconds * Manifest.SampleRateHz);
	return FMath::Clamp(RawIndex, 0, Manifest.FrameCount - 1);
}
