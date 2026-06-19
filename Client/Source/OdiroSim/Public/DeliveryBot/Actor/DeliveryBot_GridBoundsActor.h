
#pragma once

#include "CoreMinimal.h"
#include "Shared/Struct/DeliveryBot/Navigation/DeliveryBotGridInfo.h"
#include "DeliveryBot_GridBoundsActor.generated.h"

class UBoxComponent;

UCLASS()
class ODIROSIM_API ADeliveryBot_GridBoundsActor : public AActor
{
	GENERATED_BODY()

public:
	ADeliveryBot_GridBoundsActor();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	

public:
	// Controls legacy level-placed grid actors; scenario runtime builds the grid after surfaces are spawned.
	void SetBuildGridOnBeginPlay(bool bEnabled);

	UBoxComponent* GetBoundsBox() const
	{
		return BoundsBox;
	}
	
	float GetCellSize() const
	{
		return CellSize;
	}
	float GetMaxWalkableSlopeDegree() const
	{
		return MaxWalkableSlopeDegree;
	}
	
	FVector GetRobotBoxExtent() const
	{
		return RobotBoxExtent;
	}
	
	const TArray<FDeliveryBotGridCollisionRuleInfo>& GetCollisionProfileRules() const
	{
		return CollisionProfileRules;
	}
	
	ECollisionChannel GetGridTraceChannel() const
	{
		return ECC_GameTraceChannel8;
	}

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Grid", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UBoxComponent> BoundsBox;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Grid", meta = (AllowPrivateAccess = "true"))
	float CellSize{ 50.f };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Grid", meta = (AllowPrivateAccess = "true"))
	FVector RobotBoxExtent{ 30.f, 45.f, 25.f };
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Grid", meta = (AllowPrivateAccess = "true"))
	float MaxWalkableSlopeDegree{ 60.f };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Grid", meta = (AllowPrivateAccess = "true"))
	TArray<FDeliveryBotGridCollisionRuleInfo> CollisionProfileRules;

	// Whether this actor owns grid construction from BeginPlay instead of an external scenario subsystem.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Grid", meta = (AllowPrivateAccess = "true"))
	bool bBuildGridOnBeginPlay{ true };
};
