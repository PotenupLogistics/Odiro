#include "Episode/Widget/EpisodeAssetPaletteWidget.h"

#include "Components/HorizontalBox.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Episode/Editor/EpisodeEditorController.h"
#include "Episode/Widget/EpisodePlaceablePaletteItemWidget.h"
#include "Shared/EpisodeCoreTypes.h"

void UEpisodeAssetPaletteWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RequestEditorWidgetInputMode();

	if (PaletteScrollBox)
	{
		PaletteScrollBox->SetOrientation(Orient_Horizontal);
	}

	if (bRebuildOnConstruct)
	{
		RebuildPalette();
	}
}

void UEpisodeAssetPaletteWidget::NativeDestruct()
{
	ReleaseEditorWidgetInputMode();
	Super::NativeDestruct();
}

bool UEpisodeAssetPaletteWidget::RebuildPalette()
{
	ClearPalette();

	if (!PlaceableItemContainer)
	{
		SetDiagnostics(TEXT("PlaceableItemContainer is not bound."));
		return false;
	}

	if (!PlaceableItemWidgetClass)
	{
		SetDiagnostics(TEXT("PlaceableItemWidgetClass is not set."));
		return false;
	}

	AEpisodeEditorController* editorController = Cast<AEpisodeEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		SetDiagnostics(TEXT("Owning player is not an EpisodeEditorController."));
		return false;
	}

	TArray<FEpisodeStaticObstaclePropEntry> paletteEntries;
	editorController->GetStaticObstaclePaletteEntries(paletteEntries);
	for (const FEpisodeStaticObstaclePropEntry& paletteEntry : paletteEntries)
	{
		UEpisodePlaceablePaletteItemWidget* itemWidget = CreateWidget<UEpisodePlaceablePaletteItemWidget>(
			editorController,
			PlaceableItemWidgetClass);
		if (!itemWidget) continue;

		itemWidget->SetPropEntry(paletteEntry);
		itemWidget->OnSelected.RemoveDynamic(this, &UEpisodeAssetPaletteWidget::HandlePaletteItemSelected);
		itemWidget->OnSelected.AddDynamic(this, &UEpisodeAssetPaletteWidget::HandlePaletteItemSelected);
		PlaceableItemContainer->AddChildToHorizontalBox(itemWidget);
	}

	SetDiagnostics(FString::Printf(TEXT("Loaded %d placeable assets."), paletteEntries.Num()));
	return true;
}

void UEpisodeAssetPaletteWidget::ClearPalette()
{
	if (PlaceableItemContainer)
	{
		PlaceableItemContainer->ClearChildren();
	}
}

void UEpisodeAssetPaletteWidget::HandlePaletteItemSelected(FName propId)
{
	AEpisodeEditorController* editorController = Cast<AEpisodeEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		SetDiagnostics(TEXT("Owning player is not an EpisodeEditorController."));
		return;
	}

	if (!editorController->BeginStaticObstaclePlacement(propId))
	{
		SetDiagnostics(FString::Printf(TEXT("Failed to begin placement for '%s'."), *propId.ToString()));
		return;
	}

	SetDiagnostics(FString::Printf(TEXT("Placement selected: %s"), *propId.ToString()));
}

void UEpisodeAssetPaletteWidget::SetDiagnostics(const FString& message)
{
	if (DiagnosticsTextBlock)
	{
		DiagnosticsTextBlock->SetText(FText::FromString(message));
	}
}

void UEpisodeAssetPaletteWidget::RequestEditorWidgetInputMode()
{
	if (AEpisodeEditorController* editorController = Cast<AEpisodeEditorController>(GetOwningPlayer()))
	{
		editorController->RequestEditorWidgetInputMode(this);
	}
}

void UEpisodeAssetPaletteWidget::ReleaseEditorWidgetInputMode()
{
	if (AEpisodeEditorController* editorController = Cast<AEpisodeEditorController>(GetOwningPlayer()))
	{
		editorController->ReleaseEditorWidgetInputMode(this);
	}
}
