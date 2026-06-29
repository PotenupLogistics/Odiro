#pragma once

#include "CoreMinimal.h"
#include "Components/CheckBox.h"
#include "UI/BaseFormElementTypes.h"

class UImage;
class UTextBlock;
class UTexture2D;
class UWidget;

namespace BaseFormElementPrivate
{
	// Returns true when an id matches the item contract.
	bool MatchesId(FName lhs, FName rhs);

	// Converts a float to compact UI text without localization side effects.
	FText MakeNumberText(float value);

	// Parses a single floating-point value from user text.
	bool TryParseNumber(const FText& text, float& outValue);

	// Parses two numeric values separated by whitespace, comma, hyphen, or en dash.
	bool TryParseRange(const FText& text, float& outLowerValue, float& outUpperValue);

	// Normalizes an arbitrary value inside a numeric range to 0..1.
	float NormalizeValue(float value, float minValue, float maxValue);

	// Expands a normalized slider value back into its numeric range.
	float DenormalizeValue(float normalizedValue, float minValue, float maxValue);

	// Keeps min/max ordered for public numeric boundaries.
	void NormalizeMinMax(float& minValue, float& maxValue);

	// Keeps lower/upper ordered after clamping.
	void NormalizeRange(float& lowerValue, float& upperValue);

	// Sets a widget visibility using the non-hit-testable visible state for decorative children.
	void SetOptionalWidgetVisible(UWidget* widget, bool bVisible, ESlateVisibility visibleState = ESlateVisibility::SelfHitTestInvisible);

	// Applies text and collapses an optional text block when the value is empty.
	void SetTextBlockValue(UTextBlock* textBlock, const FText& value, bool bCollapseWhenEmpty = true);

	// Applies a texture to an image while preserving WBP-authored layout.
	void SetImageTexture(UImage* image, UTexture2D* texture);

	// Finds a switcher item by stable id.
	const FBaseSwitcherItem* FindSwitcherItemById(const TArray<FBaseSwitcherItem>& items, FName itemId);

	// Finds a dropdown item by stable id.
	const FBaseDropdownItem* FindDropdownItemById(const TArray<FBaseDropdownItem>& items, FName itemId);

	// Finds a checkbox item index by stable id.
	int32 FindCheckBoxItemIndexById(const TArray<FBaseCheckBoxGroupItem>& items, FName itemId);

	// Finds a tree row by stable id.
	const FBaseTreeRowItem* FindTreeRowById(const TArray<FBaseTreeRowItem>& items, FName itemId);

	// Returns whether the state should render an accent mark.
	bool IsCheckedLikeState(ECheckBoxState state);

	// Applies a compact text color override.
	void ApplyTextColor(UTextBlock* textBlock, const FLinearColor& color);

	// Anchors tooltips to the cursor tip and flips only when the viewport edge would hide the surface.
	FVector2D ResolveTooltipAlignment(const FVector2D& position, const FVector2D& viewportSize, const FVector2D& estimatedTooltipSize);
}
