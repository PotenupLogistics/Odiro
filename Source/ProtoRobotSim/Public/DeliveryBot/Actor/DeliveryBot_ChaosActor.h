#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "DeliveryBot_ChaosActor.generated.h"

class UDeliveryBot_ChaosDriveComponent;

UCLASS(Blueprintable)
class PROTOROBOTSIM_API ADeliveryBot_ChaosActor : public AWheeledVehiclePawn
{
	GENERATED_BODY()

public:
	ADeliveryBot_ChaosActor(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Temp Drive")
	void SetTempDriveInput(float throttle, float steering, float brake, bool bHandbrake);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Component")
	TObjectPtr<UDeliveryBot_ChaosDriveComponent> ChaosDriveComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Temp Drive")
	bool bUseTempAutoDrive{ true };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Temp Drive", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TempThrottle{ 1.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Temp Drive", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float TempSteering{ 0.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Temp Drive", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TempBrake{ 0.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Temp Drive")
	bool bTempHandbrake{ false };

private:
	void ApplyTempDriveInput() const;
};