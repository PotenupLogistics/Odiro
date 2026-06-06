#include "Episode/Widget/EpisodeEditorEntryWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Episode/Editor/EpisodeEditorController.h"
#include "Episode/Widget/EpisodeAssetPaletteWidget.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogEpisodeEditorEntryWidget, Log, All);

void UEpisodeEditorEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (NewEpisodeButton)
	{
		NewEpisodeButton->OnClicked.RemoveDynamic(this, &UEpisodeEditorEntryWidget::HandleNewEpisodeButtonClicked);
		NewEpisodeButton->OnClicked.AddDynamic(this, &UEpisodeEditorEntryWidget::HandleNewEpisodeButtonClicked);
	}

	if (LoadEpisodeButton)
	{
		LoadEpisodeButton->OnClicked.RemoveDynamic(this, &UEpisodeEditorEntryWidget::HandleLoadEpisodeButtonClicked);
		LoadEpisodeButton->OnClicked.AddDynamic(this, &UEpisodeEditorEntryWidget::HandleLoadEpisodeButtonClicked);
	}
}

void UEpisodeEditorEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RequestEditorWidgetInputMode();
}

void UEpisodeEditorEntryWidget::NativeDestruct()
{
	ReleaseEditorWidgetInputMode();
	Super::NativeDestruct();
}

void UEpisodeEditorEntryWidget::StartNewEpisode()
{
	AEpisodeEditorController* editorController = Cast<AEpisodeEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		SetDiagnosticsText(TEXT("Owning player is not an EpisodeEditorController."));
		return;
	}

	editorController->NewEpisodeDraft();
	SetDiagnosticsText(TEXT("New episode draft created."));
	FinishSuccessfulStart(false);
}

bool UEpisodeEditorEntryWidget::LoadEpisodeFromPathTextBox()
{
	if (!EpisodeSetupJsonPathTextBox)
	{
		SetDiagnosticsText(TEXT("EpisodeSetupJsonPathTextBox is not bound."));
		return false;
	}

	const FString jsonFilePath = EpisodeSetupJsonPathTextBox->GetText().ToString();
	if (jsonFilePath.IsEmpty())
	{
		SetDiagnosticsText(TEXT("EpisodeSetup JSON path is empty."));
		return false;
	}

	AEpisodeEditorController* editorController = Cast<AEpisodeEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		SetDiagnosticsText(TEXT("Owning player is not an EpisodeEditorController."));
		return false;
	}

	FString resolvedJsonFilePath;
	TArray<FString> diagnostics;
	UE_LOG(LogEpisodeEditorEntryWidget, Log, TEXT("EpisodeSetup JSON load requested | Input: %s"), *jsonFilePath);

	const bool bLoaded = editorController->LoadEpisodeSetupJsonFile(jsonFilePath, resolvedJsonFilePath, diagnostics);
	if (diagnostics.IsEmpty())
	{
		diagnostics.Add(bLoaded
			? FString::Printf(TEXT("Loaded EpisodeSetup JSON: %s"), *resolvedJsonFilePath)
			: TEXT("EpisodeSetup JSON load failed."));
	}
	SetDiagnosticsFromLines(diagnostics);

	if (bLoaded)
	{
		UE_LOG(
			LogEpisodeEditorEntryWidget,
			Log,
			TEXT("EpisodeSetup JSON load succeeded | Input: %s | Resolved: %s"),
			*jsonFilePath,
			*resolvedJsonFilePath);
	}
	else
	{
		UE_LOG(
			LogEpisodeEditorEntryWidget,
			Warning,
			TEXT("EpisodeSetup JSON load failed | Input: %s | Resolved: %s"),
			*jsonFilePath,
			*resolvedJsonFilePath);
	}
	for (const FString& diagnostic : diagnostics)
	{
		UE_LOG(LogEpisodeEditorEntryWidget, Log, TEXT("EpisodeSetup JSON load diagnostic | %s"), *diagnostic);
	}

	if (bLoaded)
	{
		FinishSuccessfulStart(true);
	}

	return bLoaded;
}

void UEpisodeEditorEntryWidget::SetDiagnosticsFromLines(const TArray<FString>& diagnostics)
{
	SetDiagnosticsText(FString::Join(diagnostics, TEXT("\n")));
}

void UEpisodeEditorEntryWidget::SetDiagnosticsText(const FString& diagnostics)
{
	if (DiagnosticsTextBlock)
	{
		DiagnosticsTextBlock->SetText(FText::FromString(diagnostics));
	}
}

UEpisodeAssetPaletteWidget* UEpisodeEditorEntryWidget::ShowAssetPaletteWidget()
{
	if (IsValid(AssetPaletteWidget))
	{
		if (!AssetPaletteWidget->IsInViewport())
		{
			AssetPaletteWidget->AddToViewport(AssetPaletteViewportZOrder);
		}

		AssetPaletteWidget->RebuildPalette();
		return AssetPaletteWidget;
	}

	if (!AssetPaletteWidgetClass)
	{
		SetDiagnosticsText(TEXT("AssetPaletteWidgetClass is not set."));
		UE_LOG(LogEpisodeEditorEntryWidget, Warning, TEXT("AssetPaletteWidgetClass is not set."));
		return nullptr;
	}

	APlayerController* owningPlayer = GetOwningPlayer();
	if (!owningPlayer)
	{
		SetDiagnosticsText(TEXT("Owning player is unavailable."));
		UE_LOG(LogEpisodeEditorEntryWidget, Warning, TEXT("Owning player is unavailable."));
		return nullptr;
	}

	AssetPaletteWidget = CreateWidget<UEpisodeAssetPaletteWidget>(owningPlayer, AssetPaletteWidgetClass);
	if (!AssetPaletteWidget)
	{
		SetDiagnosticsText(TEXT("Failed to create AssetPaletteWidget."));
		UE_LOG(LogEpisodeEditorEntryWidget, Warning, TEXT("Failed to create AssetPaletteWidget."));
		return nullptr;
	}

	AssetPaletteWidget->AddToViewport(AssetPaletteViewportZOrder);
	AssetPaletteWidget->RebuildPalette();
	return AssetPaletteWidget;
}

void UEpisodeEditorEntryWidget::RemoveAssetPaletteWidget()
{
	if (IsValid(AssetPaletteWidget))
	{
		AssetPaletteWidget->RemoveFromParent();
	}

	AssetPaletteWidget = nullptr;
}

void UEpisodeEditorEntryWidget::HandleNewEpisodeButtonClicked()
{
	StartNewEpisode();
}

void UEpisodeEditorEntryWidget::HandleLoadEpisodeButtonClicked()
{
	LoadEpisodeFromPathTextBox();
}

void UEpisodeEditorEntryWidget::RequestEditorWidgetInputMode()
{
	if (AEpisodeEditorController* editorController = Cast<AEpisodeEditorController>(GetOwningPlayer()))
	{
		editorController->RequestEditorWidgetInputMode(this);
	}
}

void UEpisodeEditorEntryWidget::ReleaseEditorWidgetInputMode()
{
	if (AEpisodeEditorController* editorController = Cast<AEpisodeEditorController>(GetOwningPlayer()))
	{
		editorController->ReleaseEditorWidgetInputMode(this);
	}
}

bool UEpisodeEditorEntryWidget::FinishSuccessfulStart(bool bLoadedExistingEpisode)
{
	if (bShowAssetPaletteOnSuccessfulStart && !ShowAssetPaletteWidget())
	{
		return false;
	}

	OnEpisodeEditorSessionStarted(bLoadedExistingEpisode);
	HideAfterSuccessfulStartIfNeeded();
	return true;
}

void UEpisodeEditorEntryWidget::HideAfterSuccessfulStartIfNeeded()
{
	if (bHideOnSuccessfulStart)
	{
		ReleaseEditorWidgetInputMode();
		SetVisibility(ESlateVisibility::Collapsed);
	}
}
