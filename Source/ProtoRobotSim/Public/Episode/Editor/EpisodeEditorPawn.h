#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "EpisodeEditorPawn.generated.h"

class UCameraComponent;
class UFloatingPawnMovement;
class UPawnMovementComponent;
class USceneComponent;

UCLASS(BlueprintType)
class PROTOROBOTSIM_API AEpisodeEditorPawn : public APawn
{
	GENERATED_BODY()

public:
	AEpisodeEditorPawn();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode|Editor")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode|Editor")
	TObjectPtr<UCameraComponent> CameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode|Editor")
	TObjectPtr<UFloatingPawnMovement> FloatingMovementComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Movement", meta = (ClampMin = "1.0"))
	float MaxMoveSpeed = 2400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Movement", meta = (ClampMin = "1.0"))
	float Acceleration = 8000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Movement", meta = (ClampMin = "1.0"))
	float Deceleration = 8000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Movement")
	float MinPitchDegrees = -89.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Movement")
	float MaxPitchDegrees = 89.0f;

	virtual UPawnMovementComponent* GetMovementComponent() const override;
	virtual void PostInitializeComponents() override;

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor")
	void ApplyMoveInput(float forwardValue, float rightValue, float upValue);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor")
	void ApplyLookInput(float yawDeltaDegrees, float pitchDeltaDegrees);

private:
	void ApplyMovementSettings();
};
