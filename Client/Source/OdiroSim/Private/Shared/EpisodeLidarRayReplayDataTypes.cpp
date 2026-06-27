#include "Shared/EpisodeLidarRayReplayDataTypes.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/MemoryReader.h"

namespace
{
	// Fixed magic prefix for rays.frames.bin.
	constexpr uint8 LidarRayReplayMagicBytes[8] = { 'O', 'D', 'I', 'R', 'A', 'Y', '1', 0 };

	// Small POD header mirrored at the start of rays.frames.bin.
	struct FLidarRayReplayBinaryHeader
	{
		// Binary file format version.
		int32 Version = EpisodeLidarRayReplayV1::Version;

		// Number of variable-length frames in the file.
		int32 FrameCount = 0;

		// Number of ray samples across all frames.
		int32 TotalRayCount = 0;
	};

	// Adds a diagnostic message and returns false for compact validation branches.
	bool AddLidarRayReplayDiagnostic(TArray<FString>& OutDiagnostics, const FString& Message)
	{
		OutDiagnostics.Add(Message);
		return false;
	}

	// Converts a double-backed UE vector component to the V1 float payload.
	float ToLidarRayReplayFloat(const double Value)
	{
		return static_cast<float>(Value);
	}

	// Returns true when all vector components are finite.
	bool IsFiniteLidarRayReplayVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X)
			&& FMath::IsFinite(Value.Y)
			&& FMath::IsFinite(Value.Z);
	}

	// Returns true when the dimension byte matches the known DeliveryBot ray dimension enum range.
	bool IsSupportedDimensionType(const uint8 DimensionType)
	{
		return DimensionType <= 2;
	}

	// Returns true when the classification byte matches ELidarRayReplayClassification.
	bool IsSupportedClassification(const ELidarRayReplayClassification Classification)
	{
		return static_cast<uint8>(Classification) <= static_cast<uint8>(ELidarRayReplayClassification::Unknown);
	}

	// Counts all ray samples stored in a frame array.
	int32 CountRaySamples(const TArray<FEpisodeLidarRayFrame>& Frames)
	{
		int32 TotalRayCount = 0;
		for (const FEpisodeLidarRayFrame& Frame : Frames)
		{
			TotalRayCount += Frame.Rays.Num();
		}

		return TotalRayCount;
	}

	// Serializes a JSON object to formatted UTF-8 text.
	bool SerializeLidarRayReplayJsonObject(const TSharedRef<FJsonObject>& Object, FString& OutJson)
	{
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
		return FJsonSerializer::Serialize(Object, Writer);
	}

	// Creates one RGB JSON array from a replay classification color.
	TArray<TSharedPtr<FJsonValue>> MakeColorArray(const FColor& Color)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		Values.Reserve(3);
		Values.Add(MakeShared<FJsonValueNumber>(Color.R));
		Values.Add(MakeShared<FJsonValueNumber>(Color.G));
		Values.Add(MakeShared<FJsonValueNumber>(Color.B));
		return Values;
	}

	// Reads one RGB JSON array into a replay classification color.
	bool TryReadColorArray(
		const FJsonObject& Object,
		const FString& FieldName,
		FColor& OutColor,
		TArray<FString>& OutDiagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object.TryGetArrayField(FieldName, Values) || Values == nullptr || Values->Num() != 3)
		{
			return AddLidarRayReplayDiagnostic(
				OutDiagnostics,
				FString::Printf(TEXT("LiDAR ray manifest color must be an RGB array: %s"), *FieldName));
		}

		int32 Channels[3] = {};
		for (int32 ChannelIndex = 0; ChannelIndex < 3; ++ChannelIndex)
		{
			double ChannelValue = 0.0;
			if (!(*Values)[ChannelIndex].IsValid() || !(*Values)[ChannelIndex]->TryGetNumber(ChannelValue))
			{
				return AddLidarRayReplayDiagnostic(
					OutDiagnostics,
					FString::Printf(TEXT("LiDAR ray manifest color channel is invalid: %s"), *FieldName));
			}

			Channels[ChannelIndex] = FMath::RoundToInt(ChannelValue);
			if (Channels[ChannelIndex] < 0 || Channels[ChannelIndex] > 255)
			{
				return AddLidarRayReplayDiagnostic(
					OutDiagnostics,
					FString::Printf(TEXT("LiDAR ray manifest color channel is out of range: %s"), *FieldName));
			}
		}

		OutColor = FColor(
			static_cast<uint8>(Channels[0]),
			static_cast<uint8>(Channels[1]),
			static_cast<uint8>(Channels[2]));
		return true;
	}

	// Reads all optional classification colors from a manifest JSON object.
	bool TryReadClassificationColors(
		const FJsonObject& RootObject,
		FEpisodeLidarRayReplayManifest& OutManifest,
		TArray<FString>& OutDiagnostics)
	{
		const TSharedPtr<FJsonObject>* ColorObject = nullptr;
		if (!RootObject.TryGetObjectField(TEXT("classification_colors"), ColorObject))
		{
			return true;
		}

		if (ColorObject == nullptr || !ColorObject->IsValid())
		{
			return AddLidarRayReplayDiagnostic(
				OutDiagnostics,
				TEXT("LiDAR ray manifest classification_colors must be an object."));
		}

		bool bValid = true;
		bValid &= TryReadColorArray(**ColorObject, TEXT("miss"), OutManifest.MissColor, OutDiagnostics);
		bValid &= TryReadColorArray(**ColorObject, TEXT("ground"), OutManifest.GroundColor, OutDiagnostics);
		bValid &= TryReadColorArray(**ColorObject, TEXT("wall"), OutManifest.WallColor, OutDiagnostics);
		bValid &= TryReadColorArray(**ColorObject, TEXT("obstacle"), OutManifest.ObstacleColor, OutDiagnostics);
		bValid &= TryReadColorArray(**ColorObject, TEXT("unknown"), OutManifest.UnknownColor, OutDiagnostics);
		return bValid;
	}

	// Serializes the fixed ray replay binary header.
	void SerializeLidarRayReplayHeader(FArchive& Archive, FLidarRayReplayBinaryHeader& Header)
	{
		uint8 MagicBytes[UE_ARRAY_COUNT(LidarRayReplayMagicBytes)] = {};
		if (Archive.IsSaving())
		{
			FMemory::Memcpy(MagicBytes, LidarRayReplayMagicBytes, UE_ARRAY_COUNT(LidarRayReplayMagicBytes));
		}

		Archive.Serialize(MagicBytes, UE_ARRAY_COUNT(MagicBytes));
		if (Archive.IsLoading()
			&& FMemory::Memcmp(
				MagicBytes,
				LidarRayReplayMagicBytes,
				UE_ARRAY_COUNT(LidarRayReplayMagicBytes)) != 0)
		{
			Archive.SetError();
			return;
		}

		int32 Version = Header.Version;
		uint16 EndianMarker = EpisodeLidarRayReplayV1::EndianMarker;
		int32 FrameCount = Header.FrameCount;
		int32 TotalRayCount = Header.TotalRayCount;

		Archive << Version;
		Archive << EndianMarker;
		Archive << FrameCount;
		Archive << TotalRayCount;

		if (Archive.IsLoading())
		{
			if (EndianMarker != EpisodeLidarRayReplayV1::EndianMarker)
			{
				Archive.SetError();
				return;
			}

			Header.Version = Version;
			Header.FrameCount = FrameCount;
			Header.TotalRayCount = TotalRayCount;
		}
	}

	// Serializes one replay ray sample.
	void SerializeLidarRaySample(FArchive& Archive, FEpisodeLidarRaySample& Ray)
	{
		uint8 DimensionType = Ray.DimensionType;
		uint8 Classification = static_cast<uint8>(Ray.Classification);
		uint8 bHit = Ray.bHit ? 1 : 0;
		uint8 Reserved = 0;
		int32 RayIndex = Ray.RayIndex;
		float RayYawDegree = Ray.RayYawDegree;
		float RayPitchDegree = Ray.RayPitchDegree;
		float DistanceM = Ray.DistanceM;
		float StartX = ToLidarRayReplayFloat(Ray.StartLocationCm.X);
		float StartY = ToLidarRayReplayFloat(Ray.StartLocationCm.Y);
		float StartZ = ToLidarRayReplayFloat(Ray.StartLocationCm.Z);
		float EndX = ToLidarRayReplayFloat(Ray.EndLocationCm.X);
		float EndY = ToLidarRayReplayFloat(Ray.EndLocationCm.Y);
		float EndZ = ToLidarRayReplayFloat(Ray.EndLocationCm.Z);
		float HitX = ToLidarRayReplayFloat(Ray.HitLocationCm.X);
		float HitY = ToLidarRayReplayFloat(Ray.HitLocationCm.Y);
		float HitZ = ToLidarRayReplayFloat(Ray.HitLocationCm.Z);

		Archive << DimensionType;
		Archive << Classification;
		Archive << bHit;
		Archive << Reserved;
		Archive << RayIndex;
		Archive << RayYawDegree;
		Archive << RayPitchDegree;
		Archive << DistanceM;
		Archive << StartX;
		Archive << StartY;
		Archive << StartZ;
		Archive << EndX;
		Archive << EndY;
		Archive << EndZ;
		Archive << HitX;
		Archive << HitY;
		Archive << HitZ;

		if (Archive.IsLoading())
		{
			Ray.DimensionType = DimensionType;
			Ray.Classification = static_cast<ELidarRayReplayClassification>(Classification);
			Ray.bHit = bHit != 0;
			Ray.RayIndex = RayIndex;
			Ray.RayYawDegree = RayYawDegree;
			Ray.RayPitchDegree = RayPitchDegree;
			Ray.DistanceM = DistanceM;
			Ray.StartLocationCm = FVector(StartX, StartY, StartZ);
			Ray.EndLocationCm = FVector(EndX, EndY, EndZ);
			Ray.HitLocationCm = FVector(HitX, HitY, HitZ);
		}
	}

	// Serializes one variable-length replay ray frame.
	void SerializeLidarRayFrame(FArchive& Archive, FEpisodeLidarRayFrame& Frame)
	{
		float TimeSeconds = Frame.TimeSeconds;
		int32 SensorSequence = Frame.SensorSequence;
		int32 RayCount = Frame.Rays.Num();

		Archive << TimeSeconds;
		Archive << SensorSequence;
		Archive << RayCount;

		if (Archive.IsLoading())
		{
			if (RayCount < 0)
			{
				Archive.SetError();
				return;
			}

			Frame.TimeSeconds = TimeSeconds;
			Frame.SensorSequence = SensorSequence;
			Frame.Rays.SetNum(RayCount);
		}

		for (FEpisodeLidarRaySample& Ray : Frame.Rays)
		{
			SerializeLidarRaySample(Archive, Ray);
		}
	}
}

bool FEpisodeLidarRaySample::IsValidRay(TArray<FString>& OutDiagnostics) const
{
	bool bValid = true;
	if (!IsSupportedDimensionType(DimensionType))
	{
		bValid = AddLidarRayReplayDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("LiDAR ray dimension type is unsupported: %d"), DimensionType));
	}

	if (!IsSupportedClassification(Classification))
	{
		bValid = AddLidarRayReplayDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("LiDAR ray classification is unsupported: %d"), static_cast<uint8>(Classification)));
	}

	if (RayIndex < 0)
	{
		bValid = AddLidarRayReplayDiagnostic(OutDiagnostics, TEXT("LiDAR ray index must be non-negative."));
	}

	if (!FMath::IsFinite(RayYawDegree) || !FMath::IsFinite(RayPitchDegree))
	{
		bValid = AddLidarRayReplayDiagnostic(OutDiagnostics, TEXT("LiDAR ray angles must be finite."));
	}

	if (!FMath::IsFinite(DistanceM) || DistanceM < 0.0f)
	{
		bValid = AddLidarRayReplayDiagnostic(OutDiagnostics, TEXT("LiDAR ray distance must be non-negative."));
	}

	if (!IsFiniteLidarRayReplayVector(StartLocationCm)
		|| !IsFiniteLidarRayReplayVector(EndLocationCm)
		|| !IsFiniteLidarRayReplayVector(HitLocationCm))
	{
		bValid = AddLidarRayReplayDiagnostic(OutDiagnostics, TEXT("LiDAR ray locations must be finite."));
	}

	return bValid;
}

bool FEpisodeLidarRayFrame::IsValidFrame(TArray<FString>& OutDiagnostics) const
{
	bool bValid = true;
	if (!FMath::IsFinite(TimeSeconds) || TimeSeconds < 0.0f)
	{
		bValid = AddLidarRayReplayDiagnostic(OutDiagnostics, TEXT("LiDAR ray frame time must be non-negative."));
	}

	if (SensorSequence < 0)
	{
		bValid = AddLidarRayReplayDiagnostic(OutDiagnostics, TEXT("LiDAR ray sensor sequence must be non-negative."));
	}

	for (const FEpisodeLidarRaySample& Ray : Rays)
	{
		bValid &= Ray.IsValidRay(OutDiagnostics);
	}

	return bValid;
}

bool FEpisodeLidarRayReplayManifest::IsValidManifest(TArray<FString>& OutDiagnostics) const
{
	bool bValid = true;
	if (!Schema.Equals(TEXT("episode_lidar_rays"), ESearchCase::CaseSensitive))
	{
		bValid = AddLidarRayReplayDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Unsupported LiDAR ray manifest schema: %s"), *Schema));
	}

	if (Version != EpisodeLidarRayReplayV1::Version)
	{
		bValid = AddLidarRayReplayDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Unsupported LiDAR ray manifest version: %d"), Version));
	}

	if (FrameFile.TrimStartAndEnd().IsEmpty())
	{
		bValid = AddLidarRayReplayDiagnostic(
			OutDiagnostics,
			TEXT("LiDAR ray manifest frame_file must not be empty."));
	}

	if (!Source.Equals(TEXT("deliverybot_sensor_snapshot"), ESearchCase::CaseSensitive))
	{
		bValid = AddLidarRayReplayDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Unsupported LiDAR ray source: %s"), *Source));
	}

	if (!CoordinateFrame.Equals(TEXT("unreal_world_cm"), ESearchCase::CaseSensitive))
	{
		bValid = AddLidarRayReplayDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Unsupported LiDAR ray coordinate frame: %s"), *CoordinateFrame));
	}

	if (FrameCount < 0)
	{
		bValid = AddLidarRayReplayDiagnostic(OutDiagnostics, TEXT("LiDAR ray frame count must not be negative."));
	}

	if (TotalRayCount < 0)
	{
		bValid = AddLidarRayReplayDiagnostic(OutDiagnostics, TEXT("LiDAR ray total ray count must not be negative."));
	}

	if (!FMath::IsFinite(FirstTimeSeconds) || FirstTimeSeconds < 0.0)
	{
		bValid = AddLidarRayReplayDiagnostic(
			OutDiagnostics,
			TEXT("LiDAR ray first time must be non-negative."));
	}

	if (!FMath::IsFinite(LastTimeSeconds) || LastTimeSeconds < 0.0)
	{
		bValid = AddLidarRayReplayDiagnostic(
			OutDiagnostics,
			TEXT("LiDAR ray last time must be non-negative."));
	}

	if (FrameCount > 0)
	{
		if (FirstSensorSequence < 0 || LastSensorSequence < FirstSensorSequence)
		{
			bValid = AddLidarRayReplayDiagnostic(
				OutDiagnostics,
				TEXT("LiDAR ray sensor sequence range is invalid."));
		}

		if (LastTimeSeconds < FirstTimeSeconds)
		{
			bValid = AddLidarRayReplayDiagnostic(
				OutDiagnostics,
				TEXT("LiDAR ray time range is invalid."));
		}
	}

	return bValid;
}

bool FEpisodeLidarRayReplayManifestJson::SaveToFile(
	const FString& ManifestPath,
	const FEpisodeLidarRayReplayManifest& Manifest,
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
		return AddLidarRayReplayDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Failed to create LiDAR ray manifest directory: %s"), *ManifestDirectory));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("schema"), Manifest.Schema);
	RootObject->SetNumberField(TEXT("version"), Manifest.Version);
	RootObject->SetStringField(TEXT("frame_file"), Manifest.FrameFile);
	RootObject->SetStringField(TEXT("source"), Manifest.Source);
	RootObject->SetStringField(TEXT("coordinate_frame"), Manifest.CoordinateFrame);
	RootObject->SetNumberField(TEXT("frame_count"), Manifest.FrameCount);
	RootObject->SetNumberField(TEXT("total_ray_count"), Manifest.TotalRayCount);
	RootObject->SetNumberField(TEXT("first_sensor_sequence"), Manifest.FirstSensorSequence);
	RootObject->SetNumberField(TEXT("last_sensor_sequence"), Manifest.LastSensorSequence);
	RootObject->SetNumberField(TEXT("first_time_seconds"), Manifest.FirstTimeSeconds);
	RootObject->SetNumberField(TEXT("last_time_seconds"), Manifest.LastTimeSeconds);

	const TSharedRef<FJsonObject> ColorObject = MakeShared<FJsonObject>();
	ColorObject->SetArrayField(TEXT("miss"), MakeColorArray(Manifest.MissColor));
	ColorObject->SetArrayField(TEXT("ground"), MakeColorArray(Manifest.GroundColor));
	ColorObject->SetArrayField(TEXT("wall"), MakeColorArray(Manifest.WallColor));
	ColorObject->SetArrayField(TEXT("obstacle"), MakeColorArray(Manifest.ObstacleColor));
	ColorObject->SetArrayField(TEXT("unknown"), MakeColorArray(Manifest.UnknownColor));
	RootObject->SetObjectField(TEXT("classification_colors"), ColorObject);

	FString JsonString;
	if (!SerializeLidarRayReplayJsonObject(RootObject, JsonString))
	{
		return AddLidarRayReplayDiagnostic(OutDiagnostics, TEXT("Failed to serialize LiDAR ray manifest JSON."));
	}

	if (!FFileHelper::SaveStringToFile(JsonString, *ManifestPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		return AddLidarRayReplayDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Failed to write LiDAR ray manifest: %s"), *ManifestPath));
	}

	return true;
}

bool FEpisodeLidarRayReplayManifestJson::LoadFromFile(
	const FString& ManifestPath,
	FEpisodeLidarRayReplayManifest& OutManifest,
	TArray<FString>& OutDiagnostics)
{
	OutDiagnostics.Reset();
	OutManifest = FEpisodeLidarRayReplayManifest{};

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *ManifestPath))
	{
		return AddLidarRayReplayDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Failed to read LiDAR ray manifest: %s"), *ManifestPath));
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		return AddLidarRayReplayDiagnostic(OutDiagnostics, TEXT("Failed to parse LiDAR ray manifest JSON."));
	}

	if (!RootObject->TryGetStringField(TEXT("schema"), OutManifest.Schema)
		|| !RootObject->TryGetNumberField(TEXT("version"), OutManifest.Version)
		|| !RootObject->TryGetStringField(TEXT("frame_file"), OutManifest.FrameFile)
		|| !RootObject->TryGetStringField(TEXT("source"), OutManifest.Source)
		|| !RootObject->TryGetStringField(TEXT("coordinate_frame"), OutManifest.CoordinateFrame)
		|| !RootObject->TryGetNumberField(TEXT("frame_count"), OutManifest.FrameCount)
		|| !RootObject->TryGetNumberField(TEXT("total_ray_count"), OutManifest.TotalRayCount)
		|| !RootObject->TryGetNumberField(TEXT("first_sensor_sequence"), OutManifest.FirstSensorSequence)
		|| !RootObject->TryGetNumberField(TEXT("last_sensor_sequence"), OutManifest.LastSensorSequence)
		|| !RootObject->TryGetNumberField(TEXT("first_time_seconds"), OutManifest.FirstTimeSeconds)
		|| !RootObject->TryGetNumberField(TEXT("last_time_seconds"), OutManifest.LastTimeSeconds))
	{
		return AddLidarRayReplayDiagnostic(
			OutDiagnostics,
			TEXT("LiDAR ray manifest is missing one or more required fields."));
	}

	if (!TryReadClassificationColors(*RootObject, OutManifest, OutDiagnostics))
	{
		return false;
	}

	return OutManifest.IsValidManifest(OutDiagnostics);
}

bool FEpisodeLidarRayReplayBinary::SaveFramesToFile(
	const FString& FramePath,
	const TArray<FEpisodeLidarRayFrame>& Frames,
	TArray<FString>& OutDiagnostics)
{
	OutDiagnostics.Reset();

	const FString FrameDirectory = FPaths::GetPath(FramePath);
	if (!FrameDirectory.IsEmpty() && !IFileManager::Get().MakeDirectory(*FrameDirectory, true))
	{
		return AddLidarRayReplayDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Failed to create LiDAR ray binary directory: %s"), *FrameDirectory));
	}

	FLidarRayReplayBinaryHeader Header;
	Header.FrameCount = Frames.Num();
	Header.TotalRayCount = CountRaySamples(Frames);

	FBufferArchive Archive;
	SerializeLidarRayReplayHeader(Archive, Header);
	if (Archive.IsError() || Archive.Num() != EpisodeLidarRayReplayV1::BinaryHeaderSizeBytes)
	{
		return AddLidarRayReplayDiagnostic(OutDiagnostics, TEXT("LiDAR ray binary header serialization failed."));
	}

	for (const FEpisodeLidarRayFrame& SourceFrame : Frames)
	{
		TArray<FString> FrameDiagnostics;
		if (!SourceFrame.IsValidFrame(FrameDiagnostics))
		{
			OutDiagnostics.Append(FrameDiagnostics);
			return false;
		}

		const int64 FrameStartOffset = Archive.Num();
		FEpisodeLidarRayFrame Frame = SourceFrame;
		SerializeLidarRayFrame(Archive, Frame);
		if (Archive.IsError())
		{
			return AddLidarRayReplayDiagnostic(OutDiagnostics, TEXT("LiDAR ray writer failed to serialize a frame."));
		}

		const int64 ExpectedFrameBytes =
			EpisodeLidarRayReplayV1::FrameHeaderSizeBytes
			+ static_cast<int64>(Frame.Rays.Num()) * EpisodeLidarRayReplayV1::RaySizeBytes;
		const int64 FrameByteCount = Archive.Num() - FrameStartOffset;
		if (FrameByteCount != ExpectedFrameBytes)
		{
			return AddLidarRayReplayDiagnostic(
				OutDiagnostics,
				FString::Printf(TEXT("LiDAR ray frame serialized to %lld bytes."), FrameByteCount));
		}
	}

	if (!FFileHelper::SaveArrayToFile(Archive, *FramePath))
	{
		return AddLidarRayReplayDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Failed to write LiDAR ray binary: %s"), *FramePath));
	}

	return true;
}

bool FEpisodeLidarRayReplayBinary::LoadFramesFromFile(
	const FString& FramePath,
	TArray<FEpisodeLidarRayFrame>& OutFrames,
	TArray<FString>& OutDiagnostics)
{
	OutDiagnostics.Reset();
	OutFrames.Reset();

	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *FramePath))
	{
		return AddLidarRayReplayDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Failed to read LiDAR ray binary: %s"), *FramePath));
	}

	FMemoryReader Reader(Bytes, true);
	FLidarRayReplayBinaryHeader Header;
	SerializeLidarRayReplayHeader(Reader, Header);
	if (Reader.IsError())
	{
		return AddLidarRayReplayDiagnostic(
			OutDiagnostics,
			TEXT("LiDAR ray binary header magic or endian marker is invalid."));
	}

	if (Header.Version != EpisodeLidarRayReplayV1::Version)
	{
		return AddLidarRayReplayDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Unsupported LiDAR ray binary version: %d"), Header.Version));
	}

	if (Header.FrameCount < 0 || Header.TotalRayCount < 0)
	{
		return AddLidarRayReplayDiagnostic(
			OutDiagnostics,
			TEXT("LiDAR ray binary header counts must not be negative."));
	}

	OutFrames.Reserve(Header.FrameCount);
	int32 LoadedRayCount = 0;
	for (int32 FrameIndex = 0; FrameIndex < Header.FrameCount; ++FrameIndex)
	{
		if (Reader.AtEnd())
		{
			return AddLidarRayReplayDiagnostic(
				OutDiagnostics,
				FString::Printf(TEXT("LiDAR ray binary ended before frame %d."), FrameIndex));
		}

		const int64 FrameStartOffset = Reader.Tell();
		FEpisodeLidarRayFrame Frame;
		SerializeLidarRayFrame(Reader, Frame);
		if (Reader.IsError())
		{
			return AddLidarRayReplayDiagnostic(
				OutDiagnostics,
				FString::Printf(TEXT("LiDAR ray binary frame %d failed to deserialize."), FrameIndex));
		}

		const int64 ExpectedFrameBytes =
			EpisodeLidarRayReplayV1::FrameHeaderSizeBytes
			+ static_cast<int64>(Frame.Rays.Num()) * EpisodeLidarRayReplayV1::RaySizeBytes;
		const int64 FrameByteCount = Reader.Tell() - FrameStartOffset;
		if (FrameByteCount != ExpectedFrameBytes)
		{
			return AddLidarRayReplayDiagnostic(
				OutDiagnostics,
				FString::Printf(TEXT("LiDAR ray binary frame %d byte size mismatch."), FrameIndex));
		}

		TArray<FString> FrameDiagnostics;
		if (!Frame.IsValidFrame(FrameDiagnostics))
		{
			OutDiagnostics.Append(FrameDiagnostics);
			return false;
		}

		LoadedRayCount += Frame.Rays.Num();
		OutFrames.Add(Frame);
	}

	if (!Reader.AtEnd())
	{
		return AddLidarRayReplayDiagnostic(OutDiagnostics, TEXT("LiDAR ray binary has trailing bytes."));
	}

	if (LoadedRayCount != Header.TotalRayCount)
	{
		return AddLidarRayReplayDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("LiDAR ray binary total ray count mismatch: %d."), LoadedRayCount));
	}

	return true;
}

int32 FEpisodeLidarRayReplayBinary::ResolveFrameIndex(
	double TimeSeconds,
	const TArray<FEpisodeLidarRayFrame>& Frames)
{
	if (Frames.IsEmpty() || !FMath::IsFinite(TimeSeconds))
	{
		return INDEX_NONE;
	}

	if (Frames.Num() == 1)
	{
		return 0;
	}

	const double ClampedTimeSeconds = FMath::Max(0.0, TimeSeconds);
	if (ClampedTimeSeconds <= static_cast<double>(Frames[0].TimeSeconds))
	{
		return 0;
	}

	const int32 LastFrameIndex = Frames.Num() - 1;
	if (ClampedTimeSeconds >= static_cast<double>(Frames[LastFrameIndex].TimeSeconds))
	{
		return LastFrameIndex;
	}

	int32 LowerSearchIndex = 0;
	int32 UpperSearchIndex = LastFrameIndex;
	while (LowerSearchIndex < UpperSearchIndex)
	{
		const int32 MidIndex = LowerSearchIndex + (UpperSearchIndex - LowerSearchIndex) / 2;
		if (static_cast<double>(Frames[MidIndex].TimeSeconds) < ClampedTimeSeconds)
		{
			LowerSearchIndex = MidIndex + 1;
		}
		else
		{
			UpperSearchIndex = MidIndex;
		}
	}

	const int32 UpperFrameIndex = LowerSearchIndex;
	const int32 LowerFrameIndex = FMath::Max(0, UpperFrameIndex - 1);
	const double LowerDelta =
		FMath::Abs(ClampedTimeSeconds - static_cast<double>(Frames[LowerFrameIndex].TimeSeconds));
	const double UpperDelta =
		FMath::Abs(static_cast<double>(Frames[UpperFrameIndex].TimeSeconds) - ClampedTimeSeconds);
	return LowerDelta <= UpperDelta
		? LowerFrameIndex
		: UpperFrameIndex;
}
