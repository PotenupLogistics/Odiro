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


