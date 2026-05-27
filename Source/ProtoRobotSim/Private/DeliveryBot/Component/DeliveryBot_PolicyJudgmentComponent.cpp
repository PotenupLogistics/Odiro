// Fill out your copyright notice in the Description page of Project Settings.


#include "DeliveryBot/Component/DeliveryBot_PolicyJudgmentComponent.h"


UDeliveryBot_PolicyJudgmentComponent::UDeliveryBot_PolicyJudgmentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

}


void UDeliveryBot_PolicyJudgmentComponent::BeginPlay()
{
	Super::BeginPlay();

	
}


void UDeliveryBot_PolicyJudgmentComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                         FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

}

