#include "Scenario/ScenarioSampleWorldSpecAdapter.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/Crc.h"
#include "Shared/ScenarioSampleJson.h"

namespace
{
	const double MetersToCentimeters = 100.0;

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
		const bool bResolvedStart = ResolveSampleAxisPose(
			Semantic.RouteAxis,
			Semantic.Robot.Start.AlongMeters,
			Semantic.Robot.Start.OffsetMeters,
			StartPose);
		const bool bResolvedGoal = ResolveSampleAxisPose(
			Semantic.RouteAxis,
			Semantic.Robot.Goal.AlongMeters,
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

	FVector TransformSamplePointMetersToCm(const FScenarioSampleRouteAxis& Axis, const FVector2D& PointMeters)
	{
		const FVector2D WorldPointMeters = Axis.OriginXYMeters + RotateSamplePoint(PointMeters, Axis.HeadingDegrees);
		return FVector(WorldPointMeters.X * MetersToCentimeters, WorldPointMeters.Y * MetersToCentimeters, 0.0);
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
					PathSpec.Points.Add(TransformSamplePointMetersToCm(Semantic.RouteAxis, PointMeters));
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
	AddRobotFromSample(Document.Scenario.Semantic, Result, WorldSpec);
	AddStaticObstaclesFromSample(Document.Scenario.Semantic, Result, WorldSpec);
	AddPedestriansFromSample(Document.Scenario.Semantic, Result, WorldSpec);
	WorldSpec.SpecHash = MakeAdapterSpecHash(Document);

	Result.WorldSpec = WorldSpec;
	Result.bSuccess = !HasErrorDiagnostics(Result.Diagnostics);
	return Result;
}
