// Fill out your copyright notice in the Description page of Project Settings.


#include "DeliveryBot/Actor/DeliveryBot_GridBoundsActor.h"

#include "Components/BoxComponent.h"
#include "DeliveryBot/Subsystem/DeliveryBot_GridSubsystem.h"


ADeliveryBot_GridBoundsActor::ADeliveryBot_GridBoundsActor()
{
	PrimaryActorTick.bCanEverTick = false;
	
	BoundsBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundsBox"));
	SetRootComponent(BoundsBox);
	BoundsBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
}

void ADeliveryBot_GridBoundsActor::BeginPlay()
{
	Super::BeginPlay();
	UWorld* world = GetWorld();
	if (!IsValid(world))
		return;

	UDeliveryBot_GridSubsystem* gridSubsystem = world->GetSubsystem<UDeliveryBot_GridSubsystem>();
	if (!IsValid(gridSubsystem))
		return;

	gridSubsystem->BuildGridFromBounds(this);
}

void ADeliveryBot_GridBoundsActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
}

 