#pragma once

#include "CoreMinimal.h"
#include "UI/BaseFormElementTypes.h"
#include "UI/BaseWidget.h"
#include "BaseProgressBarWidget.generated.h"

class UBorder;

// Standalone rounded progress bar component.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseProgressBarWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Applies the clamped progress value and semantic state to the WBP-owned track.
	virtual void SynchronizeBaseProperties() override;

	// Updates the progress percentage in the 0-100 range.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Progress Bar")
	void SetProgressPercent(float inProgressPercent);

	// Returns the progress percentage in the 0-100 range.
	UFUNCTION(BlueprintPure, Category = "UI|Base Progress Bar")
	float GetProgressPercent() const { return ProgressPercent; }

	// Updates the progress semantic state.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Progress Bar")
	void SetBaseState(EBaseWidgetState inState);

	// Returns the progress semantic state.
	UFUNCTION(BlueprintPure, Category = "UI|Base Progress Bar")
	EBaseWidgetState GetBaseState() const { return State; }

	// Enables explicit track and fill colors for this progress bar instance.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Progress Bar")
	void SetProgressColors(FLinearColor inTrackColor, FLinearColor inFillColor);

	// Disables explicit track and fill colors so catalog colors resolve again.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Progress Bar")
	void ClearProgressColorOverrides();

	// Enables an explicit track color for this progress bar instance.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Progress Bar")
	void SetTrackColorOverride(FLinearColor inTrackColor);

	// Disables the explicit track color so the catalog track color resolves again.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Progress Bar")
	void ClearTrackColorOverride();

	// Enables an explicit fill color for this progress bar instance.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Progress Bar")
	void SetFillColorOverride(FLinearColor inFillColor);

	// Disables the explicit fill color so the semantic state color resolves again.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Progress Bar")
	void ClearFillColorOverride();

protected:
	// Feeds the rounded progress material its painted size each paint (capture-safe).
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// Numeric progress percentage in the 0-100 range.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetProgressPercent", Setter = "SetProgressPercent", BlueprintGetter = "GetProgressPercent", BlueprintSetter = "SetProgressPercent", Category = "UI|State", meta = (ExposeOnSpawn = "true", ClampMin = "0.0", ClampMax = "100.0"))
	float ProgressPercent = 0.0f;

	// Progress semantic state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetBaseState", Setter = "SetBaseState", BlueprintGetter = "GetBaseState", BlueprintSetter = "SetBaseState", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	EBaseWidgetState State = EBaseWidgetState::Default;

	// Whether TrackColorOverride replaces the catalog track color.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Style|Progress", meta = (ExposeOnSpawn = "true"))
	bool bOverrideTrackColor = false;

	// Explicit progress track color used when bOverrideTrackColor is enabled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Style|Progress", meta = (ExposeOnSpawn = "true", EditCondition = "bOverrideTrackColor"))
	FLinearColor TrackColorOverride = FLinearColor::Transparent;

	// Whether FillColorOverride replaces the semantic state fill color.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Style|Progress", meta = (ExposeOnSpawn = "true"))
	bool bOverrideFillColor = false;

	// Explicit progress fill color used when bOverrideFillColor is enabled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Style|Progress", meta = (ExposeOnSpawn = "true", EditCondition = "bOverrideFillColor"))
	FLinearColor FillColorOverride = FLinearColor::Transparent;

	// Progress track surface owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ProgressTrack;
};
