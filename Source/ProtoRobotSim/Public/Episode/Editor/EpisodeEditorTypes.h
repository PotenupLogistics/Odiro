#pragma once

#include "CoreMinimal.h"
#include "EpisodeEditorTypes.generated.h"

UENUM(BlueprintType)
enum class EEpisodeEditorControllerMode : uint8
{
	Observer,
	EditPlacement
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
};
