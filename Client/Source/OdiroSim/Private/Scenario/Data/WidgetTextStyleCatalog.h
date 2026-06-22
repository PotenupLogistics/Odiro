#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Fonts/SlateFontInfo.h"
#include "WidgetTextStyleCatalog.generated.h"

// Semantic text roles shared by UMG widgets that need project-wide typography.
UENUM(BlueprintType)
enum class EWidgetTextStyleRole : uint8
{
	Title,
	Label,
	Value,
	Caption
};

// Font and color pair used by one semantic UMG text role.
USTRUCT(BlueprintType)
struct ODIROSIM_API FWidgetTextStyle
{
	GENERATED_BODY()

	// Creates a default value-style text style.
	FWidgetTextStyle();

	// Creates a text style with explicit font and color.
	FWidgetTextStyle(const FSlateFontInfo& inFont, const FLinearColor& inColor);

	// Font face, size, typeface, and outline used by the role.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget|Text Style")
	FSlateFontInfo Font;

	// Text color used by the role.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget|Text Style")
	FLinearColor Color = FLinearColor::White;
};

class UEditableText;
class UEditableTextBox;
class UComboBoxString;
class UMultiLineEditableTextBox;
class UTextBlock;
class UWidgetTree;

// DataAsset catalog for shared UMG typography roles.
UCLASS(BlueprintType)
class ODIROSIM_API UWidgetTextStyleCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	// Initializes built-in default styles for all semantic roles.
	UWidgetTextStyleCatalog();

	// Default project asset location for shared UMG text styling.
	static TSoftObjectPtr<UWidgetTextStyleCatalog> MakeDefaultCatalogReference();

	// Built-in fallback style for one semantic text role.
	static FWidgetTextStyle MakeDefaultStyle(EWidgetTextStyleRole role);

	// Resolves a configured style from a catalog reference, then falls back to the project default asset.
	static FWidgetTextStyle ResolveStyle(
		const TSoftObjectPtr<UWidgetTextStyleCatalog>& catalogReference,
		EWidgetTextStyleRole role);

	// Resolves a style from the project default catalog asset.
	static FWidgetTextStyle ResolveStyle(EWidgetTextStyleRole role);

	// Applies a resolved role style to a TextBlock widget.
	static void ApplyTextBlockStyle(
		UTextBlock* textBlock,
		const TSoftObjectPtr<UWidgetTextStyleCatalog>& catalogReference,
		EWidgetTextStyleRole role);

	// Applies a project-default role style to a TextBlock widget.
	static void ApplyTextBlockStyle(UTextBlock* textBlock, EWidgetTextStyleRole role);

	// Resolves a role style for inline editable text; runtime whole-style replacement is intentionally skipped.
	static void ApplyEditableTextStyle(
		UEditableText* editableText,
		const TSoftObjectPtr<UWidgetTextStyleCatalog>& catalogReference,
		EWidgetTextStyleRole role);

	// Resolves a project-default role style for inline editable text without mutating the widget style.
	static void ApplyEditableTextStyle(UEditableText* editableText, EWidgetTextStyleRole role);

	// Applies a resolved role style to a single-line editable text box.
	static void ApplyEditableTextBoxStyle(
		UEditableTextBox* textBox,
		const TSoftObjectPtr<UWidgetTextStyleCatalog>& catalogReference,
		EWidgetTextStyleRole role);

	// Applies a project-default role style to a single-line editable text box.
	static void ApplyEditableTextBoxStyle(UEditableTextBox* textBox, EWidgetTextStyleRole role);

	// Applies a resolved role style to a compact combo box.
	static void ApplyComboBoxStringStyle(
		UComboBoxString* comboBox,
		const TSoftObjectPtr<UWidgetTextStyleCatalog>& catalogReference,
		EWidgetTextStyleRole role);

	// Applies a project-default role style to a compact combo box.
	static void ApplyComboBoxStringStyle(UComboBoxString* comboBox, EWidgetTextStyleRole role);

	// Applies a resolved role style to a multiline editable text box.
	static void ApplyMultiLineEditableTextBoxStyle(
		UMultiLineEditableTextBox* textBox,
		const TSoftObjectPtr<UWidgetTextStyleCatalog>& catalogReference,
		EWidgetTextStyleRole role);

	// Applies a project-default role style to a multiline editable text box.
	static void ApplyMultiLineEditableTextBoxStyle(UMultiLineEditableTextBox* textBox, EWidgetTextStyleRole role);

	// Applies catalog styles to all supported text controls in a widget tree using widget-name role hints.
	static void ApplyWidgetTreeTextStyles(
		UWidgetTree* widgetTree,
		const TSoftObjectPtr<UWidgetTextStyleCatalog>& catalogReference);

	// Applies project-default catalog styles to all supported text controls in a widget tree.
	static void ApplyWidgetTreeTextStyles(UWidgetTree* widgetTree);

	// Text style used for panel, section, or screen titles.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget|Text Style")
	FWidgetTextStyle Title;

	// Text style used for field labels and compact command text.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget|Text Style")
	FWidgetTextStyle Label;

	// Text style used for field values and body-like UI text.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget|Text Style")
	FWidgetTextStyle Value;

	// Text style used for compact metadata such as Scenario Template field addresses.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget|Text Style")
	FWidgetTextStyle Caption;

	// Returns the style configured for one semantic role.
	UFUNCTION(BlueprintPure, Category = "Widget|Text Style")
	FWidgetTextStyle GetStyle(EWidgetTextStyleRole role) const;
};
