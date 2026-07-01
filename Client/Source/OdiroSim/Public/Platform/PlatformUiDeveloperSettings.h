#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PlatformUiDeveloperSettings.generated.h"

class UPlatformRootWidget;

// Project-level Platform UI asset references used before a Widget Blueprint instance exists.
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Platform UI"))
class ODIROSIM_API UPlatformUiDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// Returns the Project Settings category for OdiroSim settings.
	virtual FName GetCategoryName() const override;

	// Unified Platform root shell Widget Blueprint class.
	UPROPERTY(EditAnywhere, Config, Category = "Root")
	TSoftClassPtr<UPlatformRootWidget> PlatformRootWidgetClass;
};
