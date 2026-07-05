#pragma once

#include "CoreMinimal.h"
#include "UI/BaseWidget.h"
#include "ProjectOverviewScreenWidget.generated.h"

class UBaseButtonWidget;
class UBaseTextWidget;
class UBaseTextInputWidget;
class UProjectWorkspaceViewModel;
class UImage;
class UTexture2D;
class UWidget;

class UProjectOverviewScreenWidget;

// Project overview navigation request emitted by guide buttons.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FProjectOverviewScreenRequested,
	UProjectOverviewScreenWidget*,
	Screen);

// Project workspace home surface that summarizes the active project and guide actions.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UProjectOverviewScreenWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Binds guide buttons and refreshes active project summary.
	virtual void NativeConstruct() override;

	// Releases guide button bindings.
	virtual void NativeDestruct() override;

	// Refreshes labels from ProjectWorkspaceViewModel.
	UFUNCTION(BlueprintCallable, Category = "Platform|Overview")
	void RefreshFromViewModel();

	// Returns the workspace ViewModel resolved through PlatformUiSubsystem.
	UFUNCTION(BlueprintPure, Category = "Platform|Overview")
	UProjectWorkspaceViewModel* GetViewModel() const { return ProjectWorkspaceViewModel.Get(); }

	// Scenario guide button request.
	UPROPERTY(BlueprintAssignable, Category = "Platform|Overview|Events")
	FProjectOverviewScreenRequested OnScenarioRequested;

	// Robot guide button request.
	UPROPERTY(BlueprintAssignable, Category = "Platform|Overview|Events")
	FProjectOverviewScreenRequested OnRobotRequested;

	// Experiment guide button request.
	UPROPERTY(BlueprintAssignable, Category = "Platform|Overview|Events")
	FProjectOverviewScreenRequested OnExperimentRequested;

private:
	// Resolves and caches the workspace ViewModel.
	UProjectWorkspaceViewModel* ResolveViewModel();

	// Loads active project preview.png into the overview thumbnail image.
	bool ApplyScenarioThumbnail(const FString& projectPath);

	// Applies the project-name edit/display visibility state to bound WBP controls.
	void UpdateProjectNameEditState();

	// Enters or exits project-name edit mode without changing WBP-authored layout or style.
	void SetProjectNameEditMode(bool bEditing);

	// Saves the current project-name edit field through the workspace ViewModel.
	void SaveProjectNameEdit();

	// Project name edit action click handler.
	UFUNCTION()
	void HandleEditProjectNameClicked(UBaseButtonWidget* button);

	// Project name save action click handler.
	UFUNCTION()
	void HandleSaveProjectNameClicked(UBaseButtonWidget* button);

	// Project name Enter-submit handler.
	UFUNCTION()
	void HandleProjectNameSubmitted(UBaseTextInputWidget* widget, const FText& text);

	// Scenario guide click handler.
	UFUNCTION()
	void HandleScenarioButtonClicked(UBaseButtonWidget* button);

	// Robot guide click handler.
	UFUNCTION()
	void HandleRobotButtonClicked(UBaseButtonWidget* button);

	// Policy guide click handler.
	UFUNCTION()
	void HandlePolicyButtonClicked(UBaseButtonWidget* button);

	// Experiment guide click handler.
	UFUNCTION()
	void HandleExperimentButtonClicked(UBaseButtonWidget* button);

	// ViewModel supplied by PlatformUiSubsystem.
	UPROPERTY(Transient)
	TObjectPtr<UProjectWorkspaceViewModel> ProjectWorkspaceViewModel;

	// Active project id display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ProjectNameTitle;

	// Active project id edit field.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextInputWidget> ProjectNameInput;

	// Action that switches the project id title into edit mode.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> EditProjectNameButton;

	// Action that saves the project id edit field.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> SaveProjectNameButton;

	// Active project root path display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ProjectPathText;

	// Active scenario path display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ScenarioPathText;

	// Run count display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> RunCountText;

	// Workspace status display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> StatusText;

	// Active project scenario preview thumbnail image.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> ScenarioThumbnailImage;

	// Guide action that opens the scenario editor screen.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> OpenScenarioButton;

	// Guide action that opens the robot profile screen.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> OpenRobotButton;

	// Guide action that opens the current policy editing surface.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> OpenPolicyButton;

	// Guide action that opens the experiment run list screen.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> OpenExperimentButton;

	// Guide action that opens the experiment result list surface.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> OpenResultButton;

	// Runtime-loaded preview.png texture kept alive for the overview thumbnail.
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ScenarioThumbnailTexture;

	// ViewModel이 없을 때 표시할 WBP-owned 상태 문구.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|Overview|Text", meta = (AllowPrivateAccess = "true"))
	FText WorkspaceViewModelMissingText = NSLOCTEXT("OdiroPlatform", "OverviewWorkspaceViewModelMissingText", "Workspace ViewModel 없음");

	// Project id 입력이 비어 있을 때 표시할 WBP-owned 오류 문구.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|Overview|Text", meta = (AllowPrivateAccess = "true"))
	FText ProjectNameRequiredText = NSLOCTEXT("OdiroPlatform", "OverviewProjectNameRequiredText", "Project ID를 입력하세요.");

	// Project id 저장 실패 시 표시할 WBP-owned 오류 문구.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|Overview|Text", meta = (AllowPrivateAccess = "true"))
	FText ProjectNameSaveFailedText = NSLOCTEXT("OdiroPlatform", "OverviewProjectNameSaveFailedText", "Project ID 저장 실패");

	// Project id 저장 성공 시 표시할 WBP-owned 상태 문구.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|Overview|Text", meta = (AllowPrivateAccess = "true"))
	FText ProjectNameSavedText = NSLOCTEXT("OdiroPlatform", "OverviewProjectNameSavedText", "Project ID 저장됨");

	// Whether the project id title row is currently showing the editable field.
	UPROPERTY(Transient)
	bool bEditingProjectName = false;
};
