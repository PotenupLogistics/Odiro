#pragma once

#include "CoreMinimal.h"
#include "UI/BaseWidget.h"
#include "RecentProjectContextMenuWidget.generated.h"

class UBaseButtonWidget;
class URecentProjectContextMenuWidget;
class UWidget;

DECLARE_MULTICAST_DELEGATE_OneParam(
	FRecentProjectContextMenuNative,
	URecentProjectContextMenuWidget*);

// Wires WBP-authored recent-project context menu command buttons to native events.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API URecentProjectContextMenuWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Places the menu surface at a viewport-local position.
	UFUNCTION(BlueprintCallable, Category = "Platform|Recent Projects")
	void OpenAtViewportPosition(const FVector2D& viewportPosition);

	// Notifies the owner that the recent-project list removal command was selected.
	FRecentProjectContextMenuNative OnRemoveFromListSelected;

	// Notifies the owner that the project-folder delete command was selected.
	FRecentProjectContextMenuNative OnDeleteProjectSelected;

	// Notifies the owner that the full-viewport dismiss area requested close.
	FRecentProjectContextMenuNative OnDismissRequested;

protected:
	// Binds WBP-owned button delegates.
	virtual void NativeConstruct() override;

	// Releases WBP-owned button delegates.
	virtual void NativeDestruct() override;

	// Captures full-screen outside clicks before transparent child widgets can consume them.
	virtual FReply NativeOnPreviewMouseButtonDown(
		const FGeometry& inGeometry,
		const FPointerEvent& inMouseEvent) override;

private:
	// Converts the remove-from-list button click to a native event.
	UFUNCTION()
	void HandleRemoveFromListSelected(UBaseButtonWidget* button);

	// Converts the delete-project button click to a native event.
	UFUNCTION()
	void HandleDeleteProjectSelected(UBaseButtonWidget* button);

	// WBP-owned menu surface positioned over the full-viewport dismiss overlay.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MenuSurface;

	// WBP-owned container that owns the menu surface hit-test bounds.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MenuAnchor;

	// WBP-owned button for removing the project only from the recent list.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> RemoveFromListButton;

	// WBP-owned button for requesting physical project-folder deletion.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> DeleteProjectButton;
};
