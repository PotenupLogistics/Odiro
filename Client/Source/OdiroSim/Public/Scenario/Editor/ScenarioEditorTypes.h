#pragma once

#include "CoreMinimal.h"
#include "Shared/ScenarioCoreTypes.h"
#include "ScenarioEditorTypes.generated.h"

class UTexture2D;

// Scenario template sections exposed by the editor inspector.
UENUM(BlueprintType)
enum class EScenarioTemplateSidebarPanel : uint8
{
	Main,
	Corridor,
	Obstacle,
	Pedestrian
};

// Value editor shape used by one Scenario Template field row.
UENUM(BlueprintType)
enum class EScenarioEditorSidebarFieldInputType : uint8
{
	Text,
	MultilineText,
	Integer,
	Number,
	EnumText,
	ComboBox,
	Range,
	StringList,
	ObjectArray
};

// Editable field ids exposed by one root.obstacles.placements[] editor row.
UENUM(BlueprintType)
enum class EScenarioEditorSidebarObstaclePlacementField : uint8
{
	PlacementId,
	Kind,
	Prop,
	Pattern,
	Segment,
	Lane,
	Along,
	Offset,
	ZoneSegments,
	ZoneLanes,
	PaletteCategories,
	PaletteClasses,
	Count,
	Spacing,
	GapWidth,
	Density,
	Yaw,
	AllowBlocking
};

// Robot anchor target edited by the Scenario Template main panel.
UENUM(BlueprintType)
enum class EScenarioEditorSidebarRobotAnchorTarget : uint8
{
	Start,
	Goal
};

// Robot anchor field edited by the Scenario Template main panel.
UENUM(BlueprintType)
enum class EScenarioEditorSidebarRobotAnchorField : uint8
{
	Type,
	Segment,
	Along,
	Offset,
	Lane,
	Heading
};

// Right-sidebar inspector page owned by the Scenario Editor root widget.
UENUM(BlueprintType)
enum class EScenarioEditorInspectorTab : uint8
{
	Detail,
	Llm
};

// Outliner row semantic action used by the root widget when a row is selected.
UENUM(BlueprintType)
enum class EScenarioEditorOutlinerItemType : uint8
{
	TemplatePanel,
	Placeable
};

UENUM(BlueprintType)
enum class EScenarioEditorControllerMode : uint8
{
	Observer,
	EditPlacement,
	EditRegionDraw
};

// 카메라 투영 모드. EditorMode(배치 상태)와 직교하는 별도 상태임.
UENUM(BlueprintType)
enum class EScenarioEditorViewMode : uint8
{
	Perspective,
	TopDownOrtho
};

// Corridor 단면 lane이 붙는 축의 좌우 측면을 구분함.
UENUM(BlueprintType)
enum class EScenarioEditorCorridorSide : uint8
{
	Building,
	Curb
};

// Corridor authoring handle shape; handles edit the template axis without changing the schema.
UENUM(BlueprintType)
enum class EScenarioCorridorHandleType : uint8
{
	Vertex,
	Segment
};

UENUM(BlueprintType)
enum class EScenarioTransformGizmoHandle : uint8
{
	None,
	TranslateX,
	TranslateY,
	TranslateZ,
	TranslateXY,
	TranslateXZ,
	TranslateYZ,
	RotateX,
	RotateY,
	RotateZ,
	ScaleX,
	ScaleY,
	ScaleZ,
	ScaleXY,
	ScaleXZ,
	ScaleYZ,
	ScaleUniform
};

UENUM(BlueprintType)
enum class EScenarioTransformGizmoMode : uint8
{
	Translate,
	Rotate,
	Scale
};

UENUM(BlueprintType)
enum class EScenarioTransformGizmoOrientationMode : uint8
{
	World,
	Relative
};

UENUM(BlueprintType)
enum class EScenarioPaletteItemType : uint8
{
	StaticObstacle,
	Pedestrian,
	RobotStart,
	RobotGoal,
	GroundRegion
};

// Screen-space overlay marker kind used for robot route endpoints.
UENUM(BlueprintType)
enum class EScenarioEditorRouteMarkerKind : uint8
{
	Start,
	Goal
};

USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioPaletteItemEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor")
	EScenarioPaletteItemType ItemType = EScenarioPaletteItemType::StaticObstacle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor")
	FName AssetId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor")
	FText CategoryText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor")
	FString IconName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor")
	TSoftObjectPtr<UTexture2D> ThumbnailTexture;
};

// Projected marker data consumed by editor and simulation viewport overlay paint paths.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioEditorRouteMarkerOverlayItem
{
	GENERATED_BODY()

	// Stable selectable instance id owned by the route marker proxy actor.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor")
	FString InstanceId;

	// Semantic endpoint represented by this overlay marker.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor")
	EScenarioEditorRouteMarkerKind Kind = EScenarioEditorRouteMarkerKind::Start;

	// World-space endpoint location that should be projected into the viewport.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor")
	FVector WorldLocation = FVector::ZeroVector;

	// Whether the proxy actor is currently hovered by editor input.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor")
	bool bHovered = false;

	// Whether the proxy actor is currently selected by editor input.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor")
	bool bSelected = false;
};

USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioAuthoringStaticObstacleRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor")
	FString InstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor")
	FName PropId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor")
	FTransform Transform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor", meta = (ClampMin = "0.0"))
	double PlacementRadius2D = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor")
	FVector2D PlacementHalfExtent2D = FVector2D::ZeroVector;
};

USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioOutlinerItemViewModel
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Outliner")
	FString ItemKey;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Outliner")
	FString ParentKey;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Outliner")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Outliner")
	FText Subtitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Outliner")
	int32 Depth = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Outliner")
	bool bExpandable = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Outliner")
	bool bExpanded = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Outliner")
	bool bSelected = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Outliner")
	EScenarioEditorOutlinerItemType ItemType = EScenarioEditorOutlinerItemType::TemplatePanel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Outliner")
	EScenarioTemplateSidebarPanel TemplatePanel = EScenarioTemplateSidebarPanel::Main;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Outliner")
	FString InstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Outliner")
	EScenarioActorCategory ActorCategory = EScenarioActorCategory::StaticObstacle;
};
