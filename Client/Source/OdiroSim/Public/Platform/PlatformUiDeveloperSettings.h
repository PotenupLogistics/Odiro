#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PlatformUiDeveloperSettings.generated.h"

class UStartupMenuWidget;
class UMainMenuWidget;
class UExperimentResultIterationSelectorWidget;
class UProjectExperimentRunRowWidget;
class UProjectTemplateCardWidget;
class UUserWidget;

// Project-level Platform UI asset references used before a Widget Blueprint instance exists.
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Platform UI"))
class ODIROSIM_API UPlatformUiDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// Returns the Project Settings category for OdiroSim settings.
	virtual FName GetCategoryName() const override;

	// StartupMap root screen Widget Blueprint class.
	UPROPERTY(EditAnywhere, Config, Category = "Startup")
	TSoftClassPtr<UStartupMenuWidget> StartupMenuWidgetClass;

	// ScenarioEditorMap project workspace Widget Blueprint class.
	UPROPERTY(EditAnywhere, Config, Category = "Workspace")
	TSoftClassPtr<UMainMenuWidget> MainMenuWidgetClass;

	// Startup project/recent preset card Widget Blueprint class.
	UPROPERTY(EditAnywhere, Config, Category = "Startup")
	TSoftClassPtr<UProjectTemplateCardWidget> ProjectTemplateCardWidgetClass;

	// Project experiment status row Widget Blueprint class.
	UPROPERTY(EditAnywhere, Config, Category = "Workspace|ProjectExperiment")
	TSoftClassPtr<UProjectExperimentRunRowWidget> ProjectExperimentRunRowWidgetClass;

	// Project run episode replay card Widget Blueprint class.
	UPROPERTY(EditAnywhere, Config, Category = "Workspace|ProjectResult")
	TSoftClassPtr<UUserWidget> ProjectEpisodeReplayCardWidgetClass;

	// Project run AI suggestion row Widget Blueprint class.
	UPROPERTY(EditAnywhere, Config, Category = "Workspace|ProjectResult")
	TSoftClassPtr<UUserWidget> ProjectAiSuggestionRowWidgetClass;

	// Project run result iteration selector Widget Blueprint class.
	UPROPERTY(EditAnywhere, Config, Category = "Workspace|ProjectResult")
	TSoftClassPtr<UExperimentResultIterationSelectorWidget> ExperimentResultIterationSelectorWidgetClass;
};
