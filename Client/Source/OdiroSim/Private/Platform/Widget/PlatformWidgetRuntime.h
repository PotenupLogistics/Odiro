#pragma once

#include "CoreMinimal.h"

class UUserWidget;

namespace PlatformWidgetRuntime
{
	// Sizes a runtime popup overlay to the current viewport without owning its surface layout.
	void ApplyFullscreenViewportSlot(UUserWidget* Widget);

	// Removes editor transaction flags from runtime-created widget trees.
	void ClearRuntimeTransactionFlags(UUserWidget* Widget);
}
