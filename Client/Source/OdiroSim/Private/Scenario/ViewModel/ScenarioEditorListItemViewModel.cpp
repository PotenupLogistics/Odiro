#include "Scenario/ViewModel/ScenarioEditorListItemViewModel.h"

void UScenarioEditorListItemViewModel::InitializeOutlinerItem(
	const FString& itemKey,
	const FString& label,
	const EScenarioEditorOutlinerItemType itemType,
	const EScenarioTemplateSidebarPanel templatePanel,
	const FString& instanceId,
	const int32 depth)
{
	InitializeItem(itemKey, label, instanceId, FString());
	SetOutlinerItemType(itemType);
	SetTemplatePanel(templatePanel);
	SetInstanceId(instanceId);
	SetDepth(depth);
}

void UScenarioEditorListItemViewModel::InitializeFromOutlinerRow(
	const FScenarioOutlinerItemViewModel& rowViewModel)
{
	InitializeItem(
		rowViewModel.ItemKey,
		rowViewModel.Label.ToString(),
		rowViewModel.Subtitle.ToString(),
		FString());
	SetOutlinerItemType(rowViewModel.ItemType);
	SetTemplatePanel(rowViewModel.TemplatePanel);
	SetInstanceId(rowViewModel.InstanceId);
	SetParentKey(rowViewModel.ParentKey);
	SetDepth(rowViewModel.Depth);
	SetExpandable(rowViewModel.bExpandable);
	SetExpanded(rowViewModel.bExpanded);
	SetSelected(rowViewModel.bSelected);
	SetActorCategory(rowViewModel.ActorCategory);
}

void UScenarioEditorListItemViewModel::InitializePaletteItem(
	const FName& assetId,
	const FString& label,
	const FString& category,
	const EScenarioPaletteItemType itemType)
{
	InitializeItem(assetId.ToString(), label, category, FString());
	SetAssetId(assetId);
	SetPaletteItemType(itemType);
}

void UScenarioEditorListItemViewModel::SetOutlinerItemType(const EScenarioEditorOutlinerItemType itemType)
{
	UE_MVVM_SET_PROPERTY_VALUE(OutlinerItemType, itemType);
}

void UScenarioEditorListItemViewModel::SetTemplatePanel(const EScenarioTemplateSidebarPanel panel)
{
	UE_MVVM_SET_PROPERTY_VALUE(TemplatePanel, panel);
}

void UScenarioEditorListItemViewModel::SetPaletteItemType(const EScenarioPaletteItemType itemType)
{
	UE_MVVM_SET_PROPERTY_VALUE(PaletteItemType, itemType);
}

void UScenarioEditorListItemViewModel::SetAssetId(const FName assetId)
{
	UE_MVVM_SET_PROPERTY_VALUE(AssetId, assetId);
}

void UScenarioEditorListItemViewModel::SetThumbnailTexture(
	TSoftObjectPtr<UTexture2D> thumbnailTexture)
{
	UE_MVVM_SET_PROPERTY_VALUE(ThumbnailTexture, thumbnailTexture);
}

void UScenarioEditorListItemViewModel::SetInstanceId(const FString& instanceId)
{
	UE_MVVM_SET_PROPERTY_VALUE(InstanceId, instanceId);
}

void UScenarioEditorListItemViewModel::SetParentKey(const FString& parentKey)
{
	UE_MVVM_SET_PROPERTY_VALUE(ParentKey, parentKey);
}

void UScenarioEditorListItemViewModel::SetDepth(const int32 depth)
{
	UE_MVVM_SET_PROPERTY_VALUE(Depth, depth);
}

void UScenarioEditorListItemViewModel::SetExpandable(const bool bInExpandable)
{
	UE_MVVM_SET_PROPERTY_VALUE(bExpandable, bInExpandable);
}

void UScenarioEditorListItemViewModel::SetExpanded(const bool bInExpanded)
{
	UE_MVVM_SET_PROPERTY_VALUE(bExpanded, bInExpanded);
}

void UScenarioEditorListItemViewModel::SetActorCategory(const EScenarioActorCategory actorCategory)
{
	UE_MVVM_SET_PROPERTY_VALUE(ActorCategory, actorCategory);
}
