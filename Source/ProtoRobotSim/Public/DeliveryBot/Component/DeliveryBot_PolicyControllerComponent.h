#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeliveryBot_PolicyControllerComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROTOROBOTSIM_API UDeliveryBot_PolicyControllerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeliveryBot_PolicyControllerComponent();
};
