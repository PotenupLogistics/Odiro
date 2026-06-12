#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeliveryBot_PolicyJudgmentComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROTOROBOTSIM_API UDeliveryBot_PolicyJudgmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeliveryBot_PolicyJudgmentComponent();
};
