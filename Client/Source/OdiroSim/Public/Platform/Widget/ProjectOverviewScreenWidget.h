#pragma once

#include "CoreMinimal.h"
#include "UI/BaseWidget.h"
#include "ProjectOverviewScreenWidget.generated.h"

class UBaseButtonWidget;
class UBaseTextWidget;
class UProjectWorkspaceViewModel;
class UImage;
class UScrollBox;
class USpacer;
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

protected:
	// Keeps overlay scrollbars in sync with the WBP-owned overview content viewport.
	virtual void NativeTick(const FGeometry& myGeometry, float inDeltaTime) override;

private:
	// Resolves and caches the workspace ViewModel.
	UProjectWorkspaceViewModel* ResolveViewModel();

	// Loads active project preview.png into the overview thumbnail image.
	bool ApplyScenarioThumbnail(const FString& projectPath);

	// Binds overlay scrollbar delegates.
	void BindOverviewScrollbars();

	// Releases overlay scrollbar delegates.
	void UnbindOverviewScrollbars();

	// Updates overlay scrollbar visibility and scroll ranges.
	void UpdateOverviewOverlayScrollbars(const FVector2D& screenSize);

	// Stores WBP-authored scroll child padding before runtime centering adjusts it.
	bool CaptureOverviewAuthoredScrollPadding();

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

	// 화면 하단 가로 scrollbar 입력을 실제 overview scroll viewport에 반영한다.
	UFUNCTION()
	void HandleOverviewStickyHorizontalScrolled(float currentOffset);

	// 실제 overview scroll viewport의 가로 offset 변화를 화면 하단 scrollbar에 반영한다.
	UFUNCTION()
	void HandleOverviewContentHorizontalScrolled(float currentOffset);

	// 화면 오른쪽 세로 scrollbar 입력을 실제 overview scroll viewport에 반영한다.
	UFUNCTION()
	void HandleOverviewStickyVerticalScrolled(float currentOffset);

	// 실제 overview scroll viewport의 세로 offset 변화를 화면 오른쪽 scrollbar에 반영한다.
	UFUNCTION()
	void HandleOverviewContentVerticalScrolled(float currentOffset);

	// ViewModel supplied by PlatformUiSubsystem.
	UPROPERTY(Transient)
	TObjectPtr<UProjectWorkspaceViewModel> ProjectWorkspaceViewModel;

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

	// WBP-owned vertical content scroll viewport.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> ProjectOverviewScrollBox;

	// WBP-owned horizontal content scroll viewport.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> ProjectOverviewHorizontalScrollBox;

	// WBP-owned main overview content stack used to size overlay scroll ranges.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ProjectOverviewMainStack;

	// WBP-owned optional bottom overlay scrollbar.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> ProjectOverviewStickyHorizontalScrollBox;

	// WBP-owned optional bottom overlay scrollbar range spacer.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<USpacer> ProjectOverviewStickyHorizontalScrollSpacer;

	// WBP-owned optional right overlay scrollbar.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> ProjectOverviewStickyVerticalScrollBox;

	// WBP-owned optional right overlay scrollbar range spacer.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<USpacer> ProjectOverviewStickyVerticalScrollSpacer;

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

	// Last overlay scrollbar viewport size.
	UPROPERTY(Transient)
	FVector2D CachedOverviewScrollViewportSize = FVector2D::ZeroVector;

	// Last overlay scrollbar content desired size.
	UPROPERTY(Transient)
	FVector2D CachedOverviewScrollContentSize = FVector2D::ZeroVector;

	// WBP-authored padding for ProjectOverviewMainStack inside the horizontal scroll viewport.
	UPROPERTY(Transient)
	FMargin ProjectOverviewMainStackBasePadding;

	// WBP-authored padding for ProjectOverviewHorizontalScrollBox inside the vertical scroll viewport.
	UPROPERTY(Transient)
	FMargin ProjectOverviewHorizontalScrollBoxBasePadding;

	// Last computed horizontal overflow state.
	UPROPERTY(Transient)
	uint8 bCachedOverviewNeedsHorizontalScroll : 1 = false;

	// Last computed vertical overflow state.
	UPROPERTY(Transient)
	uint8 bCachedOverviewNeedsVerticalScroll : 1 = false;

	// Project overview scroll child padding capture 완료 여부.
	UPROPERTY(Transient)
	uint8 bHasProjectOverviewScrollBasePadding : 1 = false;

	// 가로 scroll offset 동기화 중 재진입을 막는다.
	UPROPERTY(Transient)
	bool bSyncingOverviewHorizontalScroll = false;

	// 세로 scroll offset 동기화 중 재진입을 막는다.
	UPROPERTY(Transient)
	bool bSyncingOverviewVerticalScroll = false;
};
