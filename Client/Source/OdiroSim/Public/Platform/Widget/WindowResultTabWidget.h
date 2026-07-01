#pragma once

#include "CoreMinimal.h"
#include "UI/BaseTabWidget.h"
#include "WindowResultTabWidget.generated.h"

class UBaseButtonWidget;

class UWindowResultTabWidget;

// Runtime result tab close request emitted by the close affordance.
DECLARE_MULTICAST_DELEGATE_OneParam(FWindowResultTabWidgetCloseRequestedNative, UWindowResultTabWidget*);

// Base tab variant that can expose a close button for dynamic result tabs.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UWindowResultTabWidget : public UBaseTabWidget
{
	GENERATED_BODY()

public:
	// Binds the optional close button owned by the Widget Blueprint.
	virtual void NativeConstruct() override;

	// Releases close button bindings.
	virtual void NativeDestruct() override;

	// Updates whether the tab close affordance is visible.
	UFUNCTION(BlueprintCallable, Category = "Window|Tab Bar")
	void SetClosable(bool bInClosable);

	// Returns whether the tab close affordance is visible.
	UFUNCTION(BlueprintPure, Category = "Window|Tab Bar")
	bool IsClosable() const { return bClosable; }

	// Close button click notification for the owning tab bar.
	FWindowResultTabWidgetCloseRequestedNative OnCloseRequestedNative;

private:
	// Forwards the optional close button click to the native owner.
	UFUNCTION()
	void HandleCloseClicked(UBaseButtonWidget* button);

	// Applies the current close affordance visibility to the WBP child.
	void RefreshCloseVisibility() const;

	// True when this dynamic tab should show a close affordance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Tab Bar", meta = (AllowPrivateAccess = "true"))
	bool bClosable = false;

	// Optional icon-only close button owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> CloseButton;
};
