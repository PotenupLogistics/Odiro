#pragma once

#include "CoreMinimal.h"
#include "Platform/ViewModel/OdiroViewModelBase.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "ScenarioEditorOutlinerViewModel.generated.h"

class UScenarioEditorListItemViewModel;
class UScenarioEditorUiSubsystem;

// Scenario Editor outliner의 반복 row 상태와 selection command facade.
UCLASS(BlueprintType)
class ODIROSIM_API UScenarioEditorOutlinerViewModel : public UOdiroViewModelBase
{
	GENERATED_BODY()

public:
	// World subsystem 소유 관계를 연결한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Outliner")
	void InitializeForSubsystem(UScenarioEditorUiSubsystem* uiSubsystem);

	// Template panel 기본 row 목록을 다시 구성한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Outliner")
	void RefreshTemplatePanels();

	// C++ outliner adapter가 만든 row 상태를 item ViewModel 목록으로 소유한다.
	void SetItemsFromOutlinerRows(const TArray<FScenarioOutlinerItemViewModel>& rowViewModels);

	// Outliner item 선택 command를 shell/controller에 위임한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Outliner")
	bool SelectItem(UScenarioEditorListItemViewModel* itemViewModel);

	// Outliner item key 선택 상태를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Outliner")
	void SetSelectedItemKey(const FString& itemKey);

	// 선택된 outliner item key를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Outliner")
	FString GetSelectedItemKey() const { return SelectedItemKey; }

	// Outliner row ViewModel 목록을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Outliner")
	TArray<UScenarioEditorListItemViewModel*> GetItems() const;

private:
	// Template panel row 하나를 생성한다.
	UScenarioEditorListItemViewModel* CreateTemplatePanelItem(
		const FString& itemKey,
		const FString& label,
		EScenarioTemplateSidebarPanel panel,
		int32 depth);

	// Subsystem을 통해 shell/controller command에 접근한다.
	UPROPERTY(Transient)
	TObjectPtr<UScenarioEditorUiSubsystem> UiSubsystem;

	// 현재 선택된 row key.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|Outliner", meta = (AllowPrivateAccess = "true"))
	FString SelectedItemKey;

	// Outliner row ViewModel 목록.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|Outliner", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UScenarioEditorListItemViewModel>> Items;
};
