#pragma once

#include "CoreMinimal.h"
#include "BaseWidgetTypes.generated.h"

// Visual emphasis variant shared by independent base widgets.
UENUM(BlueprintType)
enum class EBaseWidgetVariant : uint8
{
	Neutral,
	Primary,
	Secondary,
	Success,
	Warning,
	Danger,
	Info,
	Ghost
};

// Size scale shared by independent base widgets.
UENUM(BlueprintType)
enum class EBaseWidgetSize : uint8
{
	Small,
	Medium,
	Large
};

// Optional min/max desired-size overrides applied only when a WBP binds RootSize or RootSizeBox.
USTRUCT(BlueprintType)
struct ODIROSIM_API FBaseWidgetSizeConstraints
{
	GENERATED_BODY()

	// Minimum desired width in Slate units; zero clears the desired-size constraint.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Layout", meta = (ClampMin = "0.0", UIMin = "0.0", ExposeOnSpawn = "true"))
	float MinWidth = 0.0f;

	// Minimum desired height in Slate units; zero clears the desired-size constraint.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Layout", meta = (ClampMin = "0.0", UIMin = "0.0", ExposeOnSpawn = "true"))
	float MinHeight = 0.0f;

	// Maximum desired width in Slate units; zero clears the desired-size constraint.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Layout", meta = (ClampMin = "0.0", UIMin = "0.0", ExposeOnSpawn = "true"))
	float MaxWidth = 0.0f;

	// Maximum desired height in Slate units; zero clears the desired-size constraint.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Layout", meta = (ClampMin = "0.0", UIMin = "0.0", ExposeOnSpawn = "true"))
	float MaxHeight = 0.0f;
};

// Visual state shared by independent base widgets.
UENUM(BlueprintType)
enum class EBaseWidgetState : uint8
{
	Default,
	Hovered,
	Pressed,
	Selected,
	Disabled,
	Loading,
	Success,
	Warning,
	Error
};

// Semantic typography role used by base widget tokens.
UENUM(BlueprintType)
enum class EBaseTextRole : uint8
{
	Title,
	Label,
	Body,
	Caption,
	Value
};
