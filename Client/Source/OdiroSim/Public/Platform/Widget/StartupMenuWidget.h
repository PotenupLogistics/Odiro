#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StartupMenuWidget.generated.h"

class UButton;
class UEditableTextBox;
class UHorizontalBox;
class UProjectSessionSubsystem;
class UProjectTemplateCardWidget;
class UScenarioEditorLaunchSubsystem;
class USimulatorLaunchSubsystem;
class UTextBlock;

// StartupMap project picker that selects or creates a user project before loading ScenarioEditorMap.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UStartupMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Creates the startup menu for StartupMap and applies menu input mode.
	UFUNCTION(BlueprintCallable, Category = "StartupMenu|UI", meta = (WorldContext = "WorldContextObject"))
	static UStartupMenuWidget* ShowStartupMenu(UObject* WorldContextObject, int32 ZOrder = 0);

	// Sets the project path used by tests and StartupMenu input widgets.
	void SetProjectPathForPrototype(const FString& projectPath);

	// Returns the currently selected normalized project path.
	FString GetProjectPathForPrototype() const;

	// Selects a static project template id for the next create action.
	void SelectProjectTemplate(const FString& templateId);

	// Creates the selected project through the simulator launch subsystem.
	bool CreateSelectedProject(TArray<FString>& outDiagnostics, USimulatorLaunchSubsystem* simulatorLaunchSubsystem = nullptr);

	// Validates the selected project through the simulator launch subsystem.
	bool ValidateSelectedProject(TArray<FString>& outDiagnostics, USimulatorLaunchSubsystem* simulatorLaunchSubsystem = nullptr) const;

protected:
	UFUNCTION()
	void HandleProjectOpenInputChanged(const FText& text);

	UFUNCTION()
	void HandleOpenProjectClicked();

	UFUNCTION()
	void HandleCreateProjectClicked();

private:
	void BindControls();
	bool ValidateRequiredBindings() const;
	void InitializeProjectPathInputs();
	void LoadProjectOpenOptions();
	void SaveProjectOpenOptions();
	void CacheProjectOpenOptionsFromWidgets();
	void RefreshProjectTemplateOptions();
	void RefreshProjectOpenActions();
	void RefreshProjectTemplateCardStates();
	void SetProjectOpenWarningText(const FString& message);
	void SetDiagnosticsText(const FString& message);
	bool CommitActiveProjectAndOpenEditor();
	void HandleProjectTemplateCardSelected(UProjectTemplateCardWidget* cardWidget);

	FString GetSelectedProjectParentFolder() const;
	FString GetSelectedProjectName() const;
	FString GetSelectedProjectPath() const;
	FString GetSelectedProjectTemplateId() const;

	TSubclassOf<UProjectTemplateCardWidget> ResolveProjectTemplateCardWidgetClass() const;
	USimulatorLaunchSubsystem* GetSimulatorLaunchSubsystem() const;
	UScenarioEditorLaunchSubsystem* GetScenarioEditorLaunchSubsystem() const;
	UProjectSessionSubsystem* GetProjectSessionSubsystem() const;

	// User project parent directory input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> ProjectParentFolderTextBox;

	// User project directory name input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> ProjectNameTextBox;

	// Static project template card container.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> ProjectTemplateCardBox;

	// User project create action.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CreateProjectButton;

	// Existing user project open action.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> OpenProjectButton;

	// Project open/create validation text.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ProjectOpenWarningText;

	// Startup diagnostics text.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DiagnosticsTextBlock;

	// Project template card Widget Blueprint class.
	UPROPERTY(EditDefaultsOnly, Category = "StartupMenu|Project")
	TSubclassOf<UProjectTemplateCardWidget> ProjectTemplateCardWidgetClass;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProjectTemplateCardWidget>> ProjectTemplateCards;

	FString SelectedProjectParentFolder;
	FString SelectedProjectName;
	FString SelectedProjectTemplateId;
};
