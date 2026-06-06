#include "Episode/Widget/EpisodeEditorEntryWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Episode/Editor/EpisodeEditorController.h"
#include "Episode/Widget/EpisodeAssetPaletteWidget.h"
#include "Platform/EpisodeEditorLaunchSubsystem.h"
#include "Engine/GameInstance.h"
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
	BindEpisodeEditorLaunchSubsystem();
}

void UEpisodeEditorEntryWidget::NativeDestruct()
{
	UnbindEpisodeEditorLaunchSubsystem();
	ReleaseEditorWidgetInputMode();
	Super::NativeDestruct();
}

void UEpisodeEditorEntryWidget::StartNewEpisode()
{
	AEpisodeEditorController* editorController = Cast<AEpisodeEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		UE_LOG(LogEpisodeEditorEntryWidget, Warning, TEXT("Owning player is not an EpisodeEditorController."));
		return;
	}

	editorController->NewEpisodeDraft();
	UE_LOG(LogEpisodeEditorEntryWidget, Log, TEXT("New episode draft created."));
	FinishSuccessfulStart(false);
}

bool UEpisodeEditorEntryWidget::LoadEpisodeFromPathTextBox()
{
	if (!EpisodeSetupJsonPathTextBox)
	{
		UE_LOG(LogEpisodeEditorEntryWidget, Warning, TEXT("EpisodeSetupJsonPathTextBox is not bound."));
		return false;
	}

	const FString jsonFilePath = EpisodeSetupJsonPathTextBox->GetText().ToString();
	if (jsonFilePath.IsEmpty())
	{
		UE_LOG(LogEpisodeEditorEntryWidget, Warning, TEXT("EpisodeSetup JSON path is empty."));
		return false;
	}

	AEpisodeEditorController* editorController = Cast<AEpisodeEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		UE_LOG(LogEpisodeEditorEntryWidget, Warning, TEXT("Owning player is not an EpisodeEditorController."));
		return false;
	}

	FString resolvedJsonFilePath;
	TArray<FString> loadMessages;
	UE_LOG(LogEpisodeEditorEntryWidget, Log, TEXT("EpisodeSetup JSON load requested | Input: %s"), *jsonFilePath);

	const bool bLoaded = editorController->LoadEpisodeSetupJsonFile(jsonFilePath, resolvedJsonFilePath, loadMessages);
	if (loadMessages.IsEmpty())
	{
		loadMessages.Add(bLoaded
			? FString::Printf(TEXT("Loaded EpisodeSetup JSON: %s"), *resolvedJsonFilePath)
			: TEXT("EpisodeSetup JSON load failed."));
	}

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
	for (const FString& loadMessage : loadMessages)
	{
		UE_LOG(LogEpisodeEditorEntryWidget, Log, TEXT("EpisodeSetup JSON load message | %s"), *loadMessage);
	}

	if (bLoaded)
	{
		FinishSuccessfulStart(true);
	}

	return bLoaded;
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
		UE_LOG(LogEpisodeEditorEntryWidget, Warning, TEXT("AssetPaletteWidgetClass is not set."));
		return nullptr;
	}

	APlayerController* owningPlayer = GetOwningPlayer();
	if (!owningPlayer)
	{
		UE_LOG(LogEpisodeEditorEntryWidget, Warning, TEXT("Owning player is unavailable."));
		return nullptr;
	}

	AssetPaletteWidget = CreateWidget<UEpisodeAssetPaletteWidget>(owningPlayer, AssetPaletteWidgetClass);
	if (!AssetPaletteWidget)
	{
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

bool UEpisodeEditorEntryWidget::CompleteExternallyStartedEpisode(const bool bLoadedExistingEpisode)
{
	// The launch subsystem event and late widget construction check can both arrive for one map load.
	if (bExternalStartCompleted)
	{
		return true;
	}

	if (!FinishSuccessfulStart(bLoadedExistingEpisode))
	{
		return false;
	}

	bExternalStartCompleted = true;
	return true;
}

void UEpisodeEditorEntryWidget::HandleNewEpisodeButtonClicked()
{
	StartNewEpisode();
}

void UEpisodeEditorEntryWidget::HandleLoadEpisodeButtonClicked()
{
	LoadEpisodeFromPathTextBox();
}

void UEpisodeEditorEntryWidget::BindEpisodeEditorLaunchSubsystem()
{
	UGameInstance* gameInstance = GetGameInstance();
	if (!gameInstance)
	{
		return;
	}

	UEpisodeEditorLaunchSubsystem* launchSubsystem = gameInstance->GetSubsystem<UEpisodeEditorLaunchSubsystem>();
	if (!launchSubsystem)
	{
		return;
	}

	if (!AutoStartCompletedHandle.IsValid())
	{
		AutoStartCompletedHandle = launchSubsystem->OnAutoStartCompleted().AddUObject(
			this,
			&UEpisodeEditorEntryWidget::HandleAutoStartCompleted);
	}

	if (launchSubsystem->HasAutoStartedEpisodeEditorSession())
	{
		CompleteExternallyStartedEpisode(
			launchSubsystem->WasAutoStartedEpisodeEditorSessionLoadedExistingEpisode());
	}
}

void UEpisodeEditorEntryWidget::UnbindEpisodeEditorLaunchSubsystem()
{
	if (!AutoStartCompletedHandle.IsValid())
	{
		return;
	}

	if (UGameInstance* gameInstance = GetGameInstance())
	{
		if (UEpisodeEditorLaunchSubsystem* launchSubsystem =
				gameInstance->GetSubsystem<UEpisodeEditorLaunchSubsystem>())
		{
			launchSubsystem->OnAutoStartCompleted().Remove(AutoStartCompletedHandle);
		}
	}

	AutoStartCompletedHandle.Reset();
}

void UEpisodeEditorEntryWidget::HandleAutoStartCompleted(const bool bLoadedExistingEpisode)
{
	CompleteExternallyStartedEpisode(bLoadedExistingEpisode);
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
