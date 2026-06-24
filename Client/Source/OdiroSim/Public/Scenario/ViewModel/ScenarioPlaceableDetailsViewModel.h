#pragma once

#include "CoreMinimal.h"
#include "Platform/ViewModel/OdiroViewModelBase.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "ScenarioPlaceableDetailsViewModel.generated.h"

class UScenarioEditorUiSubsystem;
class UWidget;

// 선택된 Scenario placeable의 details panel 표시 상태.
UCLASS(BlueprintType)
class ODIROSIM_API UScenarioPlaceableDetailsViewModel : public UOdiroViewModelBase
{
	GENERATED_BODY()

public:
	// World subsystem 소유 관계를 연결한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|PlaceableDetails")
	void InitializeForSubsystem(UScenarioEditorUiSubsystem* uiSubsystem);

	// Details panel selection 표시 상태를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|PlaceableDetails")
	void SetSelectedPlaceable(const FString& instanceId, const FString& label);

	// Details panel selection 표시 상태를 비운다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|PlaceableDetails")
	void ClearSelectedPlaceable();

	// 선택된 placeable instance id를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|PlaceableDetails")
	FString GetSelectedInstanceId() const { return SelectedInstanceId; }

	// Details panel title 표시 문자열을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|PlaceableDetails")
	FString GetTitleText() const { return TitleText; }

	// Details panel selection 여부를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|PlaceableDetails")
	bool HasSelection() const { return bHasSelection; }

	// Details widget input mode를 요청한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|PlaceableDetails")
	void RequestEditorWidgetInputMode(UWidget* requestingWidget);

	// Details widget input mode 요청을 해제한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|PlaceableDetails")
	void ReleaseEditorWidgetInputMode(UWidget* requestingWidget);

	// 선택 transform gizmo 방향을 변경한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|PlaceableDetails")
	bool SetTransformGizmoOrientationMode(EScenarioTransformGizmoOrientationMode orientationMode);

	// 선택 transform gizmo 방향을 편집할 수 있는지 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|PlaceableDetails")
	bool CanEditTransformGizmoOrientationForSelection() const;

	// 현재 선택에 적용되는 transform gizmo 방향을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|PlaceableDetails")
	EScenarioTransformGizmoOrientationMode GetEffectiveTransformGizmoOrientationMode() const;

	// 선택된 placeable 삭제를 요청한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|PlaceableDetails")
	bool DeleteSelectedPlaceable(FString& outFailureReason);

	// 선택된 placeable instance id 변경을 요청한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|PlaceableDetails")
	bool RenameSelectedPlaceableInstanceId(const FString& instanceId, FString& outFailureReason);

	// 선택된 placeable transform 변경을 요청한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|PlaceableDetails")
	bool UpdateSelectedPlaceableTransform(const FTransform& transform, FString& outFailureReason);

private:
	// Subsystem command 접근 지점.
	UPROPERTY(Transient)
	TObjectPtr<UScenarioEditorUiSubsystem> UiSubsystem;

	// 선택된 placeable instance id.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|PlaceableDetails", meta = (AllowPrivateAccess = "true"))
	FString SelectedInstanceId;

	// Details panel title 표시 문자열.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|PlaceableDetails", meta = (AllowPrivateAccess = "true"))
	FString TitleText;

	// Details panel에 선택 대상이 있는지 여부.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|PlaceableDetails", meta = (AllowPrivateAccess = "true"))
	bool bHasSelection = false;
};
