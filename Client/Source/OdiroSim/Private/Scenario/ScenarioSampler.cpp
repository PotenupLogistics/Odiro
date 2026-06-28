#include "Scenario/ScenarioSampler.h"

#include "Misc/Crc.h"
#include "Scenario/Data/ScenarioCorridorSurfaceCatalog.h"
#include "Shared/ScenarioSampleJson.h"
#include "Shared/ScenarioDocumentJson.h"

const TCHAR* FScenarioSampler::GeneratorVersion = TEXT("scenario_sampler_v1");

namespace
{
	const double ScenarioSamplerMetersToCentimeters = 100.0;
	// Tolerance used when matching sampled obstacle poses to resolved Corridor lane intervals.
	const double ScenarioSamplerSurfaceToleranceMeters = 0.001;

	struct FScenarioSamplerResolvedLane
	{
		FString SegmentId;
		FString LaneId;
		FString SurfaceId;
		FScenarioAlongRangeMeters AlongRangeMeters;
		FScenarioOffsetRangeMeters OffsetRangeMeters;
		EScenarioSampleLaneType Type = EScenarioSampleLaneType::Walkable;
	};

	void ScenarioSamplerAddDiagnostic(
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		EScenarioSchemaDiagnosticSeverity Severity,
		const FString& Code,
		const FString& Path,
		const FString& Message)
	{
		FScenarioSchemaDiagnostic Diagnostic;
		Diagnostic.Severity = Severity;
		Diagnostic.Code = Code;
		Diagnostic.Path = Path;
		Diagnostic.Message = Message;
		Diagnostics.Add(Diagnostic);
	}

	bool ScenarioSamplerHasErrors(const TArray<FScenarioSchemaDiagnostic>& Diagnostics)
	{
		for (const FScenarioSchemaDiagnostic& Diagnostic : Diagnostics)
		{
			if (Diagnostic.Severity == EScenarioSchemaDiagnosticSeverity::Error)
			{
				return true;
			}
		}

		return false;
	}

	bool ScenarioSamplerIsEmptyString(const FString& Value)
	{
		return Value.TrimStartAndEnd().IsEmpty();
	}

	uint32 ScenarioSamplerMakeScopedSeed(int64 Seed, const FString& Scope)
	{
		const FString SeedSource = FString::Printf(TEXT("%lld:%s"), Seed, *Scope);
		return FCrc::StrCrc32(*SeedSource);
	}

	FRandomStream ScenarioSamplerMakeStream(int64 Seed, const FString& Scope)
	{
		return FRandomStream(static_cast<int32>(ScenarioSamplerMakeScopedSeed(Seed, Scope)));
	}

	FScenarioSampleParamValue ScenarioSamplerMakeFloatParam(double Value)
	{
		FScenarioSampleParamValue ParamValue;
		ParamValue.Type = EScenarioSampleParamValueType::Float;
		ParamValue.FloatValue = Value;
		return ParamValue;
	}

	FScenarioSampleParamValue ScenarioSamplerMakeIntegerParam(int32 Value)
	{
		FScenarioSampleParamValue ParamValue;
		ParamValue.Type = EScenarioSampleParamValueType::Integer;
		ParamValue.IntegerValue = Value;
		return ParamValue;
	}

	FScenarioSampleParamValue ScenarioSamplerMakeStringParam(const FString& Value)
	{
		FScenarioSampleParamValue ParamValue;
		ParamValue.Type = EScenarioSampleParamValueType::String;
		ParamValue.StringValue = Value;
		return ParamValue;
	}

	double ScenarioSamplerResolveNumber(
		const FScenarioTemplateNumberValue& Value,
		double DefaultValue,
		const FString& ParamKey,
		int64 Seed,
		TMap<FString, FScenarioSampleParamValue>& Params)
	{
		if (!Value.bIsSet)
		{
			return DefaultValue;
		}

		if (Value.Mode == EScenarioTemplateNumberValueMode::Range)
		{
			const double MinValue = FMath::Min(Value.MinValue, Value.MaxValue);
			const double MaxValue = FMath::Max(Value.MinValue, Value.MaxValue);
			FRandomStream Stream = ScenarioSamplerMakeStream(Seed, ParamKey);
			const double ResolvedValue = FMath::Lerp(MinValue, MaxValue, static_cast<double>(Stream.GetFraction()));
			Params.Add(ParamKey, ScenarioSamplerMakeFloatParam(ResolvedValue));
			return ResolvedValue;
		}

		return Value.FixedValue;
	}

	// Matches editor preview projection for corridor geometry so simulation surfaces use the same lane widths.
	double ScenarioSamplerResolveGeometryNumber(
		const FScenarioTemplateNumberValue& Value,
		double DefaultValue,
		const FString& ParamKey,
		TMap<FString, FScenarioSampleParamValue>& Params)
	{
		if (!Value.bIsSet)
		{
			return DefaultValue;
		}

		if (Value.Mode == EScenarioTemplateNumberValueMode::Range)
		{
			const double ResolvedValue = (Value.MinValue + Value.MaxValue) * 0.5;
			Params.Add(ParamKey, ScenarioSamplerMakeFloatParam(ResolvedValue));
			return ResolvedValue;
		}

		return Value.FixedValue;
	}

	int32 ScenarioSamplerResolveInteger(
		const FScenarioTemplateIntegerValue& Value,
		int32 DefaultValue,
		const FString& ParamKey,
		int64 Seed,
		TMap<FString, FScenarioSampleParamValue>& Params)
	{
		if (!Value.bIsSet)
		{
			return DefaultValue;
		}

		if (Value.Mode == EScenarioTemplateNumberValueMode::Range)
		{
			const int32 MinValue = FMath::Min(Value.MinValue, Value.MaxValue);
			const int32 MaxValue = FMath::Max(Value.MinValue, Value.MaxValue);
			FRandomStream Stream = ScenarioSamplerMakeStream(Seed, ParamKey);
			const int32 ResolvedValue = Stream.RandRange(MinValue, MaxValue);
			Params.Add(ParamKey, ScenarioSamplerMakeIntegerParam(ResolvedValue));
			return ResolvedValue;
		}

		return Value.FixedValue;
	}

	FString ScenarioSamplerResolveString(
		const FScenarioTemplateStringValue& Value,
		const FString& DefaultValue,
		const FString& ParamKey,
		int64 Seed,
		TMap<FString, FScenarioSampleParamValue>& Params)
	{
		if (!Value.bIsSet)
		{
			return DefaultValue;
		}

		if (Value.Mode == EScenarioTemplateStringValueMode::Choices && !Value.Choices.IsEmpty())
		{
			FRandomStream Stream = ScenarioSamplerMakeStream(Seed, ParamKey);
			const FString ResolvedValue = Value.Choices[Stream.RandRange(0, Value.Choices.Num() - 1)];
			Params.Add(ParamKey, ScenarioSamplerMakeStringParam(ResolvedValue));
			return ResolvedValue;
		}

		return Value.FixedValue;
	}

	double ScenarioSamplerMeasureAxisLength(const TArray<FVector2D>& PointsMeters)
	{
		double LengthMeters = 0.0;
		for (int32 Index = 0; Index < PointsMeters.Num() - 1; ++Index)
		{
			LengthMeters += (PointsMeters[Index + 1] - PointsMeters[Index]).Size();
		}

		return LengthMeters;
	}

	bool ScenarioSamplerResolveAxisPose(
		const TArray<FVector2D>& PointsMeters,
		double AlongMeters,
		double OffsetMeters,
		FVector2D& OutPointMeters,
		double& OutYawDegrees)
	{
		if (PointsMeters.Num() < 2)
		{
			return false;
		}

		double RemainingMeters = FMath::Max(AlongMeters, 0.0);
		FVector2D Direction = PointsMeters[1] - PointsMeters[0];
		FVector2D Point = PointsMeters[0];

		for (int32 Index = 0; Index < PointsMeters.Num() - 1; ++Index)
		{
			const FVector2D SegmentStart = PointsMeters[Index];
			const FVector2D SegmentEnd = PointsMeters[Index + 1];
			const FVector2D SegmentVector = SegmentEnd - SegmentStart;
			const double SegmentLength = SegmentVector.Size();
			if (SegmentLength <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			Direction = SegmentVector / SegmentLength;
			if (RemainingMeters <= SegmentLength || Index == PointsMeters.Num() - 2)
			{
				const double SegmentDistance = FMath::Clamp(RemainingMeters, 0.0, SegmentLength);
				Point = SegmentStart + (Direction * SegmentDistance);
				break;
			}

			RemainingMeters -= SegmentLength;
		}

		const FVector2D Normal(-Direction.Y, Direction.X);
		OutPointMeters = Point + (Normal * OffsetMeters);
		OutYawDegrees = FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
		return true;
	}

	EScenarioSampleLaneType ScenarioSamplerSurfaceToLaneType(const FString& SurfaceId)
	{
		FScenarioCorridorSurfaceEntry SurfaceEntry;
		if (UScenarioCorridorSurfaceCatalog::FindDefaultSurfaceEntryById(FName(*SurfaceId), SurfaceEntry))
		{
			return SurfaceEntry.LaneType;
		}

		const FString Normalized = SurfaceId.ToLower();
		if (Normalized.Contains(TEXT("building")) || Normalized.Contains(TEXT("block")))
		{
			return EScenarioSampleLaneType::Blocked;
		}
		if (Normalized.Contains(TEXT("road")) || Normalized.Contains(TEXT("curb")))
		{
			return EScenarioSampleLaneType::Penalty;
		}

		return EScenarioSampleLaneType::Walkable;
	}

	const FScenarioTemplateSegment* ScenarioSamplerFindSegment(
		const FScenarioDocument& ScenarioDocument,
		const FString& SegmentId)
	{
		return ScenarioDocument.Corridor.Segments.FindByPredicate(
			[&SegmentId](const FScenarioTemplateSegment& Segment)
			{
				return Segment.SegmentId == SegmentId;
			});
	}

	const FScenarioSamplerResolvedLane* ScenarioSamplerFindLane(
		const TArray<FScenarioSamplerResolvedLane>& Lanes,
		const FString& SegmentId,
		const FString& LaneId)
	{
		return Lanes.FindByPredicate(
			[&SegmentId, &LaneId](const FScenarioSamplerResolvedLane& Lane)
			{
				return Lane.SegmentId == SegmentId && Lane.LaneId == LaneId;
			});
	}

	// Returns true when a sampled scalar belongs to a Corridor interval after tolerance.
	bool ScenarioSamplerContainsRangeValue(double Value, double MinValue, double MaxValue)
	{
		const double SafeMin = FMath::Min(MinValue, MaxValue) - ScenarioSamplerSurfaceToleranceMeters;
		const double SafeMax = FMath::Max(MinValue, MaxValue) + ScenarioSamplerSurfaceToleranceMeters;
		return Value >= SafeMin && Value <= SafeMax;
	}

	// Finds the concrete resolved lane that owns a sampled Corridor-local pose.
	const FScenarioSamplerResolvedLane* ScenarioSamplerFindLaneAtPose(
		const TArray<FScenarioSamplerResolvedLane>& Lanes,
		const FString& SegmentId,
		double AlongMeters,
		double OffsetMeters)
	{
		return Lanes.FindByPredicate(
			[&SegmentId, AlongMeters, OffsetMeters](const FScenarioSamplerResolvedLane& Lane)
			{
				return Lane.SegmentId == SegmentId
					&& ScenarioSamplerContainsRangeValue(
						AlongMeters,
						Lane.AlongRangeMeters.StartMeters,
						Lane.AlongRangeMeters.EndMeters)
					&& ScenarioSamplerContainsRangeValue(
						OffsetMeters,
						Lane.OffsetRangeMeters.MinMeters,
						Lane.OffsetRangeMeters.MaxMeters);
			});
	}

	bool ScenarioSamplerResolveLaneOffset(
		const TArray<FScenarioSamplerResolvedLane>& Lanes,
		const FString& SegmentId,
		const FString& LaneId,
		double LocalOffsetMeters,
		double& OutAxisOffsetMeters)
	{
		if (LaneId.IsEmpty())
		{
			OutAxisOffsetMeters = LocalOffsetMeters;
			return true;
		}

		const FString NormalizedLane = LaneId.ToLower();
		const FScenarioSamplerResolvedLane* WalkwayLane = ScenarioSamplerFindLane(Lanes, SegmentId, TEXT("walkway"));
		if (NormalizedLane == TEXT("center") || NormalizedLane == TEXT("across"))
		{
			OutAxisOffsetMeters = LocalOffsetMeters;
			return true;
		}
		if (WalkwayLane && NormalizedLane == TEXT("building_edge"))
		{
			OutAxisOffsetMeters = WalkwayLane->OffsetRangeMeters.MinMeters + LocalOffsetMeters;
			return true;
		}
		if (WalkwayLane && NormalizedLane == TEXT("curb_edge"))
		{
			OutAxisOffsetMeters = WalkwayLane->OffsetRangeMeters.MaxMeters + LocalOffsetMeters;
			return true;
		}

		if (const FScenarioSamplerResolvedLane* Lane = ScenarioSamplerFindLane(Lanes, SegmentId, LaneId))
		{
			const double LaneCenter = (Lane->OffsetRangeMeters.MinMeters + Lane->OffsetRangeMeters.MaxMeters) * 0.5;
			OutAxisOffsetMeters = LaneCenter + LocalOffsetMeters;
			return true;
		}

		return false;
	}

	void ScenarioSamplerResolveLayout(
		const FScenarioDocument& ScenarioDocument,
		int64 Seed,
		TMap<FString, FScenarioSampleParamValue>& Params,
		TArray<FScenarioSampleLayoutEntry>& OutLayout,
		TArray<FScenarioSamplerResolvedLane>& OutLanes)
	{
		OutLayout.Reset();
		OutLanes.Reset();

		const double WalkwayWidthMeters = ScenarioSamplerResolveGeometryNumber(
			ScenarioDocument.Corridor.WalkwayWidthMeters,
			3.0,
			TEXT("corridor.walkway_width_m"),
			Params);
		const double HalfWalkwayWidthMeters = WalkwayWidthMeters * 0.5;

		for (const FScenarioTemplateSegment& Segment : ScenarioDocument.Corridor.Segments)
		{
			FScenarioSampleLayoutEntry LayoutEntry;
			LayoutEntry.SegmentId = Segment.SegmentId;
			LayoutEntry.AlongRangeMeters = Segment.AlongRangeMeters;

			FScenarioSampleLayoutLane WalkwayLane;
			WalkwayLane.LaneId = TEXT("walkway");
			WalkwayLane.OffsetRangeMeters.MinMeters = -HalfWalkwayWidthMeters;
			WalkwayLane.OffsetRangeMeters.MaxMeters = HalfWalkwayWidthMeters;
			WalkwayLane.SurfaceId = ScenarioSamplerResolveString(
				Segment.ReplacedBySurfaceId,
				TEXT("walkway"),
				FString::Printf(TEXT("corridor.segments.%s.replaced_by"), *Segment.SegmentId),
				Seed,
				Params);
			WalkwayLane.Type = ScenarioSamplerSurfaceToLaneType(WalkwayLane.SurfaceId);
			LayoutEntry.Lanes.Add(WalkwayLane);

			FScenarioSamplerResolvedLane WalkwayRegion;
			WalkwayRegion.SegmentId = Segment.SegmentId;
			WalkwayRegion.LaneId = WalkwayLane.LaneId;
			WalkwayRegion.SurfaceId = WalkwayLane.SurfaceId;
			WalkwayRegion.AlongRangeMeters = Segment.AlongRangeMeters;
			WalkwayRegion.OffsetRangeMeters = WalkwayLane.OffsetRangeMeters;
			WalkwayRegion.Type = WalkwayLane.Type;
			OutLanes.Add(WalkwayRegion);

			double BuildingMaxOffset = -HalfWalkwayWidthMeters;
			for (int32 Index = 0; Index < ScenarioDocument.Corridor.BuildingSide.Num(); ++Index)
			{
				const FScenarioTemplateLaneRule& LaneRule = ScenarioDocument.Corridor.BuildingSide[Index];
				const FString ParamKey = FString::Printf(TEXT("corridor.building_side[%d].width_m"), Index);
				const double WidthMeters = ScenarioSamplerResolveGeometryNumber(LaneRule.WidthMeters, 0.0, ParamKey, Params);
				if (WidthMeters <= KINDA_SMALL_NUMBER)
				{
					continue;
				}

				FScenarioSampleLayoutLane Lane;
				Lane.LaneId = Index == 0 ? TEXT("building_edge") : FString::Printf(TEXT("building_%d"), Index);
				Lane.OffsetRangeMeters.MinMeters = BuildingMaxOffset - WidthMeters;
				Lane.OffsetRangeMeters.MaxMeters = BuildingMaxOffset;
				Lane.SurfaceId = LaneRule.SurfaceId;
				Lane.Type = ScenarioSamplerSurfaceToLaneType(Lane.SurfaceId);
				LayoutEntry.Lanes.Add(Lane);
				BuildingMaxOffset -= WidthMeters;

				FScenarioSamplerResolvedLane Region;
				Region.SegmentId = Segment.SegmentId;
				Region.LaneId = Lane.LaneId;
				Region.SurfaceId = Lane.SurfaceId;
				Region.AlongRangeMeters = Segment.AlongRangeMeters;
				Region.OffsetRangeMeters = Lane.OffsetRangeMeters;
				Region.Type = Lane.Type;
				OutLanes.Add(Region);
			}

			double CurbMinOffset = HalfWalkwayWidthMeters;
			for (int32 Index = 0; Index < ScenarioDocument.Corridor.CurbSide.Num(); ++Index)
			{
				const FScenarioTemplateLaneRule& LaneRule = ScenarioDocument.Corridor.CurbSide[Index];
				const FString ParamKey = FString::Printf(TEXT("corridor.curb_side[%d].width_m"), Index);
				const double WidthMeters = ScenarioSamplerResolveGeometryNumber(LaneRule.WidthMeters, 0.0, ParamKey, Params);
				if (WidthMeters <= KINDA_SMALL_NUMBER)
				{
					continue;
				}

				FScenarioSampleLayoutLane Lane;
				Lane.LaneId = Index == 0 ? TEXT("curb_edge") : FString::Printf(TEXT("curb_%d"), Index);
				Lane.OffsetRangeMeters.MinMeters = CurbMinOffset;
				Lane.OffsetRangeMeters.MaxMeters = CurbMinOffset + WidthMeters;
				Lane.SurfaceId = LaneRule.SurfaceId;
				Lane.Type = ScenarioSamplerSurfaceToLaneType(Lane.SurfaceId);
				LayoutEntry.Lanes.Add(Lane);
				CurbMinOffset += WidthMeters;

				FScenarioSamplerResolvedLane Region;
				Region.SegmentId = Segment.SegmentId;
				Region.LaneId = Lane.LaneId;
				Region.SurfaceId = Lane.SurfaceId;
				Region.AlongRangeMeters = Segment.AlongRangeMeters;
				Region.OffsetRangeMeters = Lane.OffsetRangeMeters;
				Region.Type = Lane.Type;
				OutLanes.Add(Region);
			}

			OutLayout.Add(LayoutEntry);
		}
	}

	void ScenarioSamplerResolveRobotPose(
		const FScenarioDocument& ScenarioDocument,
		const FScenarioTemplateRobotAnchor& Anchor,
		bool bGoalAnchor,
		int64 Seed,
		TMap<FString, FScenarioSampleParamValue>& Params,
		FScenarioSampleRobotPose& OutPose)
	{
		const FScenarioTemplateSegment* Segment = nullptr;
		if (Anchor.Type == EScenarioTemplateRobotAnchorType::CorridorPose)
		{
			Segment = ScenarioSamplerFindSegment(ScenarioDocument, Anchor.SegmentId);
		}
		if (!Segment && !ScenarioDocument.Corridor.Segments.IsEmpty())
		{
			Segment = bGoalAnchor ? &ScenarioDocument.Corridor.Segments.Last() : &ScenarioDocument.Corridor.Segments[0];
		}

		const double DefaultAlongMeters = Segment
			? (bGoalAnchor ? Segment->AlongRangeMeters.EndMeters : Segment->AlongRangeMeters.StartMeters)
			: 0.0;
		OutPose.SegmentId = Segment ? Segment->SegmentId : Anchor.SegmentId;
		const FString ParamPrefix = bGoalAnchor ? TEXT("robot.goal") : TEXT("robot.start");
		OutPose.AlongMeters = Anchor.Type == EScenarioTemplateRobotAnchorType::CorridorPose
			? ScenarioSamplerResolveNumber(Anchor.AlongMeters, DefaultAlongMeters, FString::Printf(TEXT("%s.along_m"), *ParamPrefix), Seed, Params)
			: DefaultAlongMeters;
		OutPose.OffsetMeters = Anchor.Type == EScenarioTemplateRobotAnchorType::CorridorPose
			? ScenarioSamplerResolveNumber(Anchor.OffsetMeters, 0.0, FString::Printf(TEXT("%s.offset_m"), *ParamPrefix), Seed, Params)
			: 0.0;
		OutPose.LaneId = Anchor.LaneId.IsEmpty() ? TEXT("walkway") : Anchor.LaneId;
		OutPose.SourceAnchorType = Anchor.Type;

		FVector2D PointMeters;
		double YawDegrees = 0.0;
		ScenarioSamplerResolveAxisPose(ScenarioDocument.Corridor.Axis.PointsMeters, OutPose.AlongMeters, OutPose.OffsetMeters, PointMeters, YawDegrees);
		if (Anchor.Heading == EScenarioTemplateRobotHeading::Backward)
		{
			YawDegrees += 180.0;
		}
		OutPose.HeadingDegrees = FRotator::ClampAxis(YawDegrees);
	}

	void ScenarioSamplerResolveFixedObstacles(
		const FScenarioDocument& ScenarioDocument,
		const TArray<FScenarioSamplerResolvedLane>& Lanes,
		int64 Seed,
		double MinClearWidthMeters,
		TMap<FString, FScenarioSampleParamValue>& Params,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		TArray<FScenarioSampleStaticObstacle>& OutObstacles)
	{
		OutObstacles.Reset();
		for (const FScenarioTemplateObstaclePlacement& Placement : ScenarioDocument.Obstacles.Placements)
		{
			if (Placement.Kind != EScenarioTemplateObstaclePlacementKind::Fixed)
			{
				continue;
			}

			const FString PlacementPath = FString::Printf(TEXT("$.obstacles.placements.%s"), *Placement.PlacementId);
			const FScenarioTemplateSegment* Segment = ScenarioSamplerFindSegment(ScenarioDocument, Placement.At.SegmentId);
			if (!Segment)
			{
				ScenarioSamplerAddDiagnostic(
					Diagnostics,
					EScenarioSchemaDiagnosticSeverity::Error,
					TEXT("unknown_obstacle_segment"),
					FString::Printf(TEXT("%s.at.segment"), *PlacementPath),
					FString::Printf(TEXT("Fixed obstacle placement '%s' references unknown segment '%s'."), *Placement.PlacementId, *Placement.At.SegmentId));
				continue;
			}

			const FString ParamPrefix = FString::Printf(TEXT("obstacles.%s"), *Placement.PlacementId);
			const double DefaultAlongMeters = (Segment->AlongRangeMeters.StartMeters + Segment->AlongRangeMeters.EndMeters) * 0.5;
			const double AlongMeters = ScenarioSamplerResolveNumber(
				Placement.At.AlongMeters,
				DefaultAlongMeters,
				FString::Printf(TEXT("%s.at.along_m"), *ParamPrefix),
				Seed,
				Params);
			const double LocalOffsetMeters = ScenarioSamplerResolveNumber(
				Placement.At.OffsetMeters,
				0.0,
				FString::Printf(TEXT("%s.at.offset_m"), *ParamPrefix),
				Seed,
				Params);
			const double YawLocalDegrees = ScenarioSamplerResolveNumber(
				Placement.YawDegrees,
				0.0,
				FString::Printf(TEXT("%s.yaw_deg"), *ParamPrefix),
				Seed,
				Params);

			if (!ScenarioSamplerContainsRangeValue(
					AlongMeters,
					Segment->AlongRangeMeters.StartMeters,
					Segment->AlongRangeMeters.EndMeters))
			{
				ScenarioSamplerAddDiagnostic(
					Diagnostics,
					EScenarioSchemaDiagnosticSeverity::Error,
					TEXT("obstacle_along_outside_segment"),
					FString::Printf(TEXT("%s.at.along_m"), *PlacementPath),
					FString::Printf(
						TEXT("Fixed obstacle placement '%s' resolved along %.2fm outside segment '%s' range %.2f..%.2fm."),
						*Placement.PlacementId,
						AlongMeters,
						*Segment->SegmentId,
						Segment->AlongRangeMeters.StartMeters,
						Segment->AlongRangeMeters.EndMeters));
				continue;
			}

			double AxisOffsetMeters = 0.0;
			if (!ScenarioSamplerResolveLaneOffset(Lanes, Segment->SegmentId, Placement.At.LaneId, LocalOffsetMeters, AxisOffsetMeters))
			{
				ScenarioSamplerAddDiagnostic(
					Diagnostics,
					EScenarioSchemaDiagnosticSeverity::Error,
					TEXT("unknown_obstacle_lane"),
					FString::Printf(TEXT("%s.at.lane"), *PlacementPath),
					FString::Printf(TEXT("Fixed obstacle placement '%s' references unknown lane '%s' in segment '%s'."), *Placement.PlacementId, *Placement.At.LaneId, *Segment->SegmentId));
				continue;
			}

			const FScenarioSamplerResolvedLane* ResolvedLane =
				ScenarioSamplerFindLaneAtPose(Lanes, Segment->SegmentId, AlongMeters, AxisOffsetMeters);
			if (!ResolvedLane)
			{
				ScenarioSamplerAddDiagnostic(
					Diagnostics,
					EScenarioSchemaDiagnosticSeverity::Error,
					TEXT("obstacle_outside_corridor_surface"),
					FString::Printf(TEXT("%s.at.offset_m"), *PlacementPath),
					FString::Printf(
						TEXT("Fixed obstacle placement '%s' resolved to offset %.2fm outside Corridor surfaces in segment '%s'."),
						*Placement.PlacementId,
						AxisOffsetMeters,
						*Segment->SegmentId));
				continue;
			}

			if (ResolvedLane->Type == EScenarioSampleLaneType::Blocked)
			{
				ScenarioSamplerAddDiagnostic(
					Diagnostics,
					EScenarioSchemaDiagnosticSeverity::Error,
					TEXT("obstacle_on_blocked_surface"),
					FString::Printf(TEXT("%s.at.lane"), *PlacementPath),
					FString::Printf(
						TEXT("Fixed obstacle placement '%s' resolved onto blocked Corridor lane '%s' surface '%s'."),
						*Placement.PlacementId,
						*ResolvedLane->LaneId,
						*ResolvedLane->SurfaceId));
				continue;
			}

			const FScenarioSamplerResolvedLane* WalkwayLane =
				ScenarioSamplerFindLane(Lanes, Segment->SegmentId, TEXT("walkway"));
			const double WalkwayWidthMeters = WalkwayLane
				? WalkwayLane->OffsetRangeMeters.MaxMeters - WalkwayLane->OffsetRangeMeters.MinMeters
				: 0.0;

			FVector2D PointMeters;
			double AxisYawDegrees = 0.0;
			if (!ScenarioSamplerResolveAxisPose(ScenarioDocument.Corridor.Axis.PointsMeters, AlongMeters, AxisOffsetMeters, PointMeters, AxisYawDegrees))
			{
				ScenarioSamplerAddDiagnostic(
					Diagnostics,
					EScenarioSchemaDiagnosticSeverity::Error,
					TEXT("obstacle_axis_pose_failed"),
					PlacementPath,
					FString::Printf(TEXT("Fixed obstacle placement '%s' could not be resolved on the route axis."), *Placement.PlacementId));
				continue;
			}

			FScenarioSampleStaticObstacle Obstacle;
			Obstacle.ObstacleId = Placement.PlacementId;
			Obstacle.PropId = Placement.PropId;
			Obstacle.PerceptionTag = Placement.PropId.IsEmpty() ? TEXT("static_obstacle") : Placement.PropId;
			Obstacle.ObstacleClass = EScenarioSampleObstacleClass::Blocking;
			Obstacle.SensorProfile = TEXT("solid");
			Obstacle.AlongMeters = AlongMeters;
			Obstacle.OffsetMeters = AxisOffsetMeters;
			Obstacle.YawDegrees = FRotator::ClampAxis(AxisYawDegrees + YawLocalDegrees);
			Obstacle.FootprintMeters = FVector2D(0.5, 0.5);
			Obstacle.PlacedBy = Placement.PlacementId;
			Obstacle.ClearWidthRemainingMeters = FMath::Max(0.0, WalkwayWidthMeters - Obstacle.FootprintMeters.Y);

			if (!Placement.bAllowBlocking
				&& MinClearWidthMeters > KINDA_SMALL_NUMBER
				&& Obstacle.ClearWidthRemainingMeters + KINDA_SMALL_NUMBER < MinClearWidthMeters)
			{
				ScenarioSamplerAddDiagnostic(
					Diagnostics,
					EScenarioSchemaDiagnosticSeverity::Error,
					TEXT("min_clear_width_violation"),
					PlacementPath,
					FString::Printf(
						TEXT("Fixed obstacle placement '%s' leaves %.2fm clear width below required %.2fm."),
						*Placement.PlacementId,
						Obstacle.ClearWidthRemainingMeters,
						MinClearWidthMeters));
			}

			OutObstacles.Add(Obstacle);
		}
	}

	void ScenarioSamplerBuildClearWidthProfile(
		const TArray<FScenarioSampleLayoutEntry>& Layout,
		const TArray<FScenarioSampleStaticObstacle>& Obstacles,
		TArray<FScenarioSampleClearWidthEntry>& OutProfile,
		FScenarioSampleSummary& OutSummary)
	{
		OutProfile.Reset();
		OutSummary = FScenarioSampleSummary();
		OutSummary.GlobalMinClearWidthMeters = TNumericLimits<double>::Max();

		for (const FScenarioSampleLayoutEntry& LayoutEntry : Layout)
		{
			const FScenarioSampleLayoutLane* WalkwayLane = LayoutEntry.Lanes.FindByPredicate(
				[](const FScenarioSampleLayoutLane& Lane)
				{
					return Lane.LaneId == TEXT("walkway");
				});
			const double WalkwayWidthMeters = WalkwayLane
				? WalkwayLane->OffsetRangeMeters.MaxMeters - WalkwayLane->OffsetRangeMeters.MinMeters
				: 0.0;

			FScenarioSampleClearWidthEntry Entry;
			Entry.AlongRangeMeters = LayoutEntry.AlongRangeMeters;
			Entry.ClearWidthMeters = WalkwayWidthMeters;
			Entry.LimitedBy = TEXT("layout");

			for (const FScenarioSampleStaticObstacle& Obstacle : Obstacles)
			{
				if (Obstacle.AlongMeters + KINDA_SMALL_NUMBER < LayoutEntry.AlongRangeMeters.StartMeters
					|| Obstacle.AlongMeters - KINDA_SMALL_NUMBER > LayoutEntry.AlongRangeMeters.EndMeters)
				{
					continue;
				}

				const double CandidateClearWidth = FMath::Max(0.0, WalkwayWidthMeters - Obstacle.FootprintMeters.Y);
				if (CandidateClearWidth < Entry.ClearWidthMeters)
				{
					Entry.ClearWidthMeters = CandidateClearWidth;
					Entry.LimitedBy = Obstacle.ObstacleId;
				}
			}

			if (Entry.ClearWidthMeters < OutSummary.GlobalMinClearWidthMeters)
			{
				OutSummary.GlobalMinClearWidthMeters = Entry.ClearWidthMeters;
				OutSummary.MinClearAtAlongMeters = (Entry.AlongRangeMeters.StartMeters + Entry.AlongRangeMeters.EndMeters) * 0.5;
			}
			OutSummary.TotalLengthMeters = FMath::Max(OutSummary.TotalLengthMeters, Entry.AlongRangeMeters.EndMeters);
			OutProfile.Add(Entry);
		}

		if (OutSummary.GlobalMinClearWidthMeters == TNumericLimits<double>::Max())
		{
			OutSummary.GlobalMinClearWidthMeters = 0.0;
		}
	}

	FString ScenarioSamplerResolveRequiredSourceText(
		const FString& Value,
		const FString& Fallback,
		const FString& Path,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics)
	{
		if (!ScenarioSamplerIsEmptyString(Value))
		{
			return Value;
		}

		ScenarioSamplerAddDiagnostic(
			Diagnostics,
			EScenarioSchemaDiagnosticSeverity::Warning,
			TEXT("missing_sample_source"),
			Path,
			FString::Printf(TEXT("%s was empty; sampler recorded '%s'."), *Path, *Fallback));
		return Fallback;
	}
}

FScenarioSamplerResult FScenarioSampler::GenerateSample(
	const FScenarioDocument& ScenarioDocument,
	const FScenarioSamplerRequest& Request)
{
	FScenarioSamplerResult Result;

	TArray<FScenarioSchemaDiagnostic> ScenarioDiagnostics;
	if (!FScenarioDocumentJson::ValidateDocument(ScenarioDocument, ScenarioDiagnostics))
	{
		Result.Diagnostics.Append(ScenarioDiagnostics);
		Result.bSuccess = false;
		return Result;
	}
	Result.Diagnostics.Append(ScenarioDiagnostics);

	FScenarioSampleDocument Document;
	Document.Sample.SampleId = ScenarioSamplerIsEmptyString(Request.SampleId)
		? FString::Printf(TEXT("%06lld"), FMath::Abs(Request.Seed))
		: Request.SampleId;
	Document.Sample.ScenarioId = ScenarioSamplerIsEmptyString(Request.ScenarioId)
		? FString::Printf(TEXT("%s_%s"), *ScenarioDocument.ScenarioId, *Document.Sample.SampleId)
		: Request.ScenarioId;
	Document.Sample.Source.TemplateRef = ScenarioSamplerResolveRequiredSourceText(Request.SourceScenarioRef, TEXT("unspecified_scenario_ref"), TEXT("$.sample.source.template_ref"), Result.Diagnostics);
	Document.Sample.Source.TemplateHash = ScenarioSamplerResolveRequiredSourceText(Request.SourceScenarioHash, TEXT("unspecified_scenario_hash"), TEXT("$.sample.source.template_hash"), Result.Diagnostics);
	Document.Sample.Source.ProfileRef = ScenarioSamplerResolveRequiredSourceText(Request.ProfileRef, TEXT("unspecified_profile_ref"), TEXT("$.sample.source.profile_ref"), Result.Diagnostics);
	Document.Sample.Source.ProfileHash = ScenarioSamplerResolveRequiredSourceText(Request.ProfileHash, TEXT("unspecified_profile_hash"), TEXT("$.sample.source.profile_hash"), Result.Diagnostics);
	Document.Sample.Source.SettingRef = ScenarioSamplerResolveRequiredSourceText(Request.SettingRef, TEXT("unspecified_setting_ref"), TEXT("$.sample.source.setting_ref"), Result.Diagnostics);
	Document.Sample.Source.SettingHash = ScenarioSamplerResolveRequiredSourceText(Request.SettingHash, TEXT("unspecified_setting_hash"), TEXT("$.sample.source.setting_hash"), Result.Diagnostics);
	Document.Sample.Source.Seed = Request.Seed;
	Document.Sample.Source.GeneratorVersion = ScenarioSamplerIsEmptyString(Request.GeneratorVersion)
		? GeneratorVersion
		: Request.GeneratorVersion;

	FScenarioSampleScenario& Scenario = Document.Scenario;
	TArray<FScenarioSamplerResolvedLane> ResolvedLanes;
	ScenarioSamplerResolveLayout(
		ScenarioDocument,
		Request.Seed,
		Scenario.Params,
		Scenario.Semantic.Layout,
		ResolvedLanes);

	Scenario.Semantic.RouteAxis.Type = ScenarioDocument.Corridor.Axis.Type;
	Scenario.Semantic.RouteAxis.OriginXYMeters = FVector2D::ZeroVector;
	Scenario.Semantic.RouteAxis.HeadingDegrees = 0.0;
	Scenario.Semantic.RouteAxis.PointsMeters = ScenarioDocument.Corridor.Axis.PointsMeters;
	Scenario.Semantic.RouteAxis.LengthMeters = ScenarioSamplerMeasureAxisLength(ScenarioDocument.Corridor.Axis.PointsMeters);

	ScenarioSamplerResolveRobotPose(ScenarioDocument, ScenarioDocument.Robot.Start, false, Request.Seed, Scenario.Params, Scenario.Semantic.Robot.Start);
	ScenarioSamplerResolveRobotPose(ScenarioDocument, ScenarioDocument.Robot.Goal, true, Request.Seed, Scenario.Params, Scenario.Semantic.Robot.Goal);

	const double MinClearWidthMeters = ScenarioSamplerResolveNumber(
		ScenarioDocument.Obstacles.MinClearWidthMeters,
		0.0,
		TEXT("obstacles.min_clear_width_m"),
		Request.Seed,
		Scenario.Params);
	ScenarioSamplerResolveFixedObstacles(
		ScenarioDocument,
		ResolvedLanes,
		Request.Seed,
		MinClearWidthMeters,
		Scenario.Params,
		Result.Diagnostics,
		Scenario.Semantic.StaticObstacles);
	ScenarioSamplerBuildClearWidthProfile(
		Scenario.Semantic.Layout,
		Scenario.Semantic.StaticObstacles,
		Scenario.Semantic.ClearWidthProfile,
		Scenario.Semantic.Summary);

	Document.Validation.bEditedByUser = false;
	Document.Validation.Diagnostics = Result.Diagnostics;

	TArray<FScenarioSchemaDiagnostic> SampleDiagnostics;
	const bool bValidSample = FScenarioSampleJson::ValidateDocument(Document, SampleDiagnostics);
	Result.Diagnostics.Append(SampleDiagnostics);
	Document.Validation.Diagnostics = Result.Diagnostics;
	Result.Document = Document;
	Result.bSuccess = bValidSample && !ScenarioSamplerHasErrors(Result.Diagnostics);
	return Result;
}
