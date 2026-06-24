#pragma once

#include "CoreMinimal.h"
#include "Platform/ViewModel/OdiroViewModelBase.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "ScenarioEditorShellViewModel.generated.h"

class UScenarioEditorUiSubsystem;

// Scenario Editor root shell의 표시 상태와 editor controller command facade.
UCLASS(BlueprintType)
class ODIROSIM_API UScenarioEditorShellViewModel : public UOdiroViewModelBase
{
	GENERATED_BODY()

public:
	// World subsystem 소유 관계를 연결하고 controller 상태를 읽는다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Shell")
	void InitializeForSubsystem(UScenarioEditorUiSubsystem* uiSubsystem);

	// Controller의 view mode, snap, selection 상태를 표시 상태로 복사한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Shell")
	void RefreshFromController();

	// Inspector tab을 선택한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Shell")
	void SelectInspectorTab(EScenarioEditorInspectorTab tab);

	// Scenario template sidebar panel을 선택한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Shell")
	void SelectSidebarPanel(EScenarioTemplateSidebarPanel panel);

	// Selects a template sidebar panel and clears any placeable selection.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Shell")
	void SelectTemplatePanel(EScenarioTemplateSidebarPanel panel);

	// Selects a template block path within the requested sidebar panel.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Shell")
	void SelectTemplateBlock(EScenarioTemplateSidebarPanel panel, const FString& blockPath);

	// Asset palette visibility 표시 상태를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Shell")
	void SetAssetPaletteVisible(bool bVisible);

	// LLM panel visibility 표시 상태를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Shell")
	void SetLlmPanelVisible(bool bVisible);

	// Editor view mode command를 controller에 위임한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Shell")
	bool SetEditorViewMode(EScenarioEditorViewMode viewMode);

	// Top-down orthographic editor view mode로 전환한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Shell")
	bool SetTopDownOrthoViewMode();

	// Perspective editor view mode로 전환한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Shell")
	bool SetPerspectiveViewMode();

	// Placement grid snap command를 controller에 위임한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Shell")
	bool SetPlacementSnapToGridEnabled(bool bEnabled);

	// Placement grid snap 상태를 반전한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Shell")
	bool TogglePlacementSnapToGrid();

	// Placeable selection command를 controller에 위임한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Shell")
	bool SelectPlaceable(const FString& instanceId);

	// Placeable selection을 해제한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Shell")
	void ClearSelectedPlaceable();

	// Clears placeable selection and restores template block selection for the active panel.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Shell")
	void ClearSelection();

	// 현재 inspector tab을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Shell")
	EScenarioEditorInspectorTab GetActiveInspectorTab() const { return ActiveInspectorTab; }

	// 현재 sidebar panel을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Shell")
	EScenarioTemplateSidebarPanel GetActiveSidebarPanel() const { return ActiveSidebarPanel; }

	// 현재 editor view mode를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Shell")
	EScenarioEditorViewMode GetViewMode() const { return ViewMode; }

	// Placement grid snap 표시 상태를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Shell")
	bool IsPlacementSnapToGridEnabled() const { return bPlacementSnapToGridEnabled; }

	// Asset palette visibility 표시 상태를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Shell")
	bool IsAssetPaletteVisible() const { return bAssetPaletteVisible; }

	// LLM panel visibility 표시 상태를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Shell")
	bool IsLlmPanelVisible() const { return bLlmPanelVisible; }

	// 선택된 placeable instance id를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Shell")
	FString GetSelectedPlaceableId() const { return SelectedPlaceableId; }

	// Returns the selected Scenario Template block path when no placeable is selected.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Shell")
	FString GetSelectedTemplateBlockPath() const { return SelectedTemplateBlockPath; }

private:
	// Subsystem을 통해 world/controller 명령에 접근한다.
	UPROPERTY(Transient)
	TObjectPtr<UScenarioEditorUiSubsystem> UiSubsystem;

	// 선택된 right inspector tab.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|Shell", meta = (AllowPrivateAccess = "true"))
	EScenarioEditorInspectorTab ActiveInspectorTab = EScenarioEditorInspectorTab::Detail;

	// 선택된 Scenario Template sidebar panel.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|Shell", meta = (AllowPrivateAccess = "true"))
	EScenarioTemplateSidebarPanel ActiveSidebarPanel = EScenarioTemplateSidebarPanel::Main;

	// Controller에서 읽은 editor camera projection mode.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|Shell", meta = (AllowPrivateAccess = "true"))
	EScenarioEditorViewMode ViewMode = EScenarioEditorViewMode::Perspective;

	// Controller에서 읽은 placement grid snap 상태.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|Shell", meta = (AllowPrivateAccess = "true"))
	bool bPlacementSnapToGridEnabled = true;

	// Asset palette 표시 상태.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|Shell", meta = (AllowPrivateAccess = "true"))
	bool bAssetPaletteVisible = false;

	// LLM panel 표시 상태.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|Shell", meta = (AllowPrivateAccess = "true"))
	bool bLlmPanelVisible = false;

	// 현재 선택된 placeable instance id.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|Shell", meta = (AllowPrivateAccess = "true"))
	FString SelectedPlaceableId;

	// Selected Scenario Template block path while the sidebar is in template editing mode.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|Shell", meta = (AllowPrivateAccess = "true"))
	FString SelectedTemplateBlockPath;
};
