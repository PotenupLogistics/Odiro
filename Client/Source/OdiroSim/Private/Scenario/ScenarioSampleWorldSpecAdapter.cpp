#include "Scenario/ScenarioSampleWorldSpecAdapter.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Algo/Reverse.h"
#include "Misc/Crc.h"
#include "Scenario/ScenarioCorridorGeometry.h"
#include "Shared/ScenarioSampleJson.h"

namespace
{
	const double MetersToCentimeters = 100.0;
	const double RobotEndpointInsetMeters = 1.0;
	// Generated city surfaces share the scenario base plane; CityBuildings assets own visual road drops.
	const FName GeneratedCityRoadSurfaceId(TEXT("road"));
	// Dot-product tolerance used to recognize a right-angle generated building-side rectangle.
	const double GeneratedCityRightAngleDotTolerance = 0.01;
	// Point tolerance for stitching adjacent generated city chunks in meters.
	const double GeneratedCityPointToleranceMeters = 0.01;

	// Side of the sampled walkway used when deriving generated city padding bands.
	enum class EGeneratedCitySide : uint8
	{
		Lower,
		Upper
	};

	// Axis segment clipped to a layout along range for rectangular GroundRegion generation.
	struct FGeneratedCityAxisChunk
	{
		// World-space start point in meters.
		FVector2D StartWorldMeters = FVector2D::ZeroVector;

		// World-space end point in meters.
		FVector2D EndWorldMeters = FVector2D::ZeroVector;

		// Stable local index used to make deterministic region ids.
		int32 ChunkIndex = 0;
	};

	// One generated GroundRegion band measured as an offset interval from the sampled route axis.
	struct FGeneratedCityBandSpec
	{
		// Stable id fragment for generated runtime GroundRegions.
		FString BandId;

		// Surface catalog id used for material and semantic metadata.
		FString SurfaceId;

		// Runtime traversability class for this generated band.
		EScenarioGroundRegionType RegionType = EScenarioGroundRegionType::Walkable;

		// Offset interval in meters in the sampled corridor frame.
		FScenarioOffsetRangeMeters OffsetRangeMeters;

		// Collision tag applied to blocked generated bands.
		FString CollisionTag;

		// Penalty kind applied to penalty generated bands.
		FString PenaltyKind;

		// Penalty cost applied to penalty generated bands.
		double PenaltyCost = 0.0;
	};

	struct FResolvedSamplePose
	{
		FVector LocationCm = FVector::ZeroVector;
		double YawDegrees = 0.0;
	};

	void AddAdapterDiagnostic(
		FScenarioCompileResult& Result,
		EScenarioCompileDiagnosticSeverity Severity,
		const FString& Code,
		const FString& Message)
	{
		FScenarioCompileDiagnostic Diagnostic;
		Diagnostic.Severity = Severity;
		Diagnostic.Code = Code;
		Diagnostic.Message = Message;
		Result.Diagnostics.Add(Diagnostic);
	}

	// Resolves generated city GroundRegion top height from its surface vocabulary.
	double ResolveGeneratedCitySurfaceTopZCm(const FString& SurfaceId)
	{
		return FName(*SurfaceId) == GeneratedCityRoadSurfaceId
			? 0.0
			: FScenarioCorridorGeometry::DefaultSurfaceTopZCm;
	}

	EScenarioCompileDiagnosticSeverity ToCompileSeverity(EScenarioSchemaDiagnosticSeverity Severity)
	{
		switch (Severity)
		{
		case EScenarioSchemaDiagnosticSeverity::Info:
			return EScenarioCompileDiagnosticSeverity::Info;
		case EScenarioSchemaDiagnosticSeverity::Warning:
		case EScenarioSchemaDiagnosticSeverity::Repair:
			return EScenarioCompileDiagnosticSeverity::Warning;
		case EScenarioSchemaDiagnosticSeverity::Error:
			return EScenarioCompileDiagnosticSeverity::Error;
		default:
			return EScenarioCompileDiagnosticSeverity::Error;
		}
	}

	void AppendSchemaDiagnostics(
		const TArray<FScenarioSchemaDiagnostic>& SchemaDiagnostics,
		FScenarioCompileResult& Result)
	{
		for (const FScenarioSchemaDiagnostic& SchemaDiagnostic : SchemaDiagnostics)
		{
			FString Message = SchemaDiagnostic.Message;
			if (!SchemaDiagnostic.Path.IsEmpty())
			{
				Message = FString::Printf(TEXT("%s | Path: %s"), *Message, *SchemaDiagnostic.Path);
			}

			AddAdapterDiagnostic(Result, ToCompileSeverity(SchemaDiagnostic.Severity), SchemaDiagnostic.Code, Message);
		}
	}

	FScenarioParamValue MakeRuntimeBoolParam(bool Value)
	{
		FScenarioParamValue ParamValue;
		ParamValue.Type = EScenarioParamValueType::Bool;
		ParamValue.BoolValue = Value;
		return ParamValue;
	}

	FScenarioParamValue MakeRuntimeIntegerParam(int32 Value)
	{
		FScenarioParamValue ParamValue;
		ParamValue.Type = EScenarioParamValueType::Integer;
		ParamValue.IntegerValue = Value;
		return ParamValue;
	}

	FScenarioParamValue MakeRuntimeFloatParam(double Value)
	{
		FScenarioParamValue ParamValue;
		ParamValue.Type = EScenarioParamValueType::Float;
		ParamValue.FloatValue = Value;
		return ParamValue;
	}

	FScenarioParamValue MakeRuntimeStringParam(const FString& Value)
	{
		FScenarioParamValue ParamValue;
		ParamValue.Type = EScenarioParamValueType::String;
		ParamValue.StringValue = Value;
		return ParamValue;
	}

	FScenarioParamValue MakeRuntimeVectorParam(const FVector& Value)
	{
		FScenarioParamValue ParamValue;
		ParamValue.Type = EScenarioParamValueType::Vector;
		ParamValue.VectorValue = Value;
		return ParamValue;
	}

	bool TryConvertSampleParamValue(
		const FScenarioSampleParamValue& SampleValue,
		FScenarioParamValue& OutRuntimeValue)
	{
		switch (SampleValue.Type)
		{
		case EScenarioSampleParamValueType::Boolean:
			OutRuntimeValue = MakeRuntimeBoolParam(SampleValue.BoolValue);
			return true;
		case EScenarioSampleParamValueType::Integer:
			OutRuntimeValue = MakeRuntimeIntegerParam(SampleValue.IntegerValue);
			return true;
		case EScenarioSampleParamValueType::Float:
			OutRuntimeValue = MakeRuntimeFloatParam(SampleValue.FloatValue);
			return true;
		case EScenarioSampleParamValueType::String:
			OutRuntimeValue = MakeRuntimeStringParam(SampleValue.StringValue);
			return true;
		default:
			OutRuntimeValue = FScenarioParamValue();
			return false;
		}
	}

	FVector2D RotateSamplePoint(const FVector2D& Point, double HeadingDegrees)
	{
		const double HeadingRadians = FMath::DegreesToRadians(HeadingDegrees);
		const double CosHeading = FMath::Cos(HeadingRadians);
		const double SinHeading = FMath::Sin(HeadingRadians);
		return FVector2D(
			(Point.X * CosHeading) - (Point.Y * SinHeading),
			(Point.X * SinHeading) + (Point.Y * CosHeading));
	}

	// Resolves the Corridor lane at a sampled pose before adapting it to runtime actors.
	bool TryResolveRuntimeSurfaceAtSamplePose(
		const FScenarioSampleSemantic& Semantic,
		double AlongMeters,
		double OffsetMeters,
		double& OutSurfaceZOffsetCm,
		EScenarioSampleLaneType& OutLaneType,
		FString& OutLaneId,
		FString& OutSurfaceId)
	{
		OutSurfaceZOffsetCm = 0.0;
		OutLaneType = EScenarioSampleLaneType::Walkable;
		OutLaneId.Reset();
		OutSurfaceId.Reset();
		for (const FScenarioSampleLayoutEntry& LayoutEntry : Semantic.Layout)
		{
			if (!FScenarioCorridorGeometry::ContainsRangeValue(
					AlongMeters,
					LayoutEntry.AlongRangeMeters.StartMeters,
					LayoutEntry.AlongRangeMeters.EndMeters))
			{
				continue;
			}

			for (const FScenarioSampleLayoutLane& Lane : LayoutEntry.Lanes)
			{
				if (FScenarioCorridorGeometry::ContainsRangeValue(
						OffsetMeters,
						Lane.OffsetRangeMeters.MinMeters,
						Lane.OffsetRangeMeters.MaxMeters))
				{
					OutSurfaceZOffsetCm = FScenarioCorridorGeometry::ResolveLaneSurfaceZOffsetCm(Lane.LaneId);
					OutLaneType = Lane.Type;
					OutLaneId = Lane.LaneId;
					OutSurfaceId = Lane.SurfaceId;
					return true;
				}
			}
		}

		return false;
	}

	// Resolves the surface top height at a sampled pose so actors spawn on the matching lane surface.
	double ResolveRuntimeSurfaceTopZCm(
		const FScenarioSampleSemantic& Semantic,
		double AlongMeters,
		double OffsetMeters)
	{
		double SurfaceZOffsetCm = 0.0;
		EScenarioSampleLaneType LaneType = EScenarioSampleLaneType::Walkable;
		FString LaneId;
		FString SurfaceId;
		return TryResolveRuntimeSurfaceAtSamplePose(
				Semantic,
				AlongMeters,
				OffsetMeters,
				SurfaceZOffsetCm,
				LaneType,
				LaneId,
				SurfaceId)
			? FScenarioCorridorGeometry::DefaultSurfaceTopZCm + SurfaceZOffsetCm
			: FScenarioCorridorGeometry::DefaultSurfaceTopZCm;
	}

	bool ResolveSampleAxisPose(
		const FScenarioSampleRouteAxis& Axis,
		double AlongMeters,
		double OffsetMeters,
		FResolvedSamplePose& OutPose)
	{
		if (Axis.PointsMeters.Num() < 2)
		{
			return false;
		}

		const double ClampedAlongMeters = FMath::Max(AlongMeters, 0.0);
		double RemainingMeters = ClampedAlongMeters;
		FVector2D LocalPoint = Axis.PointsMeters[0];
		FVector2D LocalDirection = Axis.PointsMeters[1] - Axis.PointsMeters[0];

		for (int32 Index = 0; Index < Axis.PointsMeters.Num() - 1; ++Index)
		{
			const FVector2D SegmentStart = Axis.PointsMeters[Index];
			const FVector2D SegmentEnd = Axis.PointsMeters[Index + 1];
			const FVector2D SegmentVector = SegmentEnd - SegmentStart;
			const double SegmentLength = SegmentVector.Size();
			if (SegmentLength <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			LocalDirection = SegmentVector / SegmentLength;
			if (RemainingMeters <= SegmentLength || Index == Axis.PointsMeters.Num() - 2)
			{
				const double SegmentDistance = FMath::Clamp(RemainingMeters, 0.0, SegmentLength);
				LocalPoint = SegmentStart + (LocalDirection * SegmentDistance);
				break;
			}

			RemainingMeters -= SegmentLength;
		}

		const FVector2D WorldDirection = RotateSamplePoint(LocalDirection, Axis.HeadingDegrees).GetSafeNormal();
		const FVector2D WorldPointMeters = Axis.OriginXYMeters + RotateSamplePoint(LocalPoint, Axis.HeadingDegrees);
		const FVector2D OffsetDirection(-WorldDirection.Y, WorldDirection.X);
		const FVector2D OffsetPointMeters = WorldPointMeters + (OffsetDirection * OffsetMeters);

		OutPose.LocationCm = FVector(
			OffsetPointMeters.X * MetersToCentimeters,
			OffsetPointMeters.Y * MetersToCentimeters,
			0.0);
		OutPose.YawDegrees = FMath::RadiansToDegrees(FMath::Atan2(WorldDirection.Y, WorldDirection.X));
		return true;
	}

	double CalculateSampleAxisLengthMeters(const FScenarioSampleRouteAxis& Axis)
	{
		double PolylineLengthMeters = 0.0;
		for (int32 Index = 0; Index < Axis.PointsMeters.Num() - 1; ++Index)
		{
			PolylineLengthMeters += (Axis.PointsMeters[Index + 1] - Axis.PointsMeters[Index]).Size();
		}

		return PolylineLengthMeters > KINDA_SMALL_NUMBER
			? PolylineLengthMeters
			: FMath::Max(Axis.LengthMeters, 0.0);
	}

	double ResolveRuntimeRobotAlongMeters(
		const FScenarioSampleRouteAxis& Axis,
		const FScenarioSampleRobotPose& Pose)
	{
		const double AxisLengthMeters = CalculateSampleAxisLengthMeters(Axis);
		if (AxisLengthMeters <= KINDA_SMALL_NUMBER)
		{
			return FMath::Max(Pose.AlongMeters, 0.0);
		}

		const double SafeInsetMeters = FMath::Min(RobotEndpointInsetMeters, AxisLengthMeters * 0.5);
		switch (Pose.SourceAnchorType)
		{
		case EScenarioTemplateRobotAnchorType::Entry:
			return FMath::Clamp(FMath::Max(Pose.AlongMeters, SafeInsetMeters), 0.0, AxisLengthMeters);
		case EScenarioTemplateRobotAnchorType::Exit:
			return FMath::Clamp(FMath::Min(Pose.AlongMeters, AxisLengthMeters - SafeInsetMeters), 0.0, AxisLengthMeters);
		case EScenarioTemplateRobotAnchorType::CorridorPose:
		default:
			return FMath::Clamp(Pose.AlongMeters, 0.0, AxisLengthMeters);
		}
	}

	EScenarioGroundRegionType ToGroundRegionType(EScenarioSampleLaneType LaneType)
	{
		switch (LaneType)
		{
		case EScenarioSampleLaneType::Walkable:
			return EScenarioGroundRegionType::Walkable;
		case EScenarioSampleLaneType::Penalty:
			return EScenarioGroundRegionType::Penalty;
		case EScenarioSampleLaneType::Blocked:
			return EScenarioGroundRegionType::Blocked;
		default:
			return EScenarioGroundRegionType::Walkable;
		}
	}

	double ToTraversabilityScore(EScenarioSampleLaneType LaneType)
	{
		switch (LaneType)
		{
		case EScenarioSampleLaneType::Walkable:
			return 1.0;
		case EScenarioSampleLaneType::Penalty:
			return 0.5;
		case EScenarioSampleLaneType::Blocked:
			return 0.0;
		default:
			return 1.0;
		}
	}

	// Returns the score used by generated GroundRegions before authored metrics exist for the padding.
	double ToGeneratedCityTraversabilityScore(EScenarioGroundRegionType RegionType)
	{
		switch (RegionType)
		{
		case EScenarioGroundRegionType::Penalty:
			return 0.5;
		case EScenarioGroundRegionType::Blocked:
			return 0.0;
		case EScenarioGroundRegionType::Walkable:
		default:
			return 1.0;
		}
	}

	// Sanitizes authored segment ids before using them inside generated region ids.
	FString MakeGeneratedCityIdFragment(const FString& RawId)
	{
		FString IdFragment = RawId.IsEmpty() ? TEXT("segment") : RawId;
		IdFragment.ReplaceInline(TEXT(" "), TEXT("_"));
		IdFragment.ReplaceInline(TEXT("."), TEXT("_"));
		IdFragment.ReplaceInline(TEXT("/"), TEXT("_"));
		IdFragment.ReplaceInline(TEXT("\\"), TEXT("_"));
		IdFragment.ReplaceInline(TEXT(":"), TEXT("_"));
		return IdFragment;
	}

	// Builds route-axis chunks clipped to one layout entry's along range.
	void BuildGeneratedCityAxisChunks(
		const FScenarioSampleRouteAxis& Axis,
		const FScenarioAlongRangeMeters& AlongRangeMeters,
		TArray<FGeneratedCityAxisChunk>& OutChunks)
	{
		OutChunks.Reset();
		if (Axis.PointsMeters.Num() < 2)
		{
			return;
		}

		const double ClampedStartMeters = FMath::Max(0.0, FMath::Min(AlongRangeMeters.StartMeters, AlongRangeMeters.EndMeters));
		const double ClampedEndMeters = FMath::Max(0.0, FMath::Max(AlongRangeMeters.StartMeters, AlongRangeMeters.EndMeters));
		if (ClampedEndMeters <= ClampedStartMeters + KINDA_SMALL_NUMBER)
		{
			return;
		}

		double SegmentStartAlongMeters = 0.0;
		for (int32 Index = 0; Index < Axis.PointsMeters.Num() - 1; ++Index)
		{
			const FVector2D SegmentStart = Axis.PointsMeters[Index];
			const FVector2D SegmentEnd = Axis.PointsMeters[Index + 1];
			const FVector2D SegmentVector = SegmentEnd - SegmentStart;
			const double SegmentLengthMeters = SegmentVector.Size();
			if (SegmentLengthMeters <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const double SegmentEndAlongMeters = SegmentStartAlongMeters + SegmentLengthMeters;
			const double ChunkStartAlongMeters = FMath::Max(ClampedStartMeters, SegmentStartAlongMeters);
			const double ChunkEndAlongMeters = FMath::Min(ClampedEndMeters, SegmentEndAlongMeters);
			if (ChunkEndAlongMeters > ChunkStartAlongMeters + KINDA_SMALL_NUMBER)
			{
				const FVector2D LocalDirection = SegmentVector / SegmentLengthMeters;
				const FVector2D LocalStart = SegmentStart + (LocalDirection * (ChunkStartAlongMeters - SegmentStartAlongMeters));
				const FVector2D LocalEnd = SegmentStart + (LocalDirection * (ChunkEndAlongMeters - SegmentStartAlongMeters));

				FGeneratedCityAxisChunk Chunk;
				Chunk.StartWorldMeters = Axis.OriginXYMeters + RotateSamplePoint(LocalStart, Axis.HeadingDegrees);
				Chunk.EndWorldMeters = Axis.OriginXYMeters + RotateSamplePoint(LocalEnd, Axis.HeadingDegrees);
				Chunk.ChunkIndex = OutChunks.Num();
				OutChunks.Add(Chunk);
			}

			SegmentStartAlongMeters = SegmentEndAlongMeters;
			if (SegmentStartAlongMeters >= ClampedEndMeters)
			{
				break;
			}
		}
	}

	// Appends route-axis chunks while preserving a stable global chunk index for generated region ids.
	void AppendGeneratedCityAxisChunks(
		const FScenarioSampleRouteAxis& Axis,
		const FScenarioSampleLayoutEntry& LayoutEntry,
		TArray<FGeneratedCityAxisChunk>& OutChunks)
	{
		TArray<FGeneratedCityAxisChunk> LayoutChunks;
		BuildGeneratedCityAxisChunks(Axis, LayoutEntry.AlongRangeMeters, LayoutChunks);
		for (FGeneratedCityAxisChunk& LayoutChunk : LayoutChunks)
		{
			LayoutChunk.ChunkIndex = OutChunks.Num();
			OutChunks.Add(LayoutChunk);
		}
	}

	// Merges adjacent same-direction chunks so layout splits do not change generated city-block topology.
	bool TryBuildSimplifiedContinuousCityAxisChunks(
		const TArray<FGeneratedCityAxisChunk>& Chunks,
		TArray<FGeneratedCityAxisChunk>& OutChunks)
	{
		OutChunks.Reset();
		for (const FGeneratedCityAxisChunk& Chunk : Chunks)
		{
			const FVector2D ChunkVector = Chunk.EndWorldMeters - Chunk.StartWorldMeters;
			const double ChunkLengthMeters = ChunkVector.Size();
			if (ChunkLengthMeters <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			if (OutChunks.IsEmpty())
			{
				OutChunks.Add(Chunk);
				continue;
			}

			FGeneratedCityAxisChunk& PreviousChunk = OutChunks.Last();
			if ((PreviousChunk.EndWorldMeters - Chunk.StartWorldMeters).Size() > GeneratedCityPointToleranceMeters)
			{
				return false;
			}

			const FVector2D PreviousVector = PreviousChunk.EndWorldMeters - PreviousChunk.StartWorldMeters;
			const double PreviousLengthMeters = PreviousVector.Size();
			if (PreviousLengthMeters <= KINDA_SMALL_NUMBER)
			{
				PreviousChunk = Chunk;
				continue;
			}

			const FVector2D PreviousForward = PreviousVector / PreviousLengthMeters;
			const FVector2D ChunkForward = ChunkVector / ChunkLengthMeters;
			const double DirectionCrossZ =
				(PreviousForward.X * ChunkForward.Y) - (PreviousForward.Y * ChunkForward.X);
			if (FMath::Abs(DirectionCrossZ) <= GeneratedCityRightAngleDotTolerance
				&& FVector2D::DotProduct(PreviousForward, ChunkForward) > 0.0)
			{
				PreviousChunk.EndWorldMeters = Chunk.EndWorldMeters;
				continue;
			}

			OutChunks.Add(Chunk);
		}

		return !OutChunks.IsEmpty();
	}

	// Returns the signed offset direction for one generated city side.
	double GeneratedCitySideSign(EGeneratedCitySide Side)
	{
		return Side == EGeneratedCitySide::Lower ? -1.0 : 1.0;
	}

	// Returns the deterministic region-id fragment for one generated city side.
	FString GeneratedCitySideIdFragment(EGeneratedCitySide Side)
	{
		return Side == EGeneratedCitySide::Lower ? TEXT("lower") : TEXT("upper");
	}

	// Resolves the walkway lane offset bounds used as anchors for generated city padding.
	bool TryResolveWalkwayOffsetRange(
		const FScenarioSampleLayoutEntry& LayoutEntry,
		FScenarioOffsetRangeMeters& OutOffsetRangeMeters)
	{
		const FScenarioSampleLayoutLane* WalkwayLane = LayoutEntry.Lanes.FindByPredicate(
			[](const FScenarioSampleLayoutLane& Lane)
			{
				return Lane.LaneId.Equals(TEXT("walkway"), ESearchCase::IgnoreCase);
			});
		if (!WalkwayLane)
		{
			return false;
		}

		OutOffsetRangeMeters = WalkwayLane->OffsetRangeMeters;
		return OutOffsetRangeMeters.MaxMeters > OutOffsetRangeMeters.MinMeters + KINDA_SMALL_NUMBER;
	}

	// Determines whether a sampled lane is on the negative or positive side of the walkway axis.
	bool TryResolveGeneratedCitySide(
		const FScenarioSampleLayoutLane& Lane,
		EGeneratedCitySide& OutSide)
	{
		if (Lane.OffsetRangeMeters.MaxMeters <= 0.0)
		{
			OutSide = EGeneratedCitySide::Lower;
			return true;
		}

		if (Lane.OffsetRangeMeters.MinMeters >= 0.0)
		{
			OutSide = EGeneratedCitySide::Upper;
			return true;
		}

		const double CenterOffsetMeters =
			(Lane.OffsetRangeMeters.MinMeters + Lane.OffsetRangeMeters.MaxMeters) * 0.5;
		if (CenterOffsetMeters < -KINDA_SMALL_NUMBER)
		{
			OutSide = EGeneratedCitySide::Lower;
			return true;
		}

		if (CenterOffsetMeters > KINDA_SMALL_NUMBER)
		{
			OutSide = EGeneratedCitySide::Upper;
			return true;
		}

		return false;
	}

	// Detects side lanes that should seed building-front generated padding.
	bool IsGeneratedCityBuildingSideLane(const FScenarioSampleLayoutLane& Lane)
	{
		return Lane.SurfaceId.Equals(TEXT("building"), ESearchCase::IgnoreCase)
			|| Lane.LaneId.StartsWith(TEXT("building"), ESearchCase::IgnoreCase);
	}

	// Detects side lanes that should seed road generated padding.
	bool IsGeneratedCityRoadSideLane(const FScenarioSampleLayoutLane& Lane)
	{
		return Lane.SurfaceId.Equals(TEXT("road"), ESearchCase::IgnoreCase);
	}

	// Scans one layout entry for semantic sides that should receive generated city padding.
	void ResolveGeneratedCitySideFlags(
		const FScenarioSampleLayoutEntry& LayoutEntry,
		bool& bOutLowerBuildingSide,
		bool& bOutUpperBuildingSide,
		bool& bOutLowerRoadSide,
		bool& bOutUpperRoadSide)
	{
		bOutLowerBuildingSide = false;
		bOutUpperBuildingSide = false;
		bOutLowerRoadSide = false;
		bOutUpperRoadSide = false;

		for (const FScenarioSampleLayoutLane& Lane : LayoutEntry.Lanes)
		{
			EGeneratedCitySide Side = EGeneratedCitySide::Upper;
			if (!TryResolveGeneratedCitySide(Lane, Side))
			{
				continue;
			}

			if (IsGeneratedCityBuildingSideLane(Lane))
			{
				if (Side == EGeneratedCitySide::Lower)
				{
					bOutLowerBuildingSide = true;
				}
				else
				{
					bOutUpperBuildingSide = true;
				}
			}

			if (IsGeneratedCityRoadSideLane(Lane))
			{
				if (Side == EGeneratedCitySide::Lower)
				{
					bOutLowerRoadSide = true;
				}
				else
				{
					bOutUpperRoadSide = true;
				}
			}
		}
	}

	// Creates the common metadata for a building-side walkable expansion GroundRegion.
	FScenarioGroundRegionSpec MakeGeneratedCityBuildingExpansionRegion(
		const FString& RegionId,
		const FVector2D& CenterMeters,
		double LengthMeters,
		double WidthMeters,
		double YawDegrees)
	{
		FScenarioGroundRegionSpec RegionSpec;
		RegionSpec.RegionId = RegionId;
		RegionSpec.RegionType = EScenarioGroundRegionType::Walkable;
		RegionSpec.SurfaceId = TEXT("walkway");
		RegionSpec.ShapeType = EScenarioGroundShapeType::Rectangle;
		RegionSpec.Center = FVector(
			CenterMeters.X * MetersToCentimeters,
			CenterMeters.Y * MetersToCentimeters,
			ResolveGeneratedCitySurfaceTopZCm(RegionSpec.SurfaceId));
		RegionSpec.Size = FVector2D(
			LengthMeters * MetersToCentimeters,
			WidthMeters * MetersToCentimeters);
		RegionSpec.YawDegrees = YawDegrees;
		RegionSpec.TraversabilityScore = ToGeneratedCityTraversabilityScore(RegionSpec.RegionType);
		return RegionSpec;
	}

	// Creates one walkable polygon GroundRegion for a non-orthogonal building-side city block.
	FScenarioGroundRegionSpec MakeGeneratedCityBuildingExpansionPolygonRegion(
		const FString& RegionId,
		const TArray<FVector2D>& WorldVerticesMeters)
	{
		FVector2D CenterMeters = FVector2D::ZeroVector;
		for (const FVector2D& VertexMeters : WorldVerticesMeters)
		{
			CenterMeters += VertexMeters;
		}
		CenterMeters /= static_cast<double>(WorldVerticesMeters.Num());

		TArray<FVector2D> LocalVerticesCm;
		LocalVerticesCm.Reserve(WorldVerticesMeters.Num());
		FBox2D LocalBoundsCm(ForceInit);
		for (const FVector2D& VertexMeters : WorldVerticesMeters)
		{
			const FVector2D LocalVertexCm = (VertexMeters - CenterMeters) * MetersToCentimeters;
			LocalVerticesCm.Add(LocalVertexCm);
			LocalBoundsCm += LocalVertexCm;
		}

		double SignedArea = 0.0;
		for (int32 Index = 0; Index < LocalVerticesCm.Num(); ++Index)
		{
			const FVector2D& Current = LocalVerticesCm[Index];
			const FVector2D& Next = LocalVerticesCm[(Index + 1) % LocalVerticesCm.Num()];
			SignedArea += (Current.X * Next.Y) - (Next.X * Current.Y);
		}
		if (SignedArea < 0.0)
		{
			Algo::Reverse(LocalVerticesCm);
		}

		FScenarioGroundRegionSpec RegionSpec;
		RegionSpec.RegionId = RegionId;
		RegionSpec.RegionType = EScenarioGroundRegionType::Walkable;
		RegionSpec.SurfaceId = TEXT("walkway");
		RegionSpec.ShapeType = EScenarioGroundShapeType::ConvexPolygon;
		RegionSpec.Center = FVector(
			CenterMeters.X * MetersToCentimeters,
			CenterMeters.Y * MetersToCentimeters,
			ResolveGeneratedCitySurfaceTopZCm(RegionSpec.SurfaceId));
		RegionSpec.Size = LocalBoundsCm.bIsValid ? LocalBoundsCm.GetSize() : FVector2D::ZeroVector;
		RegionSpec.PolygonVertices = MoveTemp(LocalVerticesCm);
		RegionSpec.TraversabilityScore = ToGeneratedCityTraversabilityScore(RegionSpec.RegionType);
		return RegionSpec;
	}

	// Emits one rectangle or convex polygon when two chunks enclose a building-side city block.
	bool TryAddGeneratedCityTwoChunkBuildingExpansionRegion(
		const TArray<FGeneratedCityAxisChunk>& Chunks,
		EGeneratedCitySide Side,
		int32 LayoutIndex,
		FScenarioWorldSpec& WorldSpec)
	{
		TArray<FGeneratedCityAxisChunk> SimplifiedChunks;
		if (!TryBuildSimplifiedContinuousCityAxisChunks(Chunks, SimplifiedChunks)
			|| SimplifiedChunks.Num() != 2)
		{
			return false;
		}

		const FVector2D FirstVector = SimplifiedChunks[0].EndWorldMeters - SimplifiedChunks[0].StartWorldMeters;
		const FVector2D SecondVector = SimplifiedChunks[1].EndWorldMeters - SimplifiedChunks[1].StartWorldMeters;
		const double FirstLengthMeters = FirstVector.Size();
		const double SecondLengthMeters = SecondVector.Size();
		if (FirstLengthMeters <= KINDA_SMALL_NUMBER || SecondLengthMeters <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		if ((SimplifiedChunks[0].EndWorldMeters - SimplifiedChunks[1].StartWorldMeters).Size() > GeneratedCityPointToleranceMeters)
		{
			return false;
		}

		const FVector2D FirstForward = FirstVector / FirstLengthMeters;
		const FVector2D SecondForward = SecondVector / SecondLengthMeters;
		const double MinimumExpansionWidthMeters =
			FScenarioCorridorGeometry::GeneratedCityWalkwayExtensionWidthMeters
			+ FScenarioCorridorGeometry::GeneratedCityBuildingDepthMeters;
		const double EffectiveSecondLengthMeters = FMath::Max(SecondLengthMeters, MinimumExpansionWidthMeters);
		const FVector2D EffectiveSecondVector = SecondForward * EffectiveSecondLengthMeters;
		const FVector2D EffectiveSecondEndWorldMeters =
			SimplifiedChunks[1].StartWorldMeters + EffectiveSecondVector;
		const double CrossZ = (FirstForward.X * SecondForward.Y) - (FirstForward.Y * SecondForward.X);
		if (FMath::Abs(CrossZ) <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		const double InteriorSideSign = CrossZ > 0.0 ? 1.0 : -1.0;
		if (!FMath::IsNearlyEqual(InteriorSideSign, GeneratedCitySideSign(Side)))
		{
			return false;
		}

		const FString RegionId = FString::Printf(
			TEXT("generated_city_%s_building_expansion_%02d_00"),
			*GeneratedCitySideIdFragment(Side),
			LayoutIndex);
		if (FMath::Abs(FVector2D::DotProduct(FirstForward, SecondForward)) <= GeneratedCityRightAngleDotTolerance)
		{
			const FVector2D CenterMeters =
				(SimplifiedChunks[0].StartWorldMeters + EffectiveSecondEndWorldMeters) * 0.5;
			WorldSpec.GroundRegions.Add(MakeGeneratedCityBuildingExpansionRegion(
				RegionId,
				CenterMeters,
				FirstLengthMeters,
				EffectiveSecondLengthMeters,
				FMath::RadiansToDegrees(FMath::Atan2(FirstForward.Y, FirstForward.X))));
			return true;
		}

		const TArray<FVector2D> WorldVerticesMeters = {
			SimplifiedChunks[0].StartWorldMeters,
			SimplifiedChunks[0].EndWorldMeters,
			EffectiveSecondEndWorldMeters,
			SimplifiedChunks[0].StartWorldMeters + EffectiveSecondVector
		};
		WorldSpec.GroundRegions.Add(MakeGeneratedCityBuildingExpansionPolygonRegion(
			RegionId,
			WorldVerticesMeters));
		return true;
	}

	// Emits fixed-width walkable expansion strips when a single rectangle cannot represent the side area.
	void AddGeneratedCityFixedWidthBuildingExpansionRegions(
		const TArray<FGeneratedCityAxisChunk>& Chunks,
		EGeneratedCitySide Side,
		int32 LayoutIndex,
		const FScenarioOffsetRangeMeters& WalkwayOffsetRangeMeters,
		FScenarioWorldSpec& WorldSpec)
	{
		const double WidthMeters =
			FScenarioCorridorGeometry::GeneratedCityWalkwayExtensionWidthMeters
			+ FScenarioCorridorGeometry::GeneratedCityBuildingDepthMeters;
		const double WalkwayEdgeMeters = Side == EGeneratedCitySide::Lower
			? WalkwayOffsetRangeMeters.MinMeters
			: WalkwayOffsetRangeMeters.MaxMeters;
		const double CenterOffsetMeters = WalkwayEdgeMeters + (GeneratedCitySideSign(Side) * WidthMeters * 0.5);

		for (const FGeneratedCityAxisChunk& Chunk : Chunks)
		{
			const FVector2D SegmentVectorMeters = Chunk.EndWorldMeters - Chunk.StartWorldMeters;
			const double LengthMeters = SegmentVectorMeters.Size();
			if (LengthMeters <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const FVector2D Forward = SegmentVectorMeters / LengthMeters;
			const FVector2D Right(-Forward.Y, Forward.X);
			const FVector2D CenterMeters =
				((Chunk.StartWorldMeters + Chunk.EndWorldMeters) * 0.5) + (Right * CenterOffsetMeters);
			const FString RegionId = FString::Printf(
				TEXT("generated_city_%s_building_expansion_%02d_%02d"),
				*GeneratedCitySideIdFragment(Side),
				LayoutIndex,
				Chunk.ChunkIndex);
			WorldSpec.GroundRegions.Add(MakeGeneratedCityBuildingExpansionRegion(
				RegionId,
				CenterMeters,
				LengthMeters,
				WidthMeters,
				FMath::RadiansToDegrees(FMath::Atan2(Forward.Y, Forward.X))));
		}
	}

	// Appends one generated GroundRegion band for every straight axis chunk in the layout range.
	void AddGeneratedCityBandGroundRegions(
		const FScenarioSampleRouteAxis& Axis,
		const FScenarioSampleLayoutEntry& LayoutEntry,
		int32 LayoutIndex,
		const FGeneratedCityBandSpec& BandSpec,
		FScenarioWorldSpec& WorldSpec)
	{
		const double WidthMeters = BandSpec.OffsetRangeMeters.MaxMeters - BandSpec.OffsetRangeMeters.MinMeters;
		if (WidthMeters <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		TArray<FGeneratedCityAxisChunk> Chunks;
		BuildGeneratedCityAxisChunks(Axis, LayoutEntry.AlongRangeMeters, Chunks);
		if (Chunks.IsEmpty())
		{
			return;
		}

		const double CenterOffsetMeters =
			(BandSpec.OffsetRangeMeters.MinMeters + BandSpec.OffsetRangeMeters.MaxMeters) * 0.5;
		const FString SegmentId = MakeGeneratedCityIdFragment(LayoutEntry.SegmentId);
		for (const FGeneratedCityAxisChunk& Chunk : Chunks)
		{
			const FVector2D SegmentVectorMeters = Chunk.EndWorldMeters - Chunk.StartWorldMeters;
			const double LengthMeters = SegmentVectorMeters.Size();
			if (LengthMeters <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const FVector2D Forward = SegmentVectorMeters / LengthMeters;
			const FVector2D Right(-Forward.Y, Forward.X);
			const FVector2D CenterMeters =
				((Chunk.StartWorldMeters + Chunk.EndWorldMeters) * 0.5) + (Right * CenterOffsetMeters);

			FScenarioGroundRegionSpec RegionSpec;
			RegionSpec.RegionId = FString::Printf(
				TEXT("generated_city_%s_%s_%02d_%02d"),
				*SegmentId,
				*BandSpec.BandId,
				LayoutIndex,
				Chunk.ChunkIndex);
			RegionSpec.RegionType = BandSpec.RegionType;
			RegionSpec.SurfaceId = BandSpec.SurfaceId;
			RegionSpec.ShapeType = EScenarioGroundShapeType::Rectangle;
			RegionSpec.Center = FVector(
				CenterMeters.X * MetersToCentimeters,
				CenterMeters.Y * MetersToCentimeters,
				ResolveGeneratedCitySurfaceTopZCm(BandSpec.SurfaceId));
			RegionSpec.Size = FVector2D(
				LengthMeters * MetersToCentimeters,
				WidthMeters * MetersToCentimeters);
			RegionSpec.YawDegrees = FMath::RadiansToDegrees(FMath::Atan2(Forward.Y, Forward.X));
			RegionSpec.TraversabilityScore = ToGeneratedCityTraversabilityScore(BandSpec.RegionType);
			RegionSpec.CollisionTag = BandSpec.CollisionTag;
			RegionSpec.PenaltyKind = BandSpec.PenaltyKind;
			RegionSpec.PenaltyCost = BandSpec.PenaltyCost;
			WorldSpec.GroundRegions.Add(RegionSpec);
		}
	}

	// Creates generated walkable area that extends the building side of the Corridor.
	void AddGeneratedCityBuildingSideGroundRegions(
		const TArray<FGeneratedCityAxisChunk>& Chunks,
		int32 LayoutIndex,
		EGeneratedCitySide Side,
		const FScenarioOffsetRangeMeters& WalkwayOffsetRangeMeters,
		FScenarioWorldSpec& WorldSpec)
	{
		if (Chunks.IsEmpty())
		{
			return;
		}

		if (TryAddGeneratedCityTwoChunkBuildingExpansionRegion(Chunks, Side, LayoutIndex, WorldSpec))
		{
			return;
		}

		AddGeneratedCityFixedWidthBuildingExpansionRegions(
			Chunks,
			Side,
			LayoutIndex,
			WalkwayOffsetRangeMeters,
			WorldSpec);
	}

	// Creates a generated blocking curb band and penalty road band on one side of the walkway.
	void AddGeneratedCityRoadSideGroundRegions(
		const FScenarioSampleRouteAxis& Axis,
		const FScenarioSampleLayoutEntry& LayoutEntry,
		int32 LayoutIndex,
		EGeneratedCitySide Side,
		const FScenarioOffsetRangeMeters& WalkwayOffsetRangeMeters,
		FScenarioWorldSpec& WorldSpec)
	{
		const double WalkwayEdgeMeters = Side == EGeneratedCitySide::Lower
			? WalkwayOffsetRangeMeters.MinMeters
			: WalkwayOffsetRangeMeters.MaxMeters;
		const double SideSign = Side == EGeneratedCitySide::Lower ? -1.0 : 1.0;

		FGeneratedCityBandSpec CurbBand;
		CurbBand.BandId = Side == EGeneratedCitySide::Lower ? TEXT("lower_curb") : TEXT("upper_curb");
		CurbBand.SurfaceId = TEXT("road");
		CurbBand.RegionType = EScenarioGroundRegionType::Penalty;
		CurbBand.CollisionTag = TEXT("curb");
		CurbBand.PenaltyKind = TEXT("curb");
		CurbBand.PenaltyCost = 1.0;
		CurbBand.OffsetRangeMeters.MinMeters =
			FMath::Min(
				WalkwayEdgeMeters,
				WalkwayEdgeMeters + (SideSign * FScenarioCorridorGeometry::GeneratedCityCurbWidthMeters));
		CurbBand.OffsetRangeMeters.MaxMeters =
			FMath::Max(
				WalkwayEdgeMeters,
				WalkwayEdgeMeters + (SideSign * FScenarioCorridorGeometry::GeneratedCityCurbWidthMeters));
		AddGeneratedCityBandGroundRegions(Axis, LayoutEntry, LayoutIndex, CurbBand, WorldSpec);

		FGeneratedCityBandSpec RoadBand;
		RoadBand.BandId = Side == EGeneratedCitySide::Lower ? TEXT("lower_road_2lane") : TEXT("upper_road_2lane");
		RoadBand.SurfaceId = TEXT("road");
		RoadBand.RegionType = EScenarioGroundRegionType::Penalty;
		RoadBand.PenaltyKind = TEXT("road");
		RoadBand.PenaltyCost = 1.0;
		const double RoadNearEdgeMeters =
			WalkwayEdgeMeters + (SideSign * FScenarioCorridorGeometry::GeneratedCityCurbWidthMeters);
		const double RoadFarEdgeMeters =
			RoadNearEdgeMeters
			+ (SideSign * FScenarioCorridorGeometry::GeneratedCityTwoLaneRoadWidthMeters);
		RoadBand.OffsetRangeMeters.MinMeters = FMath::Min(RoadNearEdgeMeters, RoadFarEdgeMeters);
		RoadBand.OffsetRangeMeters.MaxMeters = FMath::Max(RoadNearEdgeMeters, RoadFarEdgeMeters);
		AddGeneratedCityBandGroundRegions(Axis, LayoutEntry, LayoutIndex, RoadBand, WorldSpec);
	}

	// Adds deterministic semantic city padding around sampled Corridor surfaces without changing authored JSON.
	void AddGeneratedCityGroundRegionsFromSample(
		const FScenarioSampleSemantic& Semantic,
		FScenarioWorldSpec& WorldSpec)
	{
		if (Semantic.RouteAxis.PointsMeters.Num() < 2)
		{
			return;
		}

		TArray<FGeneratedCityAxisChunk> LowerBuildingSideChunks;
		TArray<FGeneratedCityAxisChunk> UpperBuildingSideChunks;
		FScenarioOffsetRangeMeters LowerBuildingSideWalkwayOffsetRange;
		FScenarioOffsetRangeMeters UpperBuildingSideWalkwayOffsetRange;
		int32 LowerBuildingSideFirstLayoutIndex = INDEX_NONE;
		int32 UpperBuildingSideFirstLayoutIndex = INDEX_NONE;

		for (int32 LayoutIndex = 0; LayoutIndex < Semantic.Layout.Num(); ++LayoutIndex)
		{
			const FScenarioSampleLayoutEntry& LayoutEntry = Semantic.Layout[LayoutIndex];
			FScenarioOffsetRangeMeters WalkwayOffsetRangeMeters;
			if (!TryResolveWalkwayOffsetRange(LayoutEntry, WalkwayOffsetRangeMeters))
			{
				continue;
			}

			bool bLowerBuildingSide = false;
			bool bUpperBuildingSide = false;
			bool bLowerRoadSide = false;
			bool bUpperRoadSide = false;
			ResolveGeneratedCitySideFlags(
				LayoutEntry,
				bLowerBuildingSide,
				bUpperBuildingSide,
				bLowerRoadSide,
				bUpperRoadSide);

			if (bLowerBuildingSide)
			{
				if (LowerBuildingSideFirstLayoutIndex == INDEX_NONE)
				{
					LowerBuildingSideFirstLayoutIndex = LayoutIndex;
					LowerBuildingSideWalkwayOffsetRange = WalkwayOffsetRangeMeters;
				}
				AppendGeneratedCityAxisChunks(
					Semantic.RouteAxis,
					LayoutEntry,
					LowerBuildingSideChunks);
			}
			if (bUpperBuildingSide)
			{
				if (UpperBuildingSideFirstLayoutIndex == INDEX_NONE)
				{
					UpperBuildingSideFirstLayoutIndex = LayoutIndex;
					UpperBuildingSideWalkwayOffsetRange = WalkwayOffsetRangeMeters;
				}
				AppendGeneratedCityAxisChunks(
					Semantic.RouteAxis,
					LayoutEntry,
					UpperBuildingSideChunks);
			}
			if (bLowerRoadSide)
			{
				AddGeneratedCityRoadSideGroundRegions(
					Semantic.RouteAxis,
					LayoutEntry,
					LayoutIndex,
					EGeneratedCitySide::Lower,
					WalkwayOffsetRangeMeters,
					WorldSpec);
			}
			if (bUpperRoadSide)
			{
				AddGeneratedCityRoadSideGroundRegions(
					Semantic.RouteAxis,
					LayoutEntry,
					LayoutIndex,
					EGeneratedCitySide::Upper,
					WalkwayOffsetRangeMeters,
					WorldSpec);
			}
		}

		if (LowerBuildingSideFirstLayoutIndex != INDEX_NONE)
		{
			AddGeneratedCityBuildingSideGroundRegions(
				LowerBuildingSideChunks,
				LowerBuildingSideFirstLayoutIndex,
				EGeneratedCitySide::Lower,
				LowerBuildingSideWalkwayOffsetRange,
				WorldSpec);
		}
		if (UpperBuildingSideFirstLayoutIndex != INDEX_NONE)
		{
			AddGeneratedCityBuildingSideGroundRegions(
				UpperBuildingSideChunks,
				UpperBuildingSideFirstLayoutIndex,
				EGeneratedCitySide::Upper,
				UpperBuildingSideWalkwayOffsetRange,
				WorldSpec);
		}
	}

	FString MakeAdapterSpecHash(const FScenarioSampleDocument& Document)
	{
		const FString HashSource = FString::Printf(
			TEXT("%s:%s:%s:%s:%s:%lld:%d"),
			*Document.Sample.SampleId,
			*Document.Sample.ScenarioId,
			*Document.Sample.Source.TemplateHash,
			*Document.Sample.Source.ProfileHash,
			*Document.Sample.Source.SettingHash,
			Document.Sample.Source.Seed,
			Document.Version);
		return FString::Printf(TEXT("%08x"), FCrc::StrCrc32(*HashSource));
	}

	bool TryReadSampleNumber(const FScenarioSampleParamValue& ParamValue, double& OutValue)
	{
		if (ParamValue.Type == EScenarioSampleParamValueType::Float)
		{
			OutValue = ParamValue.FloatValue;
			return true;
		}

		if (ParamValue.Type == EScenarioSampleParamValueType::Integer)
		{
			OutValue = static_cast<double>(ParamValue.IntegerValue);
			return true;
		}

		return false;
	}

	double ResolveSampleMaxDurationSeconds(
		const FScenarioSampleDocument& Document,
		FScenarioCompileResult& Result)
	{
		const FScenarioSampleParamValue* MaxDurationParam = Document.Scenario.Params.Find(TEXT("max_duration_s"));
		if (!MaxDurationParam)
		{
			return 0.0;
		}

		double MaxDurationSeconds = 0.0;
		if (!TryReadSampleNumber(*MaxDurationParam, MaxDurationSeconds))
		{
			AddAdapterDiagnostic(
				Result,
				EScenarioCompileDiagnosticSeverity::Warning,
				TEXT("invalid_max_duration_s"),
				TEXT("scenario_sample param 'max_duration_s' must be numeric to populate runtime MaxDurationSeconds."));
			return 0.0;
		}

		if (MaxDurationSeconds < 0.0)
		{
			AddAdapterDiagnostic(
				Result,
				EScenarioCompileDiagnosticSeverity::Warning,
				TEXT("negative_max_duration_s"),
				FString::Printf(TEXT("scenario_sample param 'max_duration_s' was clamped to 0.0 from %.2f."), MaxDurationSeconds));
		}

		return FMath::Max(0.0, MaxDurationSeconds);
	}

	void PopulateRunConfig(
		const FScenarioSampleDocument& Document,
		FScenarioCompileResult& Result,
		FScenarioWorldSpec& WorldSpec)
	{
		WorldSpec.RunConfig.TemplateId = Document.Sample.ScenarioId.IsEmpty()
			? Document.Sample.SampleId
			: Document.Sample.ScenarioId;
		WorldSpec.RunConfig.TemplateVersion = Document.Version;
		WorldSpec.RunConfig.GeneratorVersion = FScenarioSampleJson::SupportedVersion;
		WorldSpec.RunConfig.BaseSeed = Document.Sample.Source.Seed;
		WorldSpec.RunConfig.IterationIndex = 0;
		WorldSpec.RunConfig.MaxDurationSeconds = ResolveSampleMaxDurationSeconds(Document, Result);

		for (const TPair<FString, FScenarioSampleParamValue>& Pair : Document.Scenario.Params)
		{
			FScenarioParamValue RuntimeValue;
			if (TryConvertSampleParamValue(Pair.Value, RuntimeValue))
			{
				WorldSpec.RunConfig.Parameters.Add(Pair.Key, RuntimeValue);
				continue;
			}

			AddAdapterDiagnostic(
				Result,
				EScenarioCompileDiagnosticSeverity::Warning,
				TEXT("unsupported_sample_param_type"),
				FString::Printf(TEXT("scenario_sample param '%s' is not representable by runtime scalar parameters."), *Pair.Key));
		}

		WorldSpec.Seeds.WorldSeed = Document.Sample.Source.Seed;
		WorldSpec.Seeds.LayoutSeed = Document.Sample.Source.Seed + 101;
		WorldSpec.Seeds.StaticObstacleSeed = Document.Sample.Source.Seed + 202;
		WorldSpec.Seeds.DynamicActorSeed = Document.Sample.Source.Seed + 303;
		WorldSpec.Seeds.EventSeed = Document.Sample.Source.Seed + 404;
		WorldSpec.Seeds.PolicySeed = Document.Sample.Source.Seed + 505;
	}

	void AddCorridorsFromSample(
		const FScenarioSampleSemantic& Semantic,
		FScenarioCompileResult& Result,
		FScenarioWorldSpec& WorldSpec)
	{
		if (Semantic.Layout.IsEmpty())
		{
			AddAdapterDiagnostic(
				Result,
				EScenarioCompileDiagnosticSeverity::Warning,
				TEXT("sample_layout_empty"),
				TEXT("scenario_sample semantic layout is empty; no runtime Corridor surfaces were generated."));
			return;
		}

		if (Semantic.RouteAxis.PointsMeters.Num() < 2)
		{
			AddAdapterDiagnostic(
				Result,
				EScenarioCompileDiagnosticSeverity::Error,
				TEXT("sample_corridor_axis_invalid"),
				TEXT("scenario_sample route_axis requires at least two points to generate runtime Corridor surfaces."));
			return;
		}

		FScenarioRuntimeCorridorSpec Corridor;
		Corridor.CorridorId = TEXT("corridor_01");
		Corridor.AxisType = Semantic.RouteAxis.Type;
		Corridor.OriginXYMeters = Semantic.RouteAxis.OriginXYMeters;
		Corridor.HeadingDegrees = Semantic.RouteAxis.HeadingDegrees;
		Corridor.PointsMeters = Semantic.RouteAxis.PointsMeters;
		Corridor.LengthMeters = Semantic.RouteAxis.LengthMeters;

		for (const FScenarioSampleLayoutEntry& LayoutEntry : Semantic.Layout)
		{
			FScenarioRuntimeCorridorLayoutEntry RuntimeLayoutEntry;
			RuntimeLayoutEntry.SegmentId = LayoutEntry.SegmentId;
			RuntimeLayoutEntry.AlongRangeMeters = LayoutEntry.AlongRangeMeters;

			for (const FScenarioSampleLayoutLane& Lane : LayoutEntry.Lanes)
			{
				FScenarioRuntimeCorridorLaneSpec RuntimeLane;
				RuntimeLane.LaneId = Lane.LaneId;
				RuntimeLane.OffsetRangeMeters = Lane.OffsetRangeMeters;
				RuntimeLane.SurfaceId = Lane.SurfaceId;
				RuntimeLane.RegionType = ToGroundRegionType(Lane.Type);
				RuntimeLane.SurfaceZOffsetCm = FScenarioCorridorGeometry::ResolveLaneSurfaceZOffsetCm(Lane.LaneId);
				RuntimeLane.TraversabilityScore = ToTraversabilityScore(Lane.Type);
				if (Lane.Type == EScenarioSampleLaneType::Blocked)
				{
					RuntimeLane.CollisionTag = Lane.SurfaceId.IsEmpty() ? TEXT("blocked") : Lane.SurfaceId;
				}
				if (Lane.Type == EScenarioSampleLaneType::Penalty)
				{
					RuntimeLane.PenaltyKind = Lane.SurfaceId.IsEmpty() ? TEXT("penalty") : Lane.SurfaceId;
					RuntimeLane.PenaltyCost = 1.0;
				}
				RuntimeLayoutEntry.Lanes.Add(RuntimeLane);
			}

			Corridor.Layout.Add(RuntimeLayoutEntry);
		}

		WorldSpec.Corridors.Add(Corridor);
	}

	void AddRobotFromSample(
		const FScenarioSampleSemantic& Semantic,
		FScenarioCompileResult& Result,
		FScenarioWorldSpec& WorldSpec)
	{
		FResolvedSamplePose StartPose;
		FResolvedSamplePose GoalPose;
		const double RuntimeStartAlongMeters = ResolveRuntimeRobotAlongMeters(
			Semantic.RouteAxis,
			Semantic.Robot.Start);
		const double RuntimeGoalAlongMeters = ResolveRuntimeRobotAlongMeters(
			Semantic.RouteAxis,
			Semantic.Robot.Goal);
		const bool bResolvedStart = ResolveSampleAxisPose(
			Semantic.RouteAxis,
			RuntimeStartAlongMeters,
			Semantic.Robot.Start.OffsetMeters,
			StartPose);
		const bool bResolvedGoal = ResolveSampleAxisPose(
			Semantic.RouteAxis,
			RuntimeGoalAlongMeters,
			Semantic.Robot.Goal.OffsetMeters,
			GoalPose);

		if (!bResolvedStart || !bResolvedGoal)
		{
			AddAdapterDiagnostic(
				Result,
				EScenarioCompileDiagnosticSeverity::Error,
				TEXT("sample_robot_pose_failed"),
				TEXT("Failed to resolve scenario_sample robot start or goal pose."));
			return;
		}

		StartPose.LocationCm.Z = ResolveRuntimeSurfaceTopZCm(
			Semantic,
			RuntimeStartAlongMeters,
			Semantic.Robot.Start.OffsetMeters);
		GoalPose.LocationCm.Z = ResolveRuntimeSurfaceTopZCm(
			Semantic,
			RuntimeGoalAlongMeters,
			Semantic.Robot.Goal.OffsetMeters);

		FScenarioPlaceableInstanceSpec RobotSpec;
		RobotSpec.InstanceId = TEXT("robot_01");
		RobotSpec.AssetId = TEXT("delivery_bot");
		RobotSpec.Category = EScenarioActorCategory::DeliveryBot;
		RobotSpec.Transform = FTransform(FRotator(0.0, Semantic.Robot.Start.HeadingDegrees, 0.0), StartPose.LocationCm);
		RobotSpec.DeliveryBot.bSpawnOnly = false;
		RobotSpec.DeliveryBot.bHasStartLocation = true;
		RobotSpec.DeliveryBot.bHasGoalLocation = true;
		RobotSpec.DeliveryBot.SetupInfo.LocationSetupInfo.StartLocationCm = StartPose.LocationCm;
		RobotSpec.DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm = GoalPose.LocationCm;
		RobotSpec.DeliveryBot.SetupInfo.LocationSetupInfo.bHasGoal = true;
		RobotSpec.DeliveryBot.SetupInfo.LocationSetupInfo.bAutoStartRoute = true;
		RobotSpec.Properties.Add(TEXT("sample_start_along_m"), MakeRuntimeFloatParam(Semantic.Robot.Start.AlongMeters));
		RobotSpec.Properties.Add(TEXT("sample_goal_along_m"), MakeRuntimeFloatParam(Semantic.Robot.Goal.AlongMeters));
		RobotSpec.Properties.Add(TEXT("runtime_start_along_m"), MakeRuntimeFloatParam(RuntimeStartAlongMeters));
		RobotSpec.Properties.Add(TEXT("runtime_goal_along_m"), MakeRuntimeFloatParam(RuntimeGoalAlongMeters));
		RobotSpec.Properties.Add(TEXT("sample_start_segment"), MakeRuntimeStringParam(Semantic.Robot.Start.SegmentId));
		RobotSpec.Properties.Add(TEXT("sample_goal_segment"), MakeRuntimeStringParam(Semantic.Robot.Goal.SegmentId));
		WorldSpec.Placeables.Add(RobotSpec);
	}

	void AddStaticObstaclesFromSample(
		const FScenarioSampleSemantic& Semantic,
		FScenarioCompileResult& Result,
		FScenarioWorldSpec& WorldSpec)
	{
		for (const FScenarioSampleStaticObstacle& Obstacle : Semantic.StaticObstacles)
		{
			FResolvedSamplePose Pose;
			if (!ResolveSampleAxisPose(Semantic.RouteAxis, Obstacle.AlongMeters, Obstacle.OffsetMeters, Pose))
			{
				AddAdapterDiagnostic(
					Result,
					EScenarioCompileDiagnosticSeverity::Error,
					TEXT("sample_obstacle_pose_failed"),
					FString::Printf(TEXT("Failed to resolve static obstacle '%s' pose."), *Obstacle.ObstacleId));
				continue;
			}

			double SurfaceZOffsetCm = 0.0;
			EScenarioSampleLaneType LaneType = EScenarioSampleLaneType::Walkable;
			FString LaneId;
			FString SurfaceId;
			if (!TryResolveRuntimeSurfaceAtSamplePose(
					Semantic,
					Obstacle.AlongMeters,
					Obstacle.OffsetMeters,
					SurfaceZOffsetCm,
					LaneType,
					LaneId,
					SurfaceId))
			{
				AddAdapterDiagnostic(
					Result,
					EScenarioCompileDiagnosticSeverity::Error,
					TEXT("sample_obstacle_surface_missing"),
					FString::Printf(
						TEXT("Static obstacle '%s' is outside scenario_sample Corridor surfaces."),
						*Obstacle.ObstacleId));
				continue;
			}

			if (LaneType == EScenarioSampleLaneType::Blocked)
			{
				AddAdapterDiagnostic(
					Result,
					EScenarioCompileDiagnosticSeverity::Error,
					TEXT("sample_obstacle_on_blocked_surface"),
					FString::Printf(
						TEXT("Static obstacle '%s' resolves onto blocked Corridor lane '%s' surface '%s'."),
						*Obstacle.ObstacleId,
						*LaneId,
						*SurfaceId));
				continue;
			}

			Pose.LocationCm.Z = FScenarioCorridorGeometry::DefaultSurfaceTopZCm + SurfaceZOffsetCm;

			FScenarioPlaceableInstanceSpec ObstacleSpec;
			ObstacleSpec.InstanceId = Obstacle.ObstacleId;
			ObstacleSpec.AssetId = Obstacle.PropId;
			ObstacleSpec.Category = EScenarioActorCategory::StaticObstacle;
			ObstacleSpec.Transform = FTransform(FRotator(0.0, Obstacle.YawDegrees, 0.0), Pose.LocationCm);
			ObstacleSpec.Properties.Add(TEXT("placed_by"), MakeRuntimeStringParam(Obstacle.PlacedBy));
			ObstacleSpec.Properties.Add(TEXT("perception_tag"), MakeRuntimeStringParam(Obstacle.PerceptionTag));
			ObstacleSpec.Properties.Add(
				TEXT("obstacle_class"),
				MakeRuntimeStringParam(
					Obstacle.ObstacleClass == EScenarioSampleObstacleClass::TraversableCost
						? TEXT("traversable_cost")
						: TEXT("blocking")));
			ObstacleSpec.Properties.Add(TEXT("sensor_profile"), MakeRuntimeStringParam(Obstacle.SensorProfile));
			ObstacleSpec.Properties.Add(TEXT("clear_width_remaining_m"), MakeRuntimeFloatParam(Obstacle.ClearWidthRemainingMeters));
			ObstacleSpec.Properties.Add(TEXT("sample_along_m"), MakeRuntimeFloatParam(Obstacle.AlongMeters));
			ObstacleSpec.Properties.Add(TEXT("sample_offset_m"), MakeRuntimeFloatParam(Obstacle.OffsetMeters));
			WorldSpec.Placeables.Add(ObstacleSpec);
		}
	}

	FVector TransformSamplePointMetersToCm(const FScenarioSampleSemantic& Semantic, const FVector2D& PointMeters)
	{
		const FVector2D WorldPointMeters =
			Semantic.RouteAxis.OriginXYMeters + RotateSamplePoint(PointMeters, Semantic.RouteAxis.HeadingDegrees);
		return FVector(
			WorldPointMeters.X * MetersToCentimeters,
			WorldPointMeters.Y * MetersToCentimeters,
			ResolveRuntimeSurfaceTopZCm(Semantic, PointMeters.X, PointMeters.Y));
	}

	void AddPedestriansFromSample(
		const FScenarioSampleSemantic& Semantic,
		FScenarioCompileResult& Result,
		FScenarioWorldSpec& WorldSpec)
	{
		for (const FScenarioSamplePedestrian& Pedestrian : Semantic.Pedestrians)
		{
			FResolvedSamplePose StartPose;
			if (!ResolveSampleAxisPose(
					Semantic.RouteAxis,
					Pedestrian.Baseline.StartAlongMeters,
					Pedestrian.Baseline.StartOffsetMeters,
					StartPose))
			{
				AddAdapterDiagnostic(
					Result,
					EScenarioCompileDiagnosticSeverity::Error,
					TEXT("sample_pedestrian_pose_failed"),
					FString::Printf(TEXT("Failed to resolve pedestrian '%s' start pose."), *Pedestrian.PedestrianId));
				continue;
			}

			StartPose.LocationCm.Z = ResolveRuntimeSurfaceTopZCm(
				Semantic,
				Pedestrian.Baseline.StartAlongMeters,
				Pedestrian.Baseline.StartOffsetMeters);

			FScenarioDynamicActorSpec ActorSpec;
			ActorSpec.InstanceId = Pedestrian.PedestrianId;
			ActorSpec.AssetId = Pedestrian.PersonaId.IsEmpty() ? TEXT("adult_pedestrian") : Pedestrian.PersonaId;
			ActorSpec.Category = EScenarioActorCategory::Pedestrian;
			ActorSpec.InitialTransform = FTransform(FRotator(0.0, StartPose.YawDegrees, 0.0), StartPose.LocationCm);
			ActorSpec.PathId = FString::Printf(TEXT("%s_baseline"), *Pedestrian.PedestrianId);
			ActorSpec.Properties.Add(TEXT("movement_model"), MakeRuntimeStringParam(TEXT("spline_Relative")));
			ActorSpec.Properties.Add(TEXT("speed_mps"), MakeRuntimeFloatParam(Pedestrian.SpeedMetersPerSecond));
			ActorSpec.Properties.Add(TEXT("speed_cm_per_second"), MakeRuntimeFloatParam(Pedestrian.SpeedMetersPerSecond * MetersToCentimeters));
			ActorSpec.Properties.Add(TEXT("auto_start"), MakeRuntimeBoolParam(true));
			ActorSpec.Properties.Add(
				TEXT("role"),
				MakeRuntimeStringParam(Pedestrian.Role == EScenarioSamplePedestrianRole::Encounter ? TEXT("encounter") : TEXT("background")));
			ActorSpec.Properties.Add(TEXT("placed_by"), MakeRuntimeStringParam(Pedestrian.PlacedBy));
			ActorSpec.Properties.Add(TEXT("persona"), MakeRuntimeStringParam(Pedestrian.PersonaId));
			ActorSpec.Properties.Add(TEXT("pedestrian_scenario_hash"), MakeRuntimeStringParam(Pedestrian.PedestrianScenarioHash));
			ActorSpec.Properties.Add(TEXT("behavior_cooperation"), MakeRuntimeFloatParam(Pedestrian.Behavior.Cooperation));
			ActorSpec.Properties.Add(TEXT("behavior_evasiveness"), MakeRuntimeFloatParam(Pedestrian.Behavior.Evasiveness));
			ActorSpec.Properties.Add(TEXT("behavior_personal_space_cm"), MakeRuntimeFloatParam(Pedestrian.Behavior.PersonalSpaceMeters * MetersToCentimeters));
			ActorSpec.Properties.Add(TEXT("behavior_awareness_horizon_s"), MakeRuntimeFloatParam(Pedestrian.Behavior.AwarenessHorizonSeconds));
			ActorSpec.Properties.Add(TEXT("behavior_max_yield_wait_s"), MakeRuntimeFloatParam(Pedestrian.Behavior.MaxYieldWaitSeconds));
			ActorSpec.Properties.Add(TEXT("behavior_sidestep_distance_cm"), MakeRuntimeFloatParam(Pedestrian.Behavior.SidestepDistanceMeters * MetersToCentimeters));
			WorldSpec.DynamicActors.Add(ActorSpec);

			FScenarioPathSpec PathSpec;
			PathSpec.PathId = ActorSpec.PathId;
			PathSpec.PathType = EScenarioPathType::Spline;
			if (Pedestrian.Baseline.PointsMeters.Num() >= 2)
			{
				for (const FVector2D& PointMeters : Pedestrian.Baseline.PointsMeters)
				{
					PathSpec.Points.Add(TransformSamplePointMetersToCm(Semantic, PointMeters));
				}
			}
			else
			{
				PathSpec.Points.Add(StartPose.LocationCm);
				FResolvedSamplePose GoalPose;
				if (ResolveSampleAxisPose(
						Semantic.RouteAxis,
						Pedestrian.Baseline.GoalAlongMeters,
						Pedestrian.Baseline.GoalOffsetMeters,
						GoalPose))
				{
					GoalPose.LocationCm.Z = ResolveRuntimeSurfaceTopZCm(
						Semantic,
						Pedestrian.Baseline.GoalAlongMeters,
						Pedestrian.Baseline.GoalOffsetMeters);
					PathSpec.Points.Add(GoalPose.LocationCm);
				}
			}

			if (PathSpec.Points.Num() >= 2)
			{
				WorldSpec.Paths.Add(PathSpec);
			}
		}
	}

	bool HasErrorDiagnostics(const TArray<FScenarioCompileDiagnostic>& Diagnostics)
	{
		for (const FScenarioCompileDiagnostic& Diagnostic : Diagnostics)
		{
			if (Diagnostic.Severity == EScenarioCompileDiagnosticSeverity::Error)
			{
				return true;
			}
		}

		return false;
	}
}

FScenarioCompileResult FScenarioSampleWorldSpecAdapter::CompileScenarioWorldSpecFromSampleDocument(
	const FScenarioSampleDocument& Document)
{
	FScenarioCompileResult Result;

	TArray<FScenarioSchemaDiagnostic> ValidationDiagnostics;
	if (!FScenarioSampleJson::ValidateDocument(Document, ValidationDiagnostics))
	{
		AppendSchemaDiagnostics(ValidationDiagnostics, Result);
		Result.bSuccess = false;
		return Result;
	}
	AppendSchemaDiagnostics(ValidationDiagnostics, Result);

	FScenarioWorldSpec WorldSpec;
	PopulateRunConfig(Document, Result, WorldSpec);
	AddCorridorsFromSample(Document.Scenario.Semantic, Result, WorldSpec);
	AddGeneratedCityGroundRegionsFromSample(Document.Scenario.Semantic, WorldSpec);
	AddRobotFromSample(Document.Scenario.Semantic, Result, WorldSpec);
	AddStaticObstaclesFromSample(Document.Scenario.Semantic, Result, WorldSpec);
	AddPedestriansFromSample(Document.Scenario.Semantic, Result, WorldSpec);
	WorldSpec.SpecHash = MakeAdapterSpecHash(Document);

	Result.WorldSpec = WorldSpec;
	Result.bSuccess = !HasErrorDiagnostics(Result.Diagnostics);
	return Result;
}
