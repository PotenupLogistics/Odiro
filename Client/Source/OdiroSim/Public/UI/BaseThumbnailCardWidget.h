#pragma once

#include "CoreMinimal.h"
#include "UI/BaseFormElementTypes.h"
#include "UI/BaseWidget.h"
#include "BaseThumbnailCardWidget.generated.h"

class UBorder;
class UImage;
class UNamedSlot;
class UTexture2D;
class UWidget;

// Thumbnail card container with 4:3 media and a single content slot.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseThumbnailCardWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Applies media, padding, and selected state to bound WBP controls.
	virtual void SynchronizeBaseProperties() override;

	// Updates the card media texture.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Thumbnail Card")
	void SetMediaTexture(UTexture2D* inMediaTexture);

	// Returns the card media texture.
	UFUNCTION(BlueprintPure, Category = "UI|Base Thumbnail Card")
	UTexture2D* GetMediaTexture() const { return MediaTexture; }

	// Updates whether the media area is shown at all.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Thumbnail Card")
	void SetShowMedia(bool bInShowMedia);

	// Returns whether the media area is shown.
	UFUNCTION(BlueprintPure, Category = "UI|Base Thumbnail Card")
	bool ShouldShowMedia() const { return bShowMedia; }

	// Updates the media padding mode.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Thumbnail Card")
	void SetMediaPaddingMode(EBaseThumbnailMediaPaddingMode inPaddingMode);

	// Returns the media padding mode.
	UFUNCTION(BlueprintPure, Category = "UI|Base Thumbnail Card")
	EBaseThumbnailMediaPaddingMode GetMediaPaddingMode() const { return MediaPaddingMode; }

	// Updates whether the card renders selected.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Thumbnail Card")
	void SetSelected(bool bInSelected);

	// Returns whether the card renders selected.
	UFUNCTION(BlueprintPure, Category = "UI|Base Thumbnail Card")
	bool IsBaseSelected() const { return bSelected; }

	// Updates whether the card renders disabled.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Thumbnail Card")
	void SetDisabled(bool bInDisabled);

	// Returns whether the card renders disabled.
	UFUNCTION(BlueprintPure, Category = "UI|Base Thumbnail Card")
	bool IsDisabled() const { return bDisabled; }

protected:
	// Feeds rounded card material size on every paint.
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// Media texture shown in the 4:3 media area.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetMediaTexture", Setter = "SetMediaTexture", BlueprintGetter = "GetMediaTexture", BlueprintSetter = "SetMediaTexture", Category = "UI|Base Thumbnail Card", meta = (ExposeOnSpawn = "true"))
	TObjectPtr<UTexture2D> MediaTexture;

	// Whether the media area is shown; when false the card is content-only.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "ShouldShowMedia", Setter = "SetShowMedia", BlueprintGetter = "ShouldShowMedia", BlueprintSetter = "SetShowMedia", Category = "UI|Base Thumbnail Card", meta = (ExposeOnSpawn = "true"))
	bool bShowMedia = true;

	// Media padding policy.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetMediaPaddingMode", Setter = "SetMediaPaddingMode", BlueprintGetter = "GetMediaPaddingMode", BlueprintSetter = "SetMediaPaddingMode", Category = "UI|Base Thumbnail Card", meta = (ExposeOnSpawn = "true"))
	EBaseThumbnailMediaPaddingMode MediaPaddingMode = EBaseThumbnailMediaPaddingMode::FullBleed;

	// Selected card state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsBaseSelected", Setter = "SetSelected", BlueprintGetter = "IsBaseSelected", BlueprintSetter = "SetSelected", Category = "UI|Base Thumbnail Card", meta = (ExposeOnSpawn = "true"))
	bool bSelected = false;

	// Disabled card state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsDisabled", Setter = "SetDisabled", BlueprintGetter = "IsDisabled", BlueprintSetter = "SetDisabled", Category = "UI|Base Thumbnail Card", meta = (ExposeOnSpawn = "true"))
	bool bDisabled = false;

	// Rounded card surface owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SurfaceBorder;

	// Media area wrapper (toggled by bShowMedia) owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MediaOverlay;

	// Media wrapper owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> MediaBorder;

	// Media image owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> MediaImage;

	// Single content slot owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UNamedSlot> ContentSlot;
};
