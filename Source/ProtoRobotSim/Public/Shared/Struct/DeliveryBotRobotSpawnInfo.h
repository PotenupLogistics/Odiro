#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodeCoreTypes.h"
#include "Shared/Struct/DeliveryBotSetupInfo.h"
#include "DeliveryBotRobotSpawnInfo.generated.h"

USTRUCT(BlueprintType)
struct FDeliveryBotPolicySetupInfo
{
	GENERATED_BODY()
	
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> SituationPolicyTags{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> ActionPolicyTags{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bPedestrianPriority{ true };
};

USTRUCT(BlueprintType)
struct FDeliveryBotRobotSpawnInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString InstanceId{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString AssetId{ TEXT("delivery_bot") };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EEpisodeMobilityMode MobilityMode{ EEpisodeMobilityMode::Moving };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FTransform SpawnTransformCm{ FTransform::Identity };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDeliveryBotSetupInfo SetupInfo{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDeliveryBotPolicySetupInfo PolicySetupInfo{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FString, FEpisodeParamValue> ExtraProperties{};
};
