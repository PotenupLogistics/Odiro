#include "Episode/EpisodeFullLidarPointCloudRecorder.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	const TCHAR* PointCloudDirectoryName = TEXT("lidar_point_cloud");
	const TCHAR* PointCloudFramesDirectoryName = TEXT("frames");
	const TCHAR* PointCloudAccumulatedFileName = TEXT("map_accumulated.xyz");
	const TCHAR* PointCloudWorldAccumulatedFileName = TEXT("world_accumulated.xyz");
	const TCHAR* PointCloudFrameIndexFileName = TEXT("frames.jsonl");
	const TCHAR* PointCloudManifestFileName = TEXT("manifest.json");
	const TCHAR* PointCloudSummaryFileName = TEXT("capture_summary.json");
	const TCHAR* FullOusterProfileName = TEXT("full_os1_point_cloud");
	const TCHAR* PointCloudUnitName = TEXT("centimeter");
	const TCHAR* PointCloudFormatName = TEXT("xyzrgb_ascii");
	const float PointCloudImportYAxisSign = -1.0f;

	// Adds one recorder diagnostic and returns false for validation branches.
	bool AddFullPointCloudRecorderDiagnostic(TArray<FString>& OutDiagnostics, const FString& Message)
	{
		OutDiagnostics.Add(Message);
		return false;
	}

	// Returns true when all vector components can be serialized safely.
	bool IsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X)
			&& FMath::IsFinite(Value.Y)
			&& FMath::IsFinite(Value.Z);
	}

	// Returns true when the lower-case text contains one semantic obstacle token.
	bool FullPointCloudContainsObstacleToken(const FString& LowerText)
	{
		return LowerText.Contains(TEXT("obstacle"))
			|| LowerText.Contains(TEXT("pedestrian"))
			|| LowerText.Contains(TEXT("vehicle"))
			|| LowerText.Contains(TEXT("prop"));
	}

	// Returns true when the lower-case text contains one semantic wall token.
	bool FullPointCloudContainsWallToken(const FString& LowerText)
	{
		return LowerText.Contains(TEXT("wall"))
			|| LowerText.Contains(TEXT("barrier"))
			|| LowerText.Contains(TEXT("fence"))
			|| LowerText.Contains(TEXT("building"));
	}

	// Returns true when the lower-case text contains one semantic ground token.
	bool FullPointCloudContainsGroundToken(const FString& LowerText)
	{
		return LowerText.Contains(TEXT("ground"))
			|| LowerText.Contains(TEXT("floor"))
			|| LowerText.Contains(TEXT("road"))
			|| LowerText.Contains(TEXT("walkable"))
			|| LowerText.Contains(TEXT("lane"));
	}

	// Returns true when the lower-case text marks generated corridor geometry.
	bool FullPointCloudContainsCorridorToken(const FString& LowerText)
	{
		return LowerText.Contains(TEXT("corridor"));
	}

	// Builds one xyzrgb ASCII line from a location and color.
	FString BuildXyzRgbLine(const FVector& LocationCm, const FColor& Color)
	{
		return FString::Printf(
			TEXT("%.4f %.4f %.4f %d %d %d"),
			LocationCm.X,
			LocationCm.Y,
			LocationCm.Z,
			static_cast<int32>(Color.R),
			static_cast<int32>(Color.G),
			static_cast<int32>(Color.B));
	}

	// Serializes a compact JSON object into one line.
	bool TrySerializePointCloudJsonLine(const TSharedRef<FJsonObject>& Object, FString& OutJsonLine)
	{
		OutJsonLine.Reset();
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJsonLine);
		return FJsonSerializer::Serialize(Object, Writer);
	}

	// Serializes a formatted JSON object into a document string.
	bool TrySerializePointCloudJsonDocument(const TSharedRef<FJsonObject>& Object, FString& OutJson)
	{
		OutJson.Reset();
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
		return FJsonSerializer::Serialize(Object, Writer);
	}

	// Saves newline-delimited text lines using UTF-8 without a BOM.
	bool SavePointCloudLines(const FString& Path, const TArray<FString>& Lines, TArray<FString>& OutDiagnostics)
	{
		const FString Contents = Lines.IsEmpty()
			? FString()
			: FString::Join(Lines, LINE_TERMINATOR) + LINE_TERMINATOR;
		if (FFileHelper::SaveStringToFile(Contents, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			return true;
		}

		return AddFullPointCloudRecorderDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Failed to write full LiDAR point-cloud file: %s"), *Path));
	}

	// Saves one JSON object file using UTF-8 without a BOM.
	bool SavePointCloudJsonFile(
		const FString& Path,
		const TSharedRef<FJsonObject>& Object,
		TArray<FString>& OutDiagnostics)
	{
		FString Json;
		if (!TrySerializePointCloudJsonDocument(Object, Json))
		{
			return AddFullPointCloudRecorderDiagnostic(
				OutDiagnostics,
				FString::Printf(TEXT("Failed to serialize full LiDAR point-cloud JSON: %s"), *Path));
		}

		if (FFileHelper::SaveStringToFile(Json + LINE_TERMINATOR, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			return true;
		}

		return AddFullPointCloudRecorderDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Failed to write full LiDAR point-cloud JSON: %s"), *Path));
	}

	// Builds a centimeter vector object used by point-cloud metadata files.
	TSharedRef<FJsonObject> MakePointCloudVectorObject(const FVector& Value)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("x"), Value.X);
		Object->SetNumberField(TEXT("y"), Value.Y);
		Object->SetNumberField(TEXT("z"), Value.Z);
		return Object;
	}

	// Builds one RGB color array used by point-cloud metadata files.
	TArray<TSharedPtr<FJsonValue>> MakePointCloudColorArray(int32 Red, int32 Green, int32 Blue)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		Values.Reserve(3);
		Values.Add(MakeShared<FJsonValueNumber>(Red));
		Values.Add(MakeShared<FJsonValueNumber>(Green));
		Values.Add(MakeShared<FJsonValueNumber>(Blue));
		return Values;
	}

	// Adds point-cloud coordinate metadata required by replay import.
	void SetPointCloudCoordinateMetadata(const TSharedRef<FJsonObject>& Object)
	{
		Object->SetStringField(TEXT("pointUnit"), PointCloudUnitName);
		Object->SetStringField(TEXT("pointFormat"), PointCloudFormatName);
		Object->SetObjectField(TEXT("captureOriginCm"), MakePointCloudVectorObject(FVector::ZeroVector));
		Object->SetNumberField(TEXT("importYAxisSign"), PointCloudImportYAxisSign);
	}
}

bool FEpisodeFullLidarPointCloudRecorder::Open(
	const FString& InEpisodeDirectory,
	TArray<FString>& OutDiagnostics)
{
	OutDiagnostics.Reset();
	Abort();

	EpisodeDirectory = InEpisodeDirectory.TrimStartAndEnd();
	FPaths::NormalizeDirectoryName(EpisodeDirectory);
	if (EpisodeDirectory.IsEmpty())
	{
		return AddFullPointCloudRecorderDiagnostic(
			OutDiagnostics,
			TEXT("Full LiDAR point-cloud recorder episode directory must not be empty."));
	}

	PointCloudDirectory = FPaths::Combine(EpisodeDirectory, PointCloudDirectoryName);
	FPaths::NormalizeDirectoryName(PointCloudDirectory);
	FramesDirectory = FPaths::Combine(PointCloudDirectory, PointCloudFramesDirectoryName);
	FPaths::NormalizeDirectoryName(FramesDirectory);

	if (!IFileManager::Get().MakeDirectory(*FramesDirectory, true))
	{
		return AddFullPointCloudRecorderDiagnostic(
			OutDiagnostics,
			FString::Printf(TEXT("Failed to create full LiDAR point-cloud directory: %s"), *FramesDirectory));
	}

	Frames.Reset();
	AccumulatedImportLines.Reset();
	AccumulatedWorldLines.Reset();
	LastRecordedSensorSequence = INDEX_NONE;
	FirstRecordedSensorSequence = INDEX_NONE;
	LastWrittenSensorSequence = INDEX_NONE;
	FirstRunTimeSeconds = 0.0;
	LastRunTimeSeconds = 0.0;
	bOpen = true;
	return true;
}

bool FEpisodeFullLidarPointCloudRecorder::RecordSensorSnapshot(
	const double RunTimeSeconds,
	const int32 SensorSequence,
	const FDeliveryBotLidarScanInfo& ScanInfo,
	const FDeliveryBotPointCloudCaptureConfigInfo& CaptureConfigInfo,
	TArray<FString>& OutDiagnostics)
{
	OutDiagnostics.Reset();
	if (!bOpen || !CaptureConfigInfo.bCaptureEnabled)
	{
		return true;
	}

	if (!FMath::IsFinite(RunTimeSeconds) || RunTimeSeconds < 0.0)
	{
		return AddFullPointCloudRecorderDiagnostic(
			OutDiagnostics,
			TEXT("Full LiDAR point-cloud recorder rejected a non-finite run time."));
	}

	if (SensorSequence <= 0 || SensorSequence == LastRecordedSensorSequence)
	{
		return true;
	}

	const int32 CaptureEveryNSensorFrames = FMath::Max(1, CaptureConfigInfo.CaptureEveryNSensorFrames);
	if (SensorSequence % CaptureEveryNSensorFrames != 0)
	{
		return true;
	}

	FBufferedFrame Frame;
	Frame.RunTimeSeconds = RunTimeSeconds;
	Frame.SensorSequence = SensorSequence;

	TArray<FString> WorldLines;
	Frame.ImportLines.Reserve(ScanInfo.RayInfos.Num());
	WorldLines.Reserve(ScanInfo.RayInfos.Num());

	for (const FDeliveryBotLidarRayInfo& RayInfo : ScanInfo.RayInfos)
	{
		FString ImportLine;
		FString WorldLine;
		if (TryBuildPointLines(RayInfo, CaptureConfigInfo, ImportLine, WorldLine))
		{
			Frame.ImportLines.Add(MoveTemp(ImportLine));
			WorldLines.Add(MoveTemp(WorldLine));
		}
	}

	if (Frame.ImportLines.IsEmpty())
	{
		LastRecordedSensorSequence = SensorSequence;
		return true;
	}

	if (FirstRecordedSensorSequence == INDEX_NONE)
	{
		FirstRecordedSensorSequence = SensorSequence;
		FirstRunTimeSeconds = RunTimeSeconds;
	}
	LastWrittenSensorSequence = SensorSequence;
	LastRunTimeSeconds = RunTimeSeconds;
	LastRecordedSensorSequence = SensorSequence;

	AccumulatedImportLines.Append(Frame.ImportLines);
	AccumulatedWorldLines.Append(WorldLines);
	Frames.Add(MoveTemp(Frame));
	return true;
}

bool FEpisodeFullLidarPointCloudRecorder::Close(TArray<FString>& OutDiagnostics)
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

	bool bSuccess = WriteFrameFiles(OutDiagnostics);
	bSuccess = WriteAccumulatedFiles(OutDiagnostics) && bSuccess;
	bSuccess = WriteMetadataFiles(OutDiagnostics) && bSuccess;

	Abort();
	return bSuccess;
}

void FEpisodeFullLidarPointCloudRecorder::Abort()
{
	EpisodeDirectory.Reset();
	PointCloudDirectory.Reset();
	FramesDirectory.Reset();
	Frames.Reset();
	AccumulatedImportLines.Reset();
	AccumulatedWorldLines.Reset();
	LastRecordedSensorSequence = INDEX_NONE;
	FirstRecordedSensorSequence = INDEX_NONE;
	LastWrittenSensorSequence = INDEX_NONE;
	FirstRunTimeSeconds = 0.0;
	LastRunTimeSeconds = 0.0;
	bOpen = false;
}

bool FEpisodeFullLidarPointCloudRecorder::TryBuildPointLines(
	const FDeliveryBotLidarRayInfo& RayInfo,
	const FDeliveryBotPointCloudCaptureConfigInfo& CaptureConfigInfo,
	FString& OutImportLine,
	FString& OutWorldLine) const
{
	OutImportLine.Reset();
	OutWorldLine.Reset();

	if (!RayInfo.bHit
		|| RayInfo.RayDimensionType != EDeliveryBotLidarRayDimensionType::ThreeD
		|| !IsFiniteVector(RayInfo.HitLocationCm)
		|| !FMath::IsFinite(RayInfo.DistanceM))
	{
		return false;
	}

	if (CaptureConfigInfo.RangeLimitM > 0.0f && RayInfo.DistanceM > CaptureConfigInfo.RangeLimitM)
	{
		return false;
	}

	const FString Classification = ResolvePointClassification(RayInfo);
	if (!CaptureConfigInfo.bIncludeGroundPoints && Classification == TEXT("ground"))
	{
		return false;
	}

	const FColor Color = ResolvePointColor(Classification);
	const FVector ImportLocationCm(
		RayInfo.HitLocationCm.X,
		RayInfo.HitLocationCm.Y * PointCloudImportYAxisSign,
		RayInfo.HitLocationCm.Z);

	OutImportLine = BuildXyzRgbLine(ImportLocationCm, Color);
	OutWorldLine = BuildXyzRgbLine(RayInfo.HitLocationCm, Color);
	return true;
}

FString FEpisodeFullLidarPointCloudRecorder::ResolvePointClassification(
	const FDeliveryBotLidarRayInfo& RayInfo) const
{
	const FString TargetTagClassification =
		ResolveClassificationFromTags(RayInfo.TargetTags, RayInfo);
	if (TargetTagClassification != TEXT("unknown"))
	{
		return TargetTagClassification;
	}

	const FString ActorTagClassification =
		ResolveClassificationFromTags(RayInfo.ActorTags, RayInfo);
	if (ActorTagClassification != TEXT("unknown"))
	{
		return ActorTagClassification;
	}

	const FString ActorNameClassification =
		ResolveClassificationFromText(RayInfo.ActorName, RayInfo);
	if (ActorNameClassification != TEXT("unknown"))
	{
		return ActorNameClassification;
	}

	if (RayInfo.TargetTags.IsEmpty() && RayInfo.ActorTags.IsEmpty() && RayInfo.ActorName.TrimStartAndEnd().IsEmpty())
	{
		return TEXT("ground");
	}

	return TEXT("unknown");
}

FString FEpisodeFullLidarPointCloudRecorder::ResolveClassificationFromTags(
	const TArray<FName>& Tags,
	const FDeliveryBotLidarRayInfo& RayInfo) const
{
	for (const FName& Tag : Tags)
	{
		const FString Classification = ResolveClassificationFromText(Tag.ToString(), RayInfo);
		if (Classification != TEXT("unknown"))
		{
			return Classification;
		}
	}

	return TEXT("unknown");
}

FString FEpisodeFullLidarPointCloudRecorder::ResolveClassificationFromText(
	const FString& Text,
	const FDeliveryBotLidarRayInfo& RayInfo) const
{
	const FString LowerText = Text.TrimStartAndEnd().ToLower();
	if (LowerText.IsEmpty())
	{
		return TEXT("unknown");
	}

	if (FullPointCloudContainsObstacleToken(LowerText))
	{
		return TEXT("obstacle");
	}

	if (FullPointCloudContainsWallToken(LowerText))
	{
		return TEXT("wall");
	}

	if (FullPointCloudContainsGroundToken(LowerText))
	{
		return TEXT("ground");
	}

	if (FullPointCloudContainsCorridorToken(LowerText))
	{
		return RayInfo.HitLocationCm.Z > 12.0
			? TEXT("wall")
			: TEXT("ground");
	}

	return TEXT("unknown");
}

FColor FEpisodeFullLidarPointCloudRecorder::ResolvePointColor(const FString& Classification) const
{
	if (Classification == TEXT("ground"))
	{
		return FColor(120, 120, 120);
	}
	if (Classification == TEXT("wall"))
	{
		return FColor(80, 180, 255);
	}
	if (Classification == TEXT("obstacle"))
	{
		return FColor(255, 80, 60);
	}
	return FColor(160, 120, 255);
}

bool FEpisodeFullLidarPointCloudRecorder::WriteAccumulatedFiles(TArray<FString>& OutDiagnostics) const
{
	const FString AccumulatedPath = FPaths::Combine(PointCloudDirectory, PointCloudAccumulatedFileName);
	const FString WorldAccumulatedPath = FPaths::Combine(PointCloudDirectory, PointCloudWorldAccumulatedFileName);

	bool bSuccess = SavePointCloudLines(AccumulatedPath, AccumulatedImportLines, OutDiagnostics);
	bSuccess = SavePointCloudLines(WorldAccumulatedPath, AccumulatedWorldLines, OutDiagnostics) && bSuccess;
	return bSuccess;
}

bool FEpisodeFullLidarPointCloudRecorder::WriteFrameFiles(TArray<FString>& OutDiagnostics) const
{
	TArray<FString> FrameIndexLines;
	FrameIndexLines.Reserve(Frames.Num());

	bool bSuccess = true;
	for (const FBufferedFrame& Frame : Frames)
	{
		const FString FrameFileName = FString::Printf(TEXT("frame_%06d.xyz"), Frame.SensorSequence);
		const FString FramePath = FPaths::Combine(FramesDirectory, FrameFileName);
		bSuccess = SavePointCloudLines(FramePath, Frame.ImportLines, OutDiagnostics) && bSuccess;

		FString RelativeFramePath = FPaths::Combine(PointCloudFramesDirectoryName, FrameFileName);
		FPaths::NormalizeFilename(RelativeFramePath);

		TSharedRef<FJsonObject> FrameObject = MakeShared<FJsonObject>();
		FrameObject->SetStringField(TEXT("captureType"), FullOusterProfileName);
		FrameObject->SetNumberField(TEXT("sensorSequence"), Frame.SensorSequence);
		FrameObject->SetNumberField(TEXT("sensorTimeSeconds"), Frame.RunTimeSeconds);
		FrameObject->SetNumberField(TEXT("runTimeSeconds"), Frame.RunTimeSeconds);
		FrameObject->SetNumberField(TEXT("pointCount"), Frame.ImportLines.Num());
		FrameObject->SetStringField(TEXT("path"), RelativeFramePath);

		FString JsonLine;
		if (TrySerializePointCloudJsonLine(FrameObject, JsonLine))
		{
			FrameIndexLines.Add(JsonLine);
		}
		else
		{
			bSuccess = AddFullPointCloudRecorderDiagnostic(
				OutDiagnostics,
				FString::Printf(TEXT("Failed to serialize full LiDAR point-cloud frame index: %d"), Frame.SensorSequence))
				&& bSuccess;
		}
	}

	const FString FrameIndexPath = FPaths::Combine(PointCloudDirectory, PointCloudFrameIndexFileName);
	bSuccess = SavePointCloudLines(FrameIndexPath, FrameIndexLines, OutDiagnostics) && bSuccess;
	return bSuccess;
}

bool FEpisodeFullLidarPointCloudRecorder::WriteMetadataFiles(TArray<FString>& OutDiagnostics) const
{
	TSharedRef<FJsonObject> ColorObject = MakeShared<FJsonObject>();
	ColorObject->SetArrayField(TEXT("ground"), MakePointCloudColorArray(120, 120, 120));
	ColorObject->SetArrayField(TEXT("wall"), MakePointCloudColorArray(80, 180, 255));
	ColorObject->SetArrayField(TEXT("obstacle"), MakePointCloudColorArray(255, 80, 60));
	ColorObject->SetArrayField(TEXT("unknown"), MakePointCloudColorArray(160, 120, 255));

	TSharedRef<FJsonObject> UnrealImportObject = MakeShared<FJsonObject>();
	UnrealImportObject->SetStringField(TEXT("mainReviewFile"), PointCloudAccumulatedFileName);
	UnrealImportObject->SetStringField(TEXT("debugWorldFile"), PointCloudWorldAccumulatedFileName);
	UnrealImportObject->SetStringField(TEXT("frameIndexFile"), PointCloudFrameIndexFileName);
	UnrealImportObject->SetStringField(TEXT("note"), TEXT("Generated from full OS1 LiDAR hit rays in Unreal; Python policy payload remains compacted."));

	TSharedRef<FJsonObject> ManifestObject = MakeShared<FJsonObject>();
	ManifestObject->SetStringField(TEXT("schema"), TEXT("lidar_point_cloud_capture"));
	ManifestObject->SetNumberField(TEXT("schemaVersion"), 2);
	ManifestObject->SetStringField(TEXT("profile"), FullOusterProfileName);
	ManifestObject->SetStringField(TEXT("captureType"), FullOusterProfileName);
	SetPointCloudCoordinateMetadata(ManifestObject);
	ManifestObject->SetObjectField(TEXT("classificationColors"), ColorObject);
	ManifestObject->SetObjectField(TEXT("unrealImport"), UnrealImportObject);

	TSharedRef<FJsonObject> SummaryObject = MakeShared<FJsonObject>();
	SummaryObject->SetStringField(TEXT("schema"), TEXT("lidar_point_cloud_capture_summary"));
	SummaryObject->SetStringField(TEXT("profile"), FullOusterProfileName);
	SummaryObject->SetStringField(TEXT("captureType"), FullOusterProfileName);
	SetPointCloudCoordinateMetadata(SummaryObject);
	SummaryObject->SetNumberField(TEXT("frameCount"), Frames.Num());
	SummaryObject->SetNumberField(TEXT("totalPointCount"), AccumulatedImportLines.Num());
	SummaryObject->SetNumberField(TEXT("firstSensorSequence"), FirstRecordedSensorSequence);
	SummaryObject->SetNumberField(TEXT("lastSensorSequence"), LastWrittenSensorSequence);
	SummaryObject->SetNumberField(TEXT("firstRunTimeSeconds"), FirstRunTimeSeconds);
	SummaryObject->SetNumberField(TEXT("lastRunTimeSeconds"), LastRunTimeSeconds);

	bool bSuccess = SavePointCloudJsonFile(
		FPaths::Combine(PointCloudDirectory, PointCloudManifestFileName),
		ManifestObject,
		OutDiagnostics);
	bSuccess = SavePointCloudJsonFile(
		FPaths::Combine(PointCloudDirectory, PointCloudSummaryFileName),
		SummaryObject,
		OutDiagnostics) && bSuccess;
	return bSuccess;
}
