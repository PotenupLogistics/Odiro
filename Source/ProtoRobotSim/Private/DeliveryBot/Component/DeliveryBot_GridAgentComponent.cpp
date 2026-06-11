
#include "DeliveryBot/Component/DeliveryBot_GridAgentComponent.h"


UDeliveryBot_GridAgentComponent::UDeliveryBot_GridAgentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

}


void UDeliveryBot_GridAgentComponent::BeginPlay()
{
	Super::BeginPlay();

	
}


void UDeliveryBot_GridAgentComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                    FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

 }

