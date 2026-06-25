#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "ScenarioEditorRouteMarkerOverlayWidget.generated.h"

class UScenarioEditorRootWidget;
class UTexture2D;

UCLASS(BlueprintType)
class ODIROSIM_API UScenarioEditorRouteMarkerOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	// Draws robot route markers in a viewport layer for editor and simulation worlds.
	virtual int32 NativePaint(
		const FPaintArgs& args,
		const FGeometry& allottedGeometry,
		const FSlateRect& myCullingRect,
		FSlateWindowElementList& outDrawElements,
		int32 layerId,
		const FWidgetStyle& inWidgetStyle,
		bool bParentEnabled) const override;

	// Copies route marker presentation values from the WBP-authored root widget.
	void ApplyStyleFromRootWidget(const UScenarioEditorRootWidget* rootWidget);

private:
	// Texture drawn for the editor-only robot start marker overlay.
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> RobotStartMarkerOverlayTexture;

	// Texture drawn for the editor-only robot goal marker overlay.
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> RobotGoalMarkerOverlayTexture;

	// Fixed screen-space size for route marker overlays, preserving the default 78:120 marker aspect.
	UPROPERTY(Transient)
	FVector2D RobotRouteMarkerOverlaySize = FVector2D(39.0, 60.0);

	// Normalized overlay anchor point; the marker tip is expected at the bottom center.
	UPROPERTY(Transient)
	FVector2D RobotRouteMarkerOverlayAnchor = FVector2D(0.5, 1.0);

	// Fallback tint for the robot start marker when no texture is assigned.
	UPROPERTY(Transient)
	FLinearColor RobotStartMarkerOverlayTint = FLinearColor(0.0f, 0.48f, 1.0f, 0.82f);

	// Fallback tint for the robot goal marker when no texture is assigned.
	UPROPERTY(Transient)
	FLinearColor RobotGoalMarkerOverlayTint = FLinearColor(1.0f, 0.03f, 0.03f, 0.82f);

	// Fixed screen-space size for corridor vertex square grips.
	UPROPERTY(Transient)
	FVector2D CorridorVertexHandleOverlaySize = FVector2D(12.0, 12.0);

	// Fixed screen-space size for corridor segment center grips.
	UPROPERTY(Transient)
	FVector2D CorridorSegmentGripOverlaySize = FVector2D(10.0, 10.0);

	// Screen-space thickness for corridor segment overlay lines.
	UPROPERTY(Transient)
	float CorridorSegmentLineThickness = 2.0f;

	// Default tint for inactive corridor handle overlays.
	UPROPERTY(Transient)
	FLinearColor CorridorHandleOverlayTint = FLinearColor(0.86f, 0.95f, 1.0f, 0.92f);

	// Tint for hovered corridor handle overlays.
	UPROPERTY(Transient)
	FLinearColor CorridorHandleOverlayHoverTint = FLinearColor(1.0f, 0.72f, 0.32f, 1.0f);

	// Tint for selected corridor handle overlays.
	UPROPERTY(Transient)
	FLinearColor CorridorHandleOverlaySelectedTint = FLinearColor(1.0f, 0.48f, 0.12f, 1.0f);

	// Ensures runtime-created overlays can use the same default marker textures as the editor root widget.
	void LoadDefaultMarkerOverlayTextures();
	// Paints editor-only corridor vertex and segment handles when the Corridor panel or handle selection is active.
	int32 PaintCorridorHandleOverlays(
		const FGeometry& allottedGeometry,
		FSlateWindowElementList& outDrawElements,
		int32 layerId) const;
	// Paints every visible robot route endpoint overlay for the current editor draft or simulation run.
	int32 PaintRobotRouteMarkerOverlays(
		const FGeometry& allottedGeometry,
		FSlateWindowElementList& outDrawElements,
		int32 layerId) const;
	// Paints one square corridor overlay grip at a projected local position.
	int32 PaintCorridorHandleGrip(
		const FScenarioEditorCorridorHandleOverlayItem& item,
		const FVector2D& localPosition,
		const FVector2D& gripSize,
		const FGeometry& allottedGeometry,
		FSlateWindowElementList& outDrawElements,
		int32 layerId) const;
	// Paints one fixed-size robot route endpoint marker at its projected widget position.
	int32 PaintRobotRouteMarkerOverlayItem(
		const FScenarioEditorRouteMarkerOverlayItem& item,
		const FVector2D& localPosition,
		const FGeometry& allottedGeometry,
		FSlateWindowElementList& outDrawElements,
		int32 layerId) const;
	// Projects a world location into this full-screen overlay widget's local paint space.
	bool TryProjectWorldLocationToOverlayPosition(
		const FVector& worldLocation,
		const FGeometry& allottedGeometry,
		FVector2D& outLocalPosition) const;
	// Resolves the visual color for one corridor handle state.
	FLinearColor ResolveCorridorHandleTint(const FScenarioEditorCorridorHandleOverlayItem& item) const;
};
