#pragma once

#include "CoreMinimal.h"
#include "ScenarioEditorTypes.generated.h"

class UTexture2D;

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
