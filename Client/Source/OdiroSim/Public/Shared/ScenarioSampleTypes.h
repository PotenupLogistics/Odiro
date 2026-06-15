#pragma once

#include "CoreMinimal.h"
#include "Shared/ScenarioSchemaTypes.h"
#include "Shared/ScenarioTemplateTypes.h"
#include "ScenarioSampleTypes.generated.h"

// Lane traversability type stored in scenario_sample semantic layout.
UENUM(BlueprintType)
enum class EScenarioSampleLaneType : uint8
{
	Walkable,
	Penalty,
	Blocked
};

// Static obstacle semantic class used by analysis and runtime conversion.
UENUM(BlueprintType)
enum class EScenarioSampleObstacleClass : uint8
{
	Blocking,
	TraversableCost
};

// Pedestrian role in a generated scenario sample.
UENUM(BlueprintType)
enum class EScenarioSamplePedestrianRole : uint8
{
	Encounter,
	Background
};

// Source files and generator inputs that produced one scenario sample.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioSampleSource
{
	GENERATED_BODY()

	// Source scenario template path.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FString TemplateRef;

	// Canonical hash of the source scenario template.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FString TemplateHash;

	// Experiment-local profile path used during sampling.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FString ProfileRef;

	// Canonical hash of the experiment-local profile.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FString ProfileHash;

	// Experiment setting path used during sampling.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FString SettingRef;

	// Canonical hash of the experiment setting.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FString SettingHash;

	// Concrete seed used to resolve this sample.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	int64 Seed = 0;

	// Generator version that produced this sample.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FString GeneratorVersion;
};

// Scenario sample identity and source lineage.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioSampleIdentity
{
	GENERATED_BODY()

	// Experiment-local sample id, usually matching the filename.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FString SampleId;

	// Scenario id used for display and result joins.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FString ScenarioId;

	// Source lineage for reproducibility.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FScenarioSampleSource Source;
};

// Resolved corridor axis stored in scenario.semantic.route_axis.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioSampleRouteAxis
{
	GENERATED_BODY()

	// Axis representation; v1 supports polyline only.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	EScenarioCorridorAxisType Type = EScenarioCorridorAxisType::Polyline;

	// World or template transform origin in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FVector2D OriginXYMeters = FVector2D::ZeroVector;

	// Heading of the local corridor axis frame in degrees.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	double HeadingDegrees = 0.0;

	// Concrete axis polyline points in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	TArray<FVector2D> PointsMeters;

	// Total axis length in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	double LengthMeters = 0.0;
};

// Resolved robot pose stored in scenario.semantic.robot.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioSampleRobotPose
{
	GENERATED_BODY()

	// Segment id containing the pose.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FString SegmentId;

	// Distance along the corridor axis in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	double AlongMeters = 0.0;

	// Lateral offset from the corridor axis in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	double OffsetMeters = 0.0;

	// Lane hint used when the pose was resolved.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FString LaneId;

	// Runtime yaw derived from the corridor pose in degrees.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	double HeadingDegrees = 0.0;

	// Original template anchor type used to resolve this pose.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	EScenarioTemplateRobotAnchorType SourceAnchorType = EScenarioTemplateRobotAnchorType::Entry;
};

// Resolved robot start and goal poses.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioSampleRobotSemantic
{
	GENERATED_BODY()

	// Concrete robot start pose.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FScenarioSampleRobotPose Start;

	// Concrete robot goal pose.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FScenarioSampleRobotPose Goal;
};

// One resolved lane in a semantic layout segment.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioSampleLayoutLane
{
	GENERATED_BODY()

	// Lane id such as walkway, building_edge, center, or curb_edge.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FString LaneId;

	// Lateral offset interval occupied by this lane in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FScenarioOffsetRangeMeters OffsetRangeMeters;

	// Surface catalog id assigned to this lane.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FString SurfaceId;

	// Traversability class derived from the surface catalog.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	EScenarioSampleLaneType Type = EScenarioSampleLaneType::Walkable;
};

// Layout rule that applies for one distance interval along the route axis.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioSampleLayoutEntry
{
	GENERATED_BODY()

	// Distance interval where this layout applies.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FScenarioAlongRangeMeters AlongRangeMeters;

	// Template segment id represented by this layout interval.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FString SegmentId;

	// Concrete lanes available in this interval.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	TArray<FScenarioSampleLayoutLane> Lanes;
};

// Behavior vector produced from persona defaults and template overrides.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioSamplePedestrianBehavior
{
	GENERATED_BODY()

	// Willingness to yield to the robot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	double Cooperation = 0.5;

	// Willingness to sidestep or avoid the robot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	double Evasiveness = 0.5;

	// Desired personal space in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	double PersonalSpaceMeters = 0.8;

	// Prediction horizon in seconds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	double AwarenessHorizonSeconds = 2.0;

	// Maximum yield waiting time in seconds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	double MaxYieldWaitSeconds = 2.0;

	// Sidestep distance in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	double SidestepDistanceMeters = 0.5;
};

// Baseline pedestrian route summary before robot interaction.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioSamplePedestrianBaseline
{
	GENERATED_BODY()

	// Start segment id for the baseline route.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FString StartSegmentId;

	// Goal segment id for the baseline route.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FString GoalSegmentId;

	// Start distance along the corridor axis in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	double StartAlongMeters = 0.0;

	// Start lateral offset from the corridor axis in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	double StartOffsetMeters = 0.0;

	// Goal distance along the corridor axis in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	double GoalAlongMeters = 0.0;

	// Goal lateral offset from the corridor axis in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	double GoalOffsetMeters = 0.0;

	// Optional baseline polyline points in meters for preview.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	TArray<FVector2D> PointsMeters;
};

// Concrete static obstacle instance in scenario.semantic.static_obstacles.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioSampleStaticObstacle
{
	GENERATED_BODY()

	// Generated obstacle instance id.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FString ObstacleId;

	// Prop catalog id used for this obstacle.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FString PropId;

	// Perception or log tag used for joins.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FString PerceptionTag;

	// Semantic obstacle class.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	EScenarioSampleObstacleClass ObstacleClass = EScenarioSampleObstacleClass::Blocking;

	// Sensor profile annotation such as solid, thin, or low_profile.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FString SensorProfile;

	// Distance along the corridor axis in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	double AlongMeters = 0.0;

	// Lateral offset from the corridor axis in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	double OffsetMeters = 0.0;

	// Runtime yaw in degrees.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	double YawDegrees = 0.0;

	// Analysis footprint size in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FVector2D FootprintMeters = FVector2D::ZeroVector;

	// Template placement id that generated this obstacle.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FString PlacedBy;

	// Remaining clear width at this obstacle in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	double ClearWidthRemainingMeters = 0.0;
};

// Concrete pedestrian instance in scenario.semantic.pedestrians.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioSamplePedestrian
{
	GENERATED_BODY()

	// Generated pedestrian instance id.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FString PedestrianId;

	// Role of this pedestrian in the sample.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	EScenarioSamplePedestrianRole Role = EScenarioSamplePedestrianRole::Background;

	// Template encounter id that generated this pedestrian.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FString PlacedBy;

	// Encounter type copied from the template when Role is Encounter.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	EScenarioTemplateEncounterType EncounterType = EScenarioTemplateEncounterType::OncomingPass;

	// Persona catalog id copied from the template.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FString PersonaId;

	// Concrete behavior vector after persona expansion and overrides.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FScenarioSamplePedestrianBehavior Behavior;

	// Concrete walking speed in meters per second.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	double SpeedMetersPerSecond = 1.2;

	// Baseline route summary before robot interaction.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FScenarioSamplePedestrianBaseline Baseline;

	// Plan and behavior fingerprint used for reproducibility.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FString PedestrianScenarioHash;
};

// Clear width summary for one distance interval.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioSampleClearWidthEntry
{
	GENERATED_BODY()

	// Distance interval covered by the clear width value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FScenarioAlongRangeMeters AlongRangeMeters;

	// Effective clear width in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	double ClearWidthMeters = 0.0;

	// Obstacle or region id limiting this width.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FString LimitedBy;
};

// Compact shape summary for LLM and analysis use.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioSampleSummary
{
	GENERATED_BODY()

	// Minimum effective clear width across the sample in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	double GlobalMinClearWidthMeters = 0.0;

	// Axis distance where the minimum clear width occurs.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	double MinClearAtAlongMeters = 0.0;

	// Total route axis length in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	double TotalLengthMeters = 0.0;

	// True when the main encounter overlaps the minimum-clear-width zone.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	bool bEncounterInMinClearZone = false;
};

// Human-readable semantic scenario view produced by the sampler.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioSampleSemantic
{
	GENERATED_BODY()

	// Concrete route axis used for along/offset joins.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FScenarioSampleRouteAxis RouteAxis;

	// Concrete robot route poses.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FScenarioSampleRobotSemantic Robot;

	// Segment and lane layout entries.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	TArray<FScenarioSampleLayoutEntry> Layout;

	// Concrete static obstacle instances.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	TArray<FScenarioSampleStaticObstacle> StaticObstacles;

	// Concrete pedestrian instances.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	TArray<FScenarioSamplePedestrian> Pedestrians;

	// Effective clear width profile.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	TArray<FScenarioSampleClearWidthEntry> ClearWidthProfile;

	// Compact semantic summary.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FScenarioSampleSummary Summary;
};

// Resolved scenario content inside a scenario_sample document.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioSampleScenario
{
	GENERATED_BODY()

	// Seed-resolved values for template range or choice fields.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	TMap<FString, FScenarioSampleParamValue> Params;

	// Semantic view used by LLM, analysis, preview, and runtime conversion.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FScenarioSampleSemantic Semantic;
};

// Validation block stored with a generated scenario sample.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioSampleValidation
{
	GENERATED_BODY()

	// True when a generated sample was manually edited after sampling.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	bool bEditedByUser = false;

	// Generator warnings, repairs, and errors associated with this sample.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	TArray<FScenarioSchemaDiagnostic> Diagnostics;
};

// Canonical generated sample consumed by preview and run workflows.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioSampleDocument
{
	GENERATED_BODY()

	// JSON schema name; v1 stores scenario_sample.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FString Schema = TEXT("scenario_sample");

	// Scenario sample schema version.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	int32 Version = 1;

	// Sample identity and source lineage.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FScenarioSampleIdentity Sample;

	// Resolved scenario body.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FScenarioSampleScenario Scenario;

	// Validation and edit-state metadata.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FScenarioSampleValidation Validation;
};
