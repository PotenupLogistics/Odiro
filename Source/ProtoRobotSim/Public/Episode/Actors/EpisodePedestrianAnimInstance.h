#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "EpisodePedestrianAnimInstance.generated.h"

class AEpisodePedestrian;

UCLASS()
class PROTOROBOTSIM_API UEpisodePedestrianAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UPROPERTY(BlueprintReadOnly, Category="Episode|Visual")
	float VisualSpeedCmPerSecond = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Episode|Visual")
	float VisualDirectionDegrees = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Episode|Visual")
	bool bMoving = false;

private:
	UPROPERTY(Transient)
	TObjectPtr<AEpisodePedestrian> CachedPedestrian;
};
