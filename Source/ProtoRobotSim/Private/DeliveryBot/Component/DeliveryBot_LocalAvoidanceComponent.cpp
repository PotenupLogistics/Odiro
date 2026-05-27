// Fill out your copyright notice in the Description page of Project Settings.


#include "DeliveryBot/Component/DeliveryBot_LocalAvoidanceComponent.h"


UDeliveryBot_LocalAvoidanceComponent::UDeliveryBot_LocalAvoidanceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

}


void UDeliveryBot_LocalAvoidanceComponent::BeginPlay()
{
	Super::BeginPlay();

	
}


void UDeliveryBot_LocalAvoidanceComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                         FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

}

