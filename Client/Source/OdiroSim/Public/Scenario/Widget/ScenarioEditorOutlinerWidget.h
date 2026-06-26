#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "ScenarioEditorOutlinerWidget.generated.h"

class UScenarioEditorOutlinerRowWidget;
class UScenarioEditorOutlinerViewModel;
class UScenarioEditorWidgetClassCatalog;
class UScenarioPlaceableComponent;
class UScrollBox;
class UTextBlock;
class UWidgetTextStyleCatalog;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FScenarioOutlinerItemSelected,
	FScenarioOutlinerItemViewModel,
	Item);

// Scenario-object outliner for the Scenario Editor right sidebar.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioEditorOutlinerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Outliner")
	TSubclassOf<UScenarioEditorOutlinerRowWidget> RowWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Outliner")
	TSoftObjectPtr<UWidgetTextStyleCatalog> TextStyleCatalog;

	// Widget Blueprint class catalog used for generated outliner rows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Outliner")
	TSoftObjectPtr<UScenarioEditorWidgetClassCatalog> WidgetClassCatalog;

	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Outliner")
	FScenarioOutlinerItemSelected OnItemSelected;

	// Rebuilds rows from the current authoring draft and spawned editor placeables.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Outliner")
	void RefreshFromEditorState();

	// Invalidates the cached placeable registry when actors/components were structurally added or removed.
	void InvalidatePlaceableRegistry();

	// Selects one row key without changing the controller selection.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Outliner")
	void SetSelectedItemKey(const FString& itemKey);

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Outliner")
	FString GetSelectedItemKey() const { return SelectedItemKey; }

	// Converts a selectable placeable instance id to the corresponding outliner key.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Outliner")
	static FString MakePlaceableItemKey(const FString& instanceId);

	// Converts a template detail panel to the corresponding outliner key.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Outliner")
	static FString MakeTemplateItemKey(EScenarioTemplateSidebarPanel panel);

	// Converts an actor category to the corresponding outliner group key.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Outliner")
	static FString MakeCategoryGroupItemKey(EScenarioActorCategory actorCategory);

	// Builds the outliner view model from explicit draft/placeable rows for tests.
	static void BuildOutlinerItems(
		const TArray<FScenarioOutlinerItemViewModel>& placeableItems,
		const FString& selectedItemKey,
		const TSet<FString>& expandedItemKeys,
		TArray<FScenarioOutlinerItemViewModel>& outItems);

private:
	UFUNCTION()
	void HandleRowSelected(FScenarioOutlinerItemViewModel item);

	UFUNCTION()
	void HandleRowExpansionToggled(FScenarioOutlinerItemViewModel item);

	// Resolves the subsystem-owned outliner ViewModel for selection display state.
	UScenarioEditorOutlinerViewModel* GetOutlinerViewModel() const;
	void RebuildRows(const TArray<FScenarioOutlinerItemViewModel>& items);
	// Collects placeable rows from the cached registry instead of scanning the world every refresh.
	void CollectPlaceableItems(TArray<FScenarioOutlinerItemViewModel>& outPlaceableItems);
	// Rebuilds the placeable registry from the current world after structural editor changes.
	void RebuildPlaceableRegistry();
	// Removes invalid or legacy-hidden components while keeping transiently unselectable rows recoverable.
	void CompactPlaceableRegistry();
	// Registers newly spawned placeable components after authoring preview actors are refreshed.
	void SyncPlaceableRegistryFromWorld();
	// Seeds default expanded group keys for the template hierarchy.
	void AddDefaultExpandedKeys();
	static FScenarioOutlinerItemViewModel MakeTemplateItem(
		const FString& itemKey,
		const FString& parentKey,
		const FText& label,
		int32 depth,
		EScenarioTemplateSidebarPanel panel,
		bool bExpandable = true);
	static FScenarioOutlinerItemViewModel MakePlaceableItem(
		const FString& parentKey,
		const FText& label,
		const FText& subtitle,
		int32 depth,
		const FString& instanceId,
		EScenarioActorCategory actorCategory);

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|Editor|Outliner", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UScrollBox> RowScrollBox;

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|Editor|Outliner", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> EmptyTextBlock;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UScenarioEditorOutlinerRowWidget>> RowWidgets;

	// Cached authoring placeables; row visibility is filtered separately from registry lifetime.
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UScenarioPlaceableComponent>> PlaceableComponentRegistry;

	UPROPERTY(Transient)
	TArray<FScenarioOutlinerItemViewModel> CachedItems;

	UPROPERTY(Transient)
	FString SelectedItemKey = TEXT("Scenario");

	TSet<FString> ExpandedItemKeys;

	// Tracks whether the placeable registry has performed its initial world scan.
	bool bPlaceableRegistryInitialized = false;
};
