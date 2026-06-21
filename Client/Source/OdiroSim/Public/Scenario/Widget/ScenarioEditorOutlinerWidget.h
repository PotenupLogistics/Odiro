#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "ScenarioEditorOutlinerWidget.generated.h"

class UScenarioEditorOutlinerRowWidget;
class UScrollBox;
class UTextBlock;
class UWidgetTextStyleCatalog;
class SWidget;

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
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Outliner")
	TSubclassOf<UScenarioEditorOutlinerRowWidget> RowWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Outliner")
	TSoftObjectPtr<UWidgetTextStyleCatalog> TextStyleCatalog;

	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Outliner")
	FScenarioOutlinerItemSelected OnItemSelected;

	// Rebuilds rows from the current authoring draft and spawned editor placeables.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Outliner")
	void RefreshFromEditorState();

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

	void BuildDefaultWidgetTree();
	void RebuildRows(const TArray<FScenarioOutlinerItemViewModel>& items);
	void CollectPlaceableItems(TArray<FScenarioOutlinerItemViewModel>& outPlaceableItems) const;
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

	UPROPERTY(Transient)
	TArray<FScenarioOutlinerItemViewModel> CachedItems;

	UPROPERTY(Transient)
	FString SelectedItemKey = TEXT("Scenario");

	TSet<FString> ExpandedItemKeys;
};
