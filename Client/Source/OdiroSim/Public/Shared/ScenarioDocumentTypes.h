#pragma once

#include "CoreMinimal.h"
#include "Shared/ScenarioSchemaTypes.h"
#include "ScenarioDocumentTypes.generated.h"

// Corridor segment kinds supported by scenario_template v1.
UENUM(BlueprintType)
enum class EScenarioTemplateSegmentType : uint8
{
	Straight,
	Narrowing,
	Crosswalk,
	Entrance
};

// Static obstacle placement rule kind in authored scenario_template JSON.
UENUM(BlueprintType)
enum class EScenarioTemplateObstaclePlacementKind : uint8
{
	Fixed,
	Pattern,
	Scatter
};

// Designed pedestrian encounter type in authored scenario_template JSON.
UENUM(BlueprintType)
enum class EScenarioTemplateEncounterType : uint8
{
	OncomingPass,
	Overtake,
	CrossPath,
	StandingGroup
};

// Robot anchor type used by template authors instead of direct world coordinates.
UENUM(BlueprintType)
enum class EScenarioTemplateRobotAnchorType : uint8
{
	Entry,
	Exit,
	CorridorPose
};

// Robot heading hint used when a robot anchor resolves to a concrete pose.
UENUM(BlueprintType)
enum class EScenarioTemplateRobotHeading : uint8
{
	Forward,
	Backward,
	Auto
};

// Corridor-local position rule used by fixed and pattern obstacle placements.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioTemplateCorridorPlacement
{
	GENERATED_BODY()

	// Segment id that owns the placement anchor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FString SegmentId;

	// Distance along the corridor axis in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateNumberValue AlongMeters;

	// Lateral offset from the corridor axis in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateNumberValue OffsetMeters;

	// Optional lane hint such as walkway, building_edge, center, curb_edge, or across.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FString LaneId;
};

// Corridor axis authored by editor spline/polyline or LLM points.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioTemplateAxis
{
	GENERATED_BODY()

	// Axis representation; v1 supports polyline only.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	EScenarioCorridorAxisType Type = EScenarioCorridorAxisType::Polyline;

	// Template-local XY points in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	TArray<FVector2D> PointsMeters;
};

// Lane rule on either side of the main walkway.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioTemplateLaneRule
{
	GENERATED_BODY()

	// Surface catalog id such as sidewalk, grass, road, wall, or building.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FString SurfaceId;

	// Lane width in meters, fixed or sampled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateNumberValue WidthMeters;
};

// Semantic segment along the corridor axis.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioTemplateSegment
{
	GENERATED_BODY()

	// Template-local unique segment id.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FString SegmentId;

	// Semantic segment kind.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	EScenarioTemplateSegmentType Type = EScenarioTemplateSegmentType::Straight;

	// Distance range covered by the segment along the corridor axis.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioAlongRangeMeters AlongRangeMeters;

	// Optional surface replacement choice for this segment.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateStringValue ReplacedBySurfaceId;
};

// Spatial skeleton and lane/surface rules for a scenario template.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioTemplateCorridor
{
	GENERATED_BODY()

	// Main route axis used by along/offset placement.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateAxis Axis;

	// Main walkway width in meters, fixed or sampled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateNumberValue WalkwayWidthMeters;

	// Lanes on the building side of the walkway.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	TArray<FScenarioTemplateLaneRule> BuildingSide;

	// Lanes on the curb side of the walkway.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	TArray<FScenarioTemplateLaneRule> CurbSide;

	// Semantic corridor segments referenced by obstacles, pedestrians, and robot anchors.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	TArray<FScenarioTemplateSegment> Segments;
};

// Scatter placement zone described by segment and lane filters.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioTemplateObstacleZone
{
	GENERATED_BODY()

	// Segment ids where scatter placement may generate obstacles.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	TArray<FString> SegmentIds;

	// Lane ids where scatter placement may generate obstacles.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	TArray<FString> LaneIds;
};

// Prop filters used by scatter placement.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioTemplateObstaclePalette
{
	GENERATED_BODY()

	// Prop category ids allowed for generated obstacles.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	TArray<FString> CategoryIds;

	// Prop class ids allowed for generated obstacles.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	TArray<FString> ClassIds;
};

// One authored static obstacle placement rule.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioTemplateObstaclePlacement
{
	GENERATED_BODY()

	// Template-local unique placement id.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FString PlacementId;

	// Placement rule kind.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	EScenarioTemplateObstaclePlacementKind Kind = EScenarioTemplateObstaclePlacementKind::Fixed;

	// Prop catalog id for fixed or pattern placement.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FString PropId;

	// Pattern id such as gate, line, or cluster.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FString PatternId;

	// Corridor-local anchor used by fixed and pattern placement.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateCorridorPlacement At;

	// Scatter placement zone.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateObstacleZone Zone;

	// Scatter prop filter.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateObstaclePalette Palette;

	// Pattern obstacle count or generated count range.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateIntegerValue Count;

	// Pattern spacing in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateNumberValue SpacingMeters;

	// Pattern gap width in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateNumberValue GapWidthMeters;

	// Scatter density per 10 meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateNumberValue DensityPer10Meters;

	// Obstacle yaw in degrees, fixed or sampled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateNumberValue YawDegrees;

	// True only when this placement intentionally violates min clear width.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	bool bAllowBlocking = false;
};

// Static obstacle generation rules for a scenario template.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioTemplateObstacleRules
{
	GENERATED_BODY()

	// Minimum valid clear width that generation should preserve unless explicitly overridden.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateNumberValue MinClearWidthMeters;

	// Authored placement rules that generate sample static obstacles.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	TArray<FScenarioTemplateObstaclePlacement> Placements;
};

// Background pedestrian generation rule.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioTemplatePedestrianBackground
{
	GENERATED_BODY()

	// Number of background pedestrians.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateIntegerValue Count;

	// Background pedestrian walking speed in meters per second.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateNumberValue SpeedMetersPerSecond;

	// Optional spawn segment filter.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	TArray<FString> SpawnSegmentIds;
};

// Optional behavior override fields for an authored pedestrian encounter.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioTemplatePedestrianBehaviorOverrides
{
	GENERATED_BODY()

	// Willingness to yield to the robot, fixed or sampled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateNumberValue Cooperation;

	// Willingness to sidestep or avoid the robot, fixed or sampled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateNumberValue Evasiveness;

	// Desired personal space in meters, fixed or sampled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateNumberValue PersonalSpaceMeters;

	// Prediction horizon in seconds, fixed or sampled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateNumberValue AwarenessHorizonSeconds;

	// Maximum yield waiting time in seconds, fixed or sampled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateNumberValue MaxYieldWaitSeconds;

	// Sidestep distance in meters, fixed or sampled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateNumberValue SidestepDistanceMeters;
};

// One designed pedestrian encounter rule.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioTemplatePedestrianEncounter
{
	GENERATED_BODY()

	// Template-local unique encounter id.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FString EncounterId;

	// Encounter behavior pattern.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	EScenarioTemplateEncounterType Type = EScenarioTemplateEncounterType::OncomingPass;

	// Segment id where the encounter is intended to occur.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FString AtSegmentId;

	// Pedestrian persona catalog id.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FString PersonaId;

	// Offset from the intended meeting point in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateNumberValue MeetOffsetMeters;

	// Optional behavior overrides applied to the persona defaults.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplatePedestrianBehaviorOverrides Overrides;
};

// Pedestrian generation rules for a scenario template.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioTemplatePedestrianRules
{
	GENERATED_BODY()

	// Background pedestrian generation rule.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplatePedestrianBackground Background;

	// Designed encounter rules.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	TArray<FScenarioTemplatePedestrianEncounter> Encounters;
};

// Robot start or goal anchor in authored template space.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioTemplateRobotAnchor
{
	GENERATED_BODY()

	// Anchor interpretation used by the generator.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	EScenarioTemplateRobotAnchorType Type = EScenarioTemplateRobotAnchorType::Entry;

	// Segment id used when Type is CorridorPose.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FString SegmentId;

	// Distance along the corridor axis in meters when Type is CorridorPose.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateNumberValue AlongMeters;

	// Lateral offset from the corridor axis in meters when Type is CorridorPose.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateNumberValue OffsetMeters;

	// Optional lane hint for editor display and generation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FString LaneId;

	// Optional heading hint for generation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	EScenarioTemplateRobotHeading Heading = EScenarioTemplateRobotHeading::Auto;
};

// Robot route anchors owned by the scenario template.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioTemplateRobot
{
	GENERATED_BODY()

	// Robot start anchor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateRobotAnchor Start;

	// Robot goal anchor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateRobotAnchor Goal;
};

// Authoring source edited by users and LLMs before episode scenario sampling.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioDocument
{
	GENERATED_BODY()

	// JSON schema name; v1 stores project scenario documents.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Document")
	FString Schema = TEXT("scenario");

	// Scenario document schema version.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Document")
	int32 Version = 1;

	// Human-readable snake_case scenario identifier.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Document")
	FString ScenarioId;

	// Natural-language scenario intent or hypothesis.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Document")
	FString Intent;

	// Spatial skeleton and lane rules.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Document")
	FScenarioTemplateCorridor Corridor;

	// Static obstacle generation rules.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Document")
	FScenarioTemplateObstacleRules Obstacles;

	// Pedestrian generation rules.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Document")
	FScenarioTemplatePedestrianRules Pedestrians;

	// Robot start and goal anchors.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Document")
	FScenarioTemplateRobot Robot;
};
