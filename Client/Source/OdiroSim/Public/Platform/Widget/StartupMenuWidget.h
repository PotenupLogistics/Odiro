#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Platform/SimulatorLaunchSubsystem.h"
#include "StartupMenuWidget.generated.h"

class UButton;
class UEditableTextBox;
class UProjectSessionSubsystem;
class UProjectTemplateCardWidget;
class UScenarioEditorLaunchSubsystem;
class UTextBlock;
class UWidget;
class UWidgetSwitcher;
class UWrapBox;

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

	// Selects static project preset ids for the next create action.
	void SelectProjectPresets(
		const FString& scenarioPresetId,
		const FString& profilePresetId,
		const FString& policyPresetId);

	// Creates the selected project through the simulator launch subsystem.
	bool CreateSelectedProject(TArray<FString>& outDiagnostics, USimulatorLaunchSubsystem* simulatorLaunchSubsystem = nullptr);

	// Validates the selected project through the simulator launch subsystem.
	bool ValidateSelectedProject(TArray<FString>& outDiagnostics, USimulatorLaunchSubsystem* simulatorLaunchSubsystem = nullptr) const;

protected:
	UFUNCTION()
	void HandleProjectOpenInputChanged(const FText& text);

	UFUNCTION()
	void HandleCreateNewProjectClicked();

	UFUNCTION()
	void HandleBackToRecentProjectsClicked();

	UFUNCTION()
	void HandleCreateProjectClicked();

private:
	void BindControls();
	bool ValidateRequiredBindings() const;
	void InitializeProjectPathInputs();
	void LoadProjectOpenOptions();
	void SaveProjectOpenOptions();
	void LoadRecentProjectPaths();
	void SaveRecentProjectPaths() const;
	void RememberRecentProject(const FString& projectPath);
	bool RemoveRecentProject(const FString& projectPath);
	void CacheProjectOpenOptionsFromWidgets();
	void ShowRecentProjectsScreen();
	void ShowCreateProjectScreen();
	void RefreshRecentProjectCards();
	void RefreshProjectPresetOptions();
	void RefreshProjectOpenActions();
	void RefreshProjectPresetCardStates();
	void SetProjectOpenWarningText(const FString& message);
	void SetDiagnosticsText(const FString& message);
	bool OpenExistingProject(const FString& projectPath);
	bool CommitActiveProjectAndOpenEditor();
	void HandleRecentProjectCardSelected(UProjectTemplateCardWidget* cardWidget);
	void HandleRecentProjectCardContextRequested(UProjectTemplateCardWidget* cardWidget);
	void HandleScenarioPresetCardSelected(UProjectTemplateCardWidget* cardWidget);
	void HandleProfilePresetCardSelected(UProjectTemplateCardWidget* cardWidget);
	void HandlePolicyPresetCardSelected(UProjectTemplateCardWidget* cardWidget);
	void ShowRecentProjectDeleteDialog(const FString& projectPath);
	void HideRecentProjectDeleteDialog();

	UFUNCTION()
	void HandleConfirmRecentProjectDeleteClicked();

	UFUNCTION()
	void HandleCancelRecentProjectDeleteClicked();

	FString GetSelectedProjectParentFolder() const;
	FString GetSelectedProjectName() const;
	FString GetSelectedProjectPath() const;
	FProjectPresetSelection GetSelectedProjectPresetSelection() const;

	TSubclassOf<UProjectTemplateCardWidget> ResolveProjectTemplateCardWidgetClass() const;
	USimulatorLaunchSubsystem* GetSimulatorLaunchSubsystem() const;
	UScenarioEditorLaunchSubsystem* GetScenarioEditorLaunchSubsystem() const;
	UProjectSessionSubsystem* GetProjectSessionSubsystem() const;

	// Startup flow screen switcher owned by WBP_StartupMenu.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> StartupScreenSwitcher;

	// Recent project card screen owned by WBP_StartupMenu.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> RecentProjectsScreen;

	// Project creation screen owned by WBP_StartupMenu.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ProjectCreateScreen;

	// Recent project card wrap container.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWrapBox> RecentProjectCardWrapBox;

	// Empty recent project list text.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RecentProjectsEmptyText;

	// Recent project open validation text.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RecentProjectOpenWarningText;

	// Recent project diagnostics text.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RecentDiagnosticsTextBlock;

	// Recent project removal confirmation dialog root.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> RecentProjectDeleteDialog;

	// Recent project removal confirmation message.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RecentProjectDeleteDialogMessageText;

	// Confirms removal from the recent project list.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> RecentProjectDeleteConfirmButton;

	// Cancels recent project removal.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> RecentProjectDeleteCancelButton;

	// Navigates from recent projects to the creation form.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CreateNewProjectButton;

	// Navigates from the creation form back to recent projects.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> BackToRecentProjectsButton;

	// User project parent directory input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> ProjectParentFolderTextBox;

	// User project directory name input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> ProjectNameTextBox;

	// Scenario preset card container.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWrapBox> ScenarioPresetCardWrapBox;

	// Profile preset card container.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWrapBox> ProfilePresetCardWrapBox;

	// Policy preset card container.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWrapBox> PolicyPresetCardWrapBox;

	// User project create action.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CreateProjectButton;

	// Project open/create validation text.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ProjectOpenWarningText;

	// Startup diagnostics text.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DiagnosticsTextBlock;

	// Project preset card Widget Blueprint class.
	UPROPERTY(EditDefaultsOnly, Category = "StartupMenu|Project")
	TSubclassOf<UProjectTemplateCardWidget> ProjectTemplateCardWidgetClass;

	// Maximum recent projects retained by WBP_StartupMenu.
	UPROPERTY(EditDefaultsOnly, Category = "StartupMenu|Recent Projects", meta = (ClampMin = "1"))
	int32 MaxRecentProjectCount = 8;

	// Recent project cards currently owned by the recent list container.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProjectTemplateCardWidget>> RecentProjectCards;

	// Scenario preset cards currently owned by the create screen.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProjectTemplateCardWidget>> ScenarioPresetCards;

	// Profile preset cards currently owned by the create screen.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProjectTemplateCardWidget>> ProfilePresetCards;

	// Policy preset cards currently owned by the create screen.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProjectTemplateCardWidget>> PolicyPresetCards;

	// Normalized recent project paths ordered newest-first.
	TArray<FString> RecentProjectPaths;

	// Normalized project path awaiting recent-list removal confirmation.
	FString PendingRecentProjectDeletePath;

	// Cached create form parent folder.
	FString SelectedProjectParentFolder;

	// Cached create form project directory name.
	FString SelectedProjectName;

	// Cached scenario preset id for the next project create action.
	FString SelectedScenarioPresetId;

	// Cached profile preset id for the next project create action.
	FString SelectedProfilePresetId;

	// Cached policy preset id for the next project create action.
	FString SelectedPolicyPresetId;
};
