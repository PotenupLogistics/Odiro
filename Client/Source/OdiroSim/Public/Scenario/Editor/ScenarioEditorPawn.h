#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ScenarioEditorPawn.generated.h"

class UCameraComponent;
class UFloatingPawnMovement;
class UPawnMovementComponent;
class USceneComponent;

UCLASS(BlueprintType)
class ODIROSIM_API AScenarioEditorPawn : public APawn
{
	GENERATED_BODY()

public:
	AScenarioEditorPawn();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor")
	TObjectPtr<UCameraComponent> CameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor")
	TObjectPtr<UFloatingPawnMovement> FloatingMovementComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Movement", meta = (ClampMin = "1.0"))
	float MaxMoveSpeed = 2400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Movement", meta = (ClampMin = "1.0"))
	float Acceleration = 8000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Movement", meta = (ClampMin = "1.0"))
	float Deceleration = 8000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Movement")
	float MinPitchDegrees = -89.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Movement")
	float MaxPitchDegrees = 89.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|TopDown", meta = (ClampMin = "100.0"))
	double TopDownCameraHeightCm = 10000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|TopDown", meta = (ClampMin = "100.0"))
	double TopDownOrthoWidthCm = 5000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|TopDown", meta = (ClampMin = "1.0"))
	double TopDownOrthoWidthMinCm = 500.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|TopDown", meta = (ClampMin = "1.0"))
	double TopDownOrthoWidthMaxCm = 50000.0;

	// zoom 입력 1 단위당 ortho width 증감 비율.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|TopDown", meta = (ClampMin = "0.001"))
	double TopDownZoomStepRatio = 0.1;

	// drag pan 입력 1 단위당 이동량을 현재 ortho width 대비 비율로 결정함.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|TopDown", meta = (ClampMin = "0.0"))
	double TopDownDragPanSensitivity = 0.005;

	virtual UPawnMovementComponent* GetMovementComponent() const override;
	virtual void PostInitializeComponents() override;

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor")
	void ApplyMoveInput(float forwardValue, float rightValue, float upValue);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor")
	void ApplyLookInput(float yawDeltaDegrees, float pitchDeltaDegrees);

	// perspective pose를 저장한 뒤 현재 XY 위에서 수직 하향 orthographic 카메라로 전환함.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor")
	void EnterTopDownView();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor")
	void EnterPerspectiveView();

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor")
	bool IsTopDownViewActive() const { return bTopDownViewActive; }

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor")
	void ApplyTopDownPanInput(float forwardValue, float rightValue);

	// grab-pan: 드래그 방향으로 월드가 따라오도록 카메라를 반대로 이동시킴.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor")
	void ApplyTopDownDragPanInput(float rightValue, float upValue);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor")
	void ApplyTopDownZoomInput(float zoomValue);

private:
	void ApplyMovementSettings();
	void ApplyTopDownPanSpeed();

	UPROPERTY(Transient)
	bool bTopDownViewActive = false;

	UPROPERTY(Transient)
	FTransform SavedPerspectiveTransform = FTransform::Identity;

	UPROPERTY(Transient)
	double CurrentOrthoWidthCm = 0.0;
};
