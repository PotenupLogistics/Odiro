#include "DeliveryBot/Actor/DeliveryBot_ChaosActor.h"

#include "ChaosVehicleMovementComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "DeliveryBot/Component/DeliveryBot_ChaosDriveComponent.h"

ADeliveryBot_ChaosActor::ADeliveryBot_ChaosActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	ChaosDriveComponent = CreateDefaultSubobject<UDeliveryBot_ChaosDriveComponent>(TEXT("ChaosDriveComponent"));

	UChaosWheeledVehicleMovementComponent* wheeledMovement{
		Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent())
	};

	if (IsValid(ChaosDriveComponent))
	{
		ChaosDriveComponent->SetupVehicleMovement(wheeledMovement);
	}
}

void ADeliveryBot_ChaosActor::BeginPlay()
{
	Super::BeginPlay();

	UChaosVehicleMovementComponent* vehicleMovement{ GetVehicleMovementComponent() };

	if (!IsValid(vehicleMovement))
	{
		return;
	}

	vehicleMovement->SetRequiresControllerForInputs(false);
	vehicleMovement->SetUseAutomaticGears(true);
	vehicleMovement->SetTargetGear(1, true);
}

void ADeliveryBot_ChaosActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bUseTempAutoDrive)
	{
		return;
	}

	ApplyTempDriveInput();
}

void ADeliveryBot_ChaosActor::SetTempDriveInput(float throttle, float steering, float brake, bool bHandbrake)
{
	TempThrottle = FMath::Clamp(throttle, 0.f, 1.f);
	TempSteering = FMath::Clamp(steering, -1.f, 1.f);
	TempBrake = FMath::Clamp(brake, 0.f, 1.f);
	bTempHandbrake = bHandbrake;
}

void ADeliveryBot_ChaosActor::ApplyTempDriveInput() const
{
	if (!IsValid(ChaosDriveComponent))
	{
		return;
	}

	UChaosVehicleMovementComponent* vehicleMovement{ GetVehicleMovementComponent() };

	if (!IsValid(vehicleMovement))
	{
		return;
	}

	ChaosDriveComponent->ApplyDriveInput(
		vehicleMovement,
		TempThrottle,
		TempSteering,
		TempBrake,
		bTempHandbrake
	);
}