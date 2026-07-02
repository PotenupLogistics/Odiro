#pragma once

#include "CoreMinimal.h"
#include "UI/BaseButtonWidget.h"
#include "BaseTabWidget.generated.h"

class UBorder;
class UBaseTabWidget;
class UImage;
class UWidget;

// Close request emitted by an optional close affordance inside a tab.
DECLARE_MULTICAST_DELEGATE_OneParam(FBaseTabCloseRequestedNative, UBaseTabWidget*);

// Tab component for local navigation.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseTabWidget : public UBaseButtonWidget
{
	GENERATED_BODY()

public:
	// Keeps designer/runtime close affordance visibility aligned before construction.
	virtual void NativePreConstruct() override;

	// Binds optional tab affordances owned by the Widget Blueprint.
	virtual void NativeConstruct() override;

	// Releases optional tab affordance bindings.
	virtual void NativeDestruct() override;

	// Applies button state plus selected tab indicator.
	virtual void SynchronizeBaseProperties() override;

	// Updates whether the tab close affordance is visible.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Tab")
	void SetClosable(bool bInClosable);

	// Returns whether the tab close affordance is visible.
	UFUNCTION(BlueprintPure, Category = "UI|Base Tab")
	bool IsClosable() const { return bClosable; }

	// Updates the side divider size and refreshes the bound WBP divider widgets.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Tab|Divider")
	void SetDividerSize(float inDividerWidth, float inDividerHeight);

	// Updates whether the left and right side dividers are drawn.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Tab|Divider")
	void SetDividerEdgesVisible(bool bInShowLeftDivider, bool bInShowRightDivider);

	// Returns the authored side divider width in logical pixels.
	UFUNCTION(BlueprintPure, Category = "UI|Base Tab|Divider")
	float GetDividerWidth() const { return DividerWidth; }

	// Returns the authored side divider height in logical pixels.
	UFUNCTION(BlueprintPure, Category = "UI|Base Tab|Divider")
	float GetDividerHeight() const { return DividerHeight; }

	// Returns whether the left side divider should be drawn.
	UFUNCTION(BlueprintPure, Category = "UI|Base Tab|Divider")
	bool IsLeftDividerVisible() const { return bShowLeftDivider; }

	// Returns whether the right side divider should be drawn.
	UFUNCTION(BlueprintPure, Category = "UI|Base Tab|Divider")
	bool IsRightDividerVisible() const { return bShowRightDivider; }

	// Close button click notification for the owning tab surface.
	FBaseTabCloseRequestedNative OnCloseRequestedNative;

protected:
	// Selected-state marker owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SelectedIndicator;

	// True when this tab should show an optional close affordance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Tab", meta = (AllowPrivateAccess = "true"))
	bool bClosable = false;

	// Optional icon-only close button owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> CloseButton;

	// Optional icon/label stack that stays independent from the close affordance.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ContentStack;

	// Left side divider owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> LeftDivider;

	// Right side divider owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> RightDivider;

	// Side divider width in logical pixels.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Tab|Divider", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float DividerWidth = 0.5f;

	// Side divider height in logical pixels.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Tab|Divider", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float DividerHeight = 16.0f;

	// True when the WBP-authored left divider should draw.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Tab|Divider", meta = (AllowPrivateAccess = "true"))
	bool bShowLeftDivider = true;

	// True when the WBP-authored right divider should draw.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Tab|Divider", meta = (AllowPrivateAccess = "true"))
	bool bShowRightDivider = true;

private:
	// Forwards the optional close button click to the native owner.
	UFUNCTION()
	void HandleCloseClicked(UBaseButtonWidget* button);

	// Applies the current close affordance visibility to the WBP child.
	void RefreshCloseVisibility() const;

	// Keeps the tab overlay fill-sized while aligning only the icon/label group.
	void RefreshTabContentLayout() const;

	// Applies side divider size, color token, and edge-centered overlap offsets to WBP children.
	void RefreshDividerMetrics(bool bUpdateSlotLayout = false) const;
};
