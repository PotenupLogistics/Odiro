#pragma once

#include "CoreMinimal.h"
#include "RePlayEventInfo.h"
#include "RePlayInfo.generated.h"

USTRUCT(BlueprintType)
struct FRePlayInfo
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString RePlayId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString EpisodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double TotalPlayTimeSeconds{0.0};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TotalEventCount{0};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 LastEventIndex{INDEX_NONE};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FRePlayEventInfo> Events;
};
