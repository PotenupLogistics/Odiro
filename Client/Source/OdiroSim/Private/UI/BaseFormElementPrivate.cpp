#include "UI/BaseFormElementPrivate.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"
#include "Misc/DefaultValueHelper.h"

namespace BaseFormElementPrivate
{
	// Returns true when an id matches the item contract.
	bool MatchesId(const FName lhs, const FName rhs)
	{
		return !lhs.IsNone() && lhs == rhs;
	}

	// Converts a float to compact UI text without localization side effects.
	FText MakeNumberText(const float value)
	{
		FString text = FString::SanitizeFloat(value);
		text.RemoveFromEnd(TEXT(".0"));
		return FText::FromString(text);
	}

	// Parses a single floating-point value from user text.
	bool TryParseNumber(const FText& text, float& outValue)
	{
		double parsedValue = 0.0;
		if (!FDefaultValueHelper::ParseDouble(text.ToString(), parsedValue))
		{
			return false;
		}

		outValue = static_cast<float>(parsedValue);
		return true;
	}

	// Parses two numeric values separated by whitespace, comma, hyphen, or en dash.
	bool TryParseRange(const FText& text, float& outLowerValue, float& outUpperValue)
	{
		FString normalized = text.ToString();
		normalized.ReplaceInline(TEXT(","), TEXT(" "));
		normalized.ReplaceInline(TEXT("-"), TEXT(" "));
		normalized.ReplaceInline(TEXT("\u2013"), TEXT(" "));

		TArray<FString> parts;
		normalized.ParseIntoArrayWS(parts);
		if (parts.Num() < 2)
		{
			return false;
		}

		double parsedLower = 0.0;
		double parsedUpper = 0.0;
		if (!FDefaultValueHelper::ParseDouble(parts[0], parsedLower)
			|| !FDefaultValueHelper::ParseDouble(parts[1], parsedUpper))
		{
			return false;
		}

		outLowerValue = static_cast<float>(parsedLower);
		outUpperValue = static_cast<float>(parsedUpper);
		return true;
	}

	// Normalizes an arbitrary value inside a numeric range to 0..1.
	float NormalizeValue(const float value, const float minValue, const float maxValue)
	{
		if (FMath::IsNearlyEqual(minValue, maxValue))
		{
			return 0.0f;
		}

		return FMath::Clamp((value - minValue) / (maxValue - minValue), 0.0f, 1.0f);
	}

	// Expands a normalized slider value back into its numeric range.
	float DenormalizeValue(const float normalizedValue, const float minValue, const float maxValue)
	{
		return FMath::Lerp(minValue, maxValue, FMath::Clamp(normalizedValue, 0.0f, 1.0f));
	}

	// Keeps min/max ordered for public numeric boundaries.
	void NormalizeMinMax(float& minValue, float& maxValue)
	{
		if (minValue > maxValue)
		{
			Swap(minValue, maxValue);
		}
	}

	// Keeps lower/upper ordered after clamping.
	void NormalizeRange(float& lowerValue, float& upperValue)
	{
		if (lowerValue > upperValue)
		{
			Swap(lowerValue, upperValue);
		}
	}

	// Sets a widget visibility using the non-hit-testable visible state for decorative children.
	void SetOptionalWidgetVisible(UWidget* widget, const bool bVisible, const ESlateVisibility visibleState)
	{
		if (IsValid(widget))
		{
			widget->SetVisibility(bVisible ? visibleState : ESlateVisibility::Collapsed);
		}
	}

	// Applies text and collapses an optional text block when the value is empty.
	void SetTextBlockValue(UTextBlock* textBlock, const FText& value, const bool bCollapseWhenEmpty)
	{
		if (!IsValid(textBlock))
		{
			return;
		}

		textBlock->SetText(value);
		if (bCollapseWhenEmpty)
		{
			textBlock->SetVisibility(value.IsEmpty()
				? ESlateVisibility::Collapsed
				: ESlateVisibility::SelfHitTestInvisible);
		}
	}

	// Applies a texture to an image while preserving WBP-authored layout.
	void SetImageTexture(UImage* image, UTexture2D* texture)
	{
		if (!IsValid(image))
		{
			return;
		}

		if (texture)
		{
			image->SetBrushFromTexture(texture, false);
		}
		image->SetVisibility(texture ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	// Finds a switcher item by stable id.
	const FBaseSwitcherItem* FindSwitcherItemById(const TArray<FBaseSwitcherItem>& items, const FName itemId)
	{
		return items.FindByPredicate(
			[itemId](const FBaseSwitcherItem& item)
			{
				return MatchesId(item.Id, itemId);
			});
	}

	// Finds a dropdown item by stable id.
	const FBaseDropdownItem* FindDropdownItemById(const TArray<FBaseDropdownItem>& items, const FName itemId)
	{
		return items.FindByPredicate(
			[itemId](const FBaseDropdownItem& item)
			{
				return MatchesId(item.Id, itemId);
			});
	}

	// Finds a checkbox item index by stable id.
	int32 FindCheckBoxItemIndexById(const TArray<FBaseCheckBoxGroupItem>& items, const FName itemId)
	{
		return items.IndexOfByPredicate(
			[itemId](const FBaseCheckBoxGroupItem& item)
			{
				return MatchesId(item.Id, itemId);
			});
	}

	// Finds a tree row by stable id.
	const FBaseTreeRowItem* FindTreeRowById(const TArray<FBaseTreeRowItem>& items, const FName itemId)
	{
		return items.FindByPredicate(
			[itemId](const FBaseTreeRowItem& item)
			{
				return MatchesId(item.Id, itemId);
			});
	}

	// Returns whether the state should render an accent mark.
	bool IsCheckedLikeState(const ECheckBoxState state)
	{
		return state == ECheckBoxState::Checked || state == ECheckBoxState::Undetermined;
	}

	// Applies a compact text color override.
	void ApplyTextColor(UTextBlock* textBlock, const FLinearColor& color)
	{
		if (IsValid(textBlock))
		{
			textBlock->SetColorAndOpacity(FSlateColor(color));
		}
	}

	// Anchors tooltips to the cursor tip and flips only when the viewport edge would hide the surface.
	FVector2D ResolveTooltipAlignment(const FVector2D& position, const FVector2D& viewportSize, const FVector2D& estimatedTooltipSize)
	{
		FVector2D alignment(0.0f, 1.0f);
		const FVector2D clampedEstimatedSize(
			FMath::Max(0.0f, estimatedTooltipSize.X),
			FMath::Max(0.0f, estimatedTooltipSize.Y));
		if (viewportSize.X > 0.0f && position.X > viewportSize.X - clampedEstimatedSize.X)
		{
			alignment.X = 1.0f;
		}
		if (viewportSize.Y > 0.0f && position.Y < clampedEstimatedSize.Y)
		{
			alignment.Y = 0.0f;
		}
		return alignment;
	}
}
