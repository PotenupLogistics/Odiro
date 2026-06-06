#pragma once

#include "CoreMinimal.h"
#include "EpisodeEditorTypes.generated.h"

UENUM(BlueprintType)
enum class EEpisodeEditorControllerMode : uint8
{
	Observer,
	EditPlacement
};

UENUM(BlueprintType)
enum class EEpisodeTransformGizmoHandle : uint8
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
enum class EEpisodeTransformGizmoMode : uint8
{
	Translate,
	Rotate,
	Scale
};

USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeAuthoringStaticObstacleRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode|Editor")
	FString InstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode|Editor")
	FName PropId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode|Editor")
	FTransform Transform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode|Editor", meta = (ClampMin = "0.0"))
	double PlacementRadius2D = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode|Editor")
	FVector2D PlacementHalfExtent2D = FVector2D::ZeroVector;
};
