// Fill out your copyright notice in the Description page of Project Settings.


#include "DeliveryBot/Actor/DeliveryBot_SimpleMesh.h"


ADeliveryBot_SimpleMesh::ADeliveryBot_SimpleMesh()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ADeliveryBot_SimpleMesh::BeginPlay()
{
	Super::BeginPlay();
	
}

void ADeliveryBot_SimpleMesh::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

