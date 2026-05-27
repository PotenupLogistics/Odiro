#pragma once
#include "CoreMinimal.h"
#include "RePlayEventInfo.generated.h"

USTRUCT(BlueprintType)
struct FRePlayEventInfo
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 EventIndex{0};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double EventRecordingTime{0.0};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ActorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FTransform BotTransform{FTransform::Identity};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Velocity{FVector::ZeroVector}; // 매 프레임마다 달라지는 속도가 필요한가..?

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString TargetActorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector TargetLocation{FVector::ZeroVector};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PathIndex{INDEX_NONE};

	
};
