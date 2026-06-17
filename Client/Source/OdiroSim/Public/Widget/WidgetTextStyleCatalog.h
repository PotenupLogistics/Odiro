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
	Value
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

	// Text style used for panel, section, or screen titles.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget|Text Style")
	FWidgetTextStyle Title;

	// Text style used for field labels and compact captions.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget|Text Style")
	FWidgetTextStyle Label;

	// Text style used for field values and body-like UI text.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget|Text Style")
	FWidgetTextStyle Value;

	// Returns the style configured for one semantic role.
	UFUNCTION(BlueprintPure, Category = "Widget|Text Style")
	FWidgetTextStyle GetStyle(EWidgetTextStyleRole role) const;
};
