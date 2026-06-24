#pragma once

#include "CoreMinimal.h"
#include "Platform/ViewModel/OdiroListItemViewModel.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "ScenarioEditorListItemViewModel.generated.h"

class UTexture2D;

// Scenario Editor의 outliner row와 palette tile이 공유하는 반복 item ViewModel.
UCLASS(BlueprintType)
class ODIROSIM_API UScenarioEditorListItemViewModel : public UOdiroListItemViewModel
{
	GENERATED_BODY()

public:
	// Outliner row 표시/command 상태를 설정한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Item")
	void InitializeOutlinerItem(
		const FString& itemKey,
		const FString& label,
		EScenarioEditorOutlinerItemType itemType,
		EScenarioTemplateSidebarPanel templatePanel,
		const FString& instanceId,
		int32 depth);

	// 기존 outliner row 상태를 UObject item ViewModel 상태로 변환한다.
	void InitializeFromOutlinerRow(const FScenarioOutlinerItemViewModel& rowViewModel);

	// Palette tile 표시/command 상태를 설정한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Item")
	void InitializePaletteItem(
		const FName& assetId,
		const FString& label,
		const FString& category,
		EScenarioPaletteItemType itemType);

	// Outliner item semantic type을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Item")
	EScenarioEditorOutlinerItemType GetOutlinerItemType() const { return OutlinerItemType; }

	// Outliner item semantic type을 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Item")
	void SetOutlinerItemType(EScenarioEditorOutlinerItemType itemType);

	// Template sidebar panel command 대상 값을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Item")
	EScenarioTemplateSidebarPanel GetTemplatePanel() const { return TemplatePanel; }

	// Template sidebar panel command 대상 값을 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Item")
	void SetTemplatePanel(EScenarioTemplateSidebarPanel panel);

	// Palette item type을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Item")
	EScenarioPaletteItemType GetPaletteItemType() const { return PaletteItemType; }

	// Palette item type을 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Item")
	void SetPaletteItemType(EScenarioPaletteItemType itemType);

	// Palette asset id를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Item")
	FName GetAssetId() const { return AssetId; }

	// Palette asset id를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Item")
	void SetAssetId(FName assetId);

	// Palette thumbnail texture 참조를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Item")
	TSoftObjectPtr<UTexture2D> GetThumbnailTexture() const { return ThumbnailTexture; }

	// Palette thumbnail texture 참조를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Item")
	void SetThumbnailTexture(TSoftObjectPtr<UTexture2D> thumbnailTexture);

	// Placeable instance id를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Item")
	FString GetInstanceId() const { return InstanceId; }

	// Placeable instance id를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Item")
	void SetInstanceId(const FString& instanceId);

	// Tree parent key를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Item")
	FString GetParentKey() const { return ParentKey; }

	// Tree parent key를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Item")
	void SetParentKey(const FString& parentKey);

	// Tree/list indent depth를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Item")
	int32 GetDepth() const { return Depth; }

	// Tree/list indent depth를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Item")
	void SetDepth(int32 depth);

	// Tree row가 확장 가능한지 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Item")
	bool IsExpandable() const { return bExpandable; }

	// Tree row 확장 가능 상태를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Item")
	void SetExpandable(bool bInExpandable);

	// Tree row가 현재 확장되어 있는지 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Item")
	bool IsExpanded() const { return bExpanded; }

	// Tree row 확장 상태를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Item")
	void SetExpanded(bool bInExpanded);

	// Placeable row actor category를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Item")
	EScenarioActorCategory GetActorCategory() const { return ActorCategory; }

	// Placeable row actor category를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Item")
	void SetActorCategory(EScenarioActorCategory actorCategory);

private:
	// Outliner row command semantic.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|Item", meta = (AllowPrivateAccess = "true"))
	EScenarioEditorOutlinerItemType OutlinerItemType = EScenarioEditorOutlinerItemType::TemplatePanel;

	// Template panel row command 대상.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|Item", meta = (AllowPrivateAccess = "true"))
	EScenarioTemplateSidebarPanel TemplatePanel = EScenarioTemplateSidebarPanel::Main;

	// Palette tile command semantic.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|Item", meta = (AllowPrivateAccess = "true"))
	EScenarioPaletteItemType PaletteItemType = EScenarioPaletteItemType::StaticObstacle;

	// Palette catalog asset id.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|Item", meta = (AllowPrivateAccess = "true"))
	FName AssetId;

	// Palette thumbnail soft reference.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|Item", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> ThumbnailTexture;

	// Placeable selection/command 대상 instance id.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|Item", meta = (AllowPrivateAccess = "true"))
	FString InstanceId;

	// Tree parent row key.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|Item", meta = (AllowPrivateAccess = "true"))
	FString ParentKey;

	// Tree/list indent depth.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|Item", meta = (AllowPrivateAccess = "true"))
	int32 Depth = 0;

	// Tree row expansion affordance state.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|Item", meta = (AllowPrivateAccess = "true"))
	bool bExpandable = false;

	// Tree row current expansion state.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|Item", meta = (AllowPrivateAccess = "true"))
	bool bExpanded = false;

	// Placeable actor category carried for legacy outliner event compatibility.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|Item", meta = (AllowPrivateAccess = "true"))
	EScenarioActorCategory ActorCategory = EScenarioActorCategory::StaticObstacle;
};
