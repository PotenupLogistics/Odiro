#pragma once

#include "CoreMinimal.h"
#include "Platform/SimulatorLaunchSubsystem.h"
#include "Platform/Widget/OdiroActivatableScreenWidget.h"
#include "StartupMenuWidget.generated.h"

class UButton;
class UComboBoxString;
class UEditableTextBox;
class UPlatformUiSubsystem;
class UProjectTemplateCardWidget;
class UStartupMenuViewModel;
class UTextBlock;
class UWidget;
class UWidgetSwitcher;
class UWrapBox;

// StartupMap project picker that selects or creates a user project before loading ScenarioEditorMap.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UStartupMenuWidget : public UOdiroActivatableScreenWidget
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

	// Validates the selected project through the simulator launch subsystem.
	bool ValidateSelectedProject(TArray<FString>& outDiagnostics, USimulatorLaunchSubsystem* simulatorLaunchSubsystem = nullptr);

	// Adds an existing user project to the recent list without opening it.
	bool AddRecentProjectForPrototype(
		const FString& projectPath,
		TArray<FString>& outDiagnostics,
		USimulatorLaunchSubsystem* simulatorLaunchSubsystem = nullptr);

	// Returns normalized recent project paths ordered newest-first.
	TArray<FString> GetRecentProjectPathsForPrototype();

	// Startup menu ViewModel 연결 상태를 반환한다.
	UFUNCTION(BlueprintPure, Category = "StartupMenu|ViewModel")
	UStartupMenuViewModel* GetStartupMenuViewModel() const { return StartupMenuViewModel; }

protected:
	UFUNCTION()
	void HandleProjectOpenInputChanged(const FText& text);

	UFUNCTION()
	void HandleCreateNewProjectClicked();

	UFUNCTION()
	void HandleOpenProjectClicked();

	UFUNCTION()
	void HandleAddRecentProjectClicked();

	UFUNCTION()
	void HandleBackToRecentProjectsClicked();

	UFUNCTION()
	void HandleCreateProjectClicked();

	UFUNCTION()
	void HandleProjectParentFolderBrowseClicked();

	UFUNCTION()
	void HandleScenarioPresetSelectionChanged(FString selectedItem, ESelectInfo::Type selectionType);

	UFUNCTION()
	void HandleProfilePresetSelectionChanged(FString selectedItem, ESelectInfo::Type selectionType);

	UFUNCTION()
	void HandlePolicyPresetSelectionChanged(FString selectedItem, ESelectInfo::Type selectionType);

private:
	void BindControls();
	bool ValidateRequiredBindings() const;
	void InitializeProjectPathInputs();
	void LoadProjectOpenOptions();
	void SaveProjectOpenOptions();
	bool RemoveRecentProject(const FString& projectPath);
	void CacheProjectOpenOptionsFromWidgets();
	void ShowRecentProjectsScreen();
	void ShowCreateProjectScreen();
	void RefreshRecentProjectCards();
	void RefreshProjectPresetOptions();
	void RefreshProjectOpenActions();
	void RefreshProjectPresetSelectionStates();
	void SetProjectOpenWarningText(const FString& message);
	void SetDiagnosticsText(const FString& message);
	bool BrowseForProjectParentFolder(FString& outFolder) const;
	bool BrowseForExistingProjectFolder(FString& outFolder) const;
	bool AddRecentProjectIfValid(
		const FString& projectPath,
		TArray<FString>& outDiagnostics,
		USimulatorLaunchSubsystem* simulatorLaunchSubsystem = nullptr);
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
	UStartupMenuViewModel* EnsureStartupMenuViewModel(USimulatorLaunchSubsystem* simulatorLaunchSubsystem = nullptr);
	UPlatformUiSubsystem* GetPlatformUiSubsystem() const;

	// Startup flow screen switcher owned by WBP_StartupMenu.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> StartupScreenSwitcher;

	// PlatformUiSubsystem이 소유하는 startup ViewModel 참조.
	UPROPERTY(Transient)
	TObjectPtr<UStartupMenuViewModel> StartupMenuViewModel;

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

	// Opens an existing user project folder from the recent project screen.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> OpenProjectButton;

	// Adds an existing user project folder to the recent project list.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> RecentProjectAddButton;

	// Navigates from the creation form back to recent projects.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> BackToRecentProjectsButton;

	// User project parent directory input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> ProjectParentFolderTextBox;

	// Opens an OS folder picker for the project parent directory.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ProjectParentFolderBrowseButton;

	// User project directory name input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> ProjectNameTextBox;

	// Scenario preset dropdown.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> ScenarioPresetSelectionBox;

	// Profile preset dropdown.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> ProfilePresetSelectionBox;

	// Policy preset dropdown.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> PolicyPresetSelectionBox;

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
