// Fill out your copyright notice in the Description page of Project Settings.


#include "DeliveryBot/Component/DeliveryBot_MotionController.h"


// Sets default values for this component's properties
UDeliveryBot_MotionController::UDeliveryBot_MotionController()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UDeliveryBot_MotionController::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UDeliveryBot_MotionController::TickComponent(float DeltaTime, ELevelTick TickType,
                                                  FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

