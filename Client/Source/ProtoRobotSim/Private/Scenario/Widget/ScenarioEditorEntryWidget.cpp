#include "Scenario/Widget/ScenarioEditorEntryWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Scenario/Editor/ScenarioEditorController.h"
#include "Scenario/Widget/ScenarioAssetPaletteWidget.h"
#include "Scenario/Widget/ScenarioEditorRootWidget.h"
#include "Platform/ScenarioEditorLaunchSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogScenarioEditorEntryWidget, Log, All);

void UScenarioEditorEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (NewScenarioButton)
	{
		NewScenarioButton->OnClicked.RemoveDynamic(this, &UScenarioEditorEntryWidget::HandleNewScenarioButtonClicked);
		NewScenarioButton->OnClicked.AddDynamic(this, &UScenarioEditorEntryWidget::HandleNewScenarioButtonClicked);
	}

	if (LoadScenarioButton)
	{
		LoadScenarioButton->OnClicked.RemoveDynamic(this, &UScenarioEditorEntryWidget::HandleLoadScenarioButtonClicked);
		LoadScenarioButton->OnClicked.AddDynamic(this, &UScenarioEditorEntryWidget::HandleLoadScenarioButtonClicked);
	}
}

void UScenarioEditorEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RequestEditorWidgetInputMode();
	BindScenarioEditorLaunchSubsystem();
}

void UScenarioEditorEntryWidget::NativeDestruct()
{
	UnbindScenarioEditorLaunchSubsystem();
	ReleaseEditorWidgetInputMode();
	Super::NativeDestruct();
}

void UScenarioEditorEntryWidget::StartNewScenario()
{
	AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		UE_LOG(LogScenarioEditorEntryWidget, Warning, TEXT("Owning player is not an ScenarioEditorController."));
		return;
	}

	editorController->NewScenarioDraft();
	UE_LOG(LogScenarioEditorEntryWidget, Log, TEXT("New scenario draft created."));
	FinishSuccessfulStart(false);
}

bool UScenarioEditorEntryWidget::LoadScenarioFromPathTextBox()
{
	if (!ScenarioSetupJsonPathTextBox)
	{
		UE_LOG(LogScenarioEditorEntryWidget, Warning, TEXT("ScenarioSetupJsonPathTextBox is not bound."));
		return false;
	}

	const FString jsonFilePath = ScenarioSetupJsonPathTextBox->GetText().ToString();
	if (jsonFilePath.IsEmpty())
	{
		UE_LOG(LogScenarioEditorEntryWidget, Warning, TEXT("ScenarioSetup JSON path is empty."));
		return false;
	}

	AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		UE_LOG(LogScenarioEditorEntryWidget, Warning, TEXT("Owning player is not an ScenarioEditorController."));
		return false;
	}

	FString resolvedJsonFilePath;
	TArray<FString> loadMessages;
	UE_LOG(LogScenarioEditorEntryWidget, Log, TEXT("ScenarioSetup JSON load requested | Input: %s"), *jsonFilePath);

	const bool bLoaded = editorController->LoadScenarioSetupJsonFile(jsonFilePath, resolvedJsonFilePath, loadMessages);
	if (loadMessages.IsEmpty())
	{
		loadMessages.Add(bLoaded
			? FString::Printf(TEXT("Loaded ScenarioSetup JSON: %s"), *resolvedJsonFilePath)
			: TEXT("ScenarioSetup JSON load failed."));
	}

	if (bLoaded)
	{
		UE_LOG(
			LogScenarioEditorEntryWidget,
			Log,
			TEXT("ScenarioSetup JSON load succeeded | Input: %s | Resolved: %s"),
			*jsonFilePath,
			*resolvedJsonFilePath);
	}
	else
	{
		UE_LOG(
			LogScenarioEditorEntryWidget,
			Warning,
			TEXT("ScenarioSetup JSON load failed | Input: %s | Resolved: %s"),
			*jsonFilePath,
			*resolvedJsonFilePath);
	}
	for (const FString& loadMessage : loadMessages)
	{
		UE_LOG(LogScenarioEditorEntryWidget, Log, TEXT("ScenarioSetup JSON load message | %s"), *loadMessage);
	}

	if (bLoaded)
	{
		FinishSuccessfulStart(true);
	}

	return bLoaded;
}

UScenarioAssetPaletteWidget* UScenarioEditorEntryWidget::ShowAssetPaletteWidget()
{
	UScenarioEditorRootWidget* rootWidget = GetEditorRootWidget();
	if (!rootWidget)
	{
		UE_LOG(LogScenarioEditorEntryWidget, Warning, TEXT("Editor root widget is unavailable."));
		return nullptr;
	}

	return rootWidget->ShowAssetPaletteWidget();
}

void UScenarioEditorEntryWidget::RemoveAssetPaletteWidget()
{
	if (UScenarioEditorRootWidget* rootWidget = GetEditorRootWidget())
	{
		rootWidget->HideAssetPaletteWidget();
	}
}

UScenarioAssetPaletteWidget* UScenarioEditorEntryWidget::GetAssetPaletteWidget() const
{
	if (const UScenarioEditorRootWidget* rootWidget = GetEditorRootWidget())
	{
		return rootWidget->GetAssetPaletteWidget();
	}

	return nullptr;
}

bool UScenarioEditorEntryWidget::CompleteExternallyStartedScenario(const bool bLoadedExistingScenario)
{
	// The launch subsystem event and late widget construction check can both arrive for one map load.
	if (bExternalStartCompleted)
	{
		return true;
	}

	if (!FinishSuccessfulStart(bLoadedExistingScenario))
	{
		return false;
	}

	bExternalStartCompleted = true;
	return true;
}

void UScenarioEditorEntryWidget::HandleNewScenarioButtonClicked()
{
	StartNewScenario();
}

void UScenarioEditorEntryWidget::HandleLoadScenarioButtonClicked()
{
	LoadScenarioFromPathTextBox();
}

void UScenarioEditorEntryWidget::BindScenarioEditorLaunchSubsystem()
{
	UGameInstance* gameInstance = GetGameInstance();
	if (!gameInstance)
	{
		return;
	}

	UScenarioEditorLaunchSubsystem* launchSubsystem = gameInstance->GetSubsystem<UScenarioEditorLaunchSubsystem>();
	if (!launchSubsystem)
	{
		return;
	}

	if (!AutoStartCompletedHandle.IsValid())
	{
		AutoStartCompletedHandle = launchSubsystem->OnAutoStartCompleted().AddUObject(
			this,
			&UScenarioEditorEntryWidget::HandleAutoStartCompleted);
	}

	if (launchSubsystem->HasAutoStartedScenarioEditorSession())
	{
		CompleteExternallyStartedScenario(
			launchSubsystem->WasAutoStartedScenarioEditorSessionLoadedExistingScenario());
	}
}

void UScenarioEditorEntryWidget::UnbindScenarioEditorLaunchSubsystem()
{
	if (!AutoStartCompletedHandle.IsValid())
	{
		return;
	}

	if (UGameInstance* gameInstance = GetGameInstance())
	{
		if (UScenarioEditorLaunchSubsystem* launchSubsystem =
				gameInstance->GetSubsystem<UScenarioEditorLaunchSubsystem>())
		{
			launchSubsystem->OnAutoStartCompleted().Remove(AutoStartCompletedHandle);
		}
	}

	AutoStartCompletedHandle.Reset();
}

void UScenarioEditorEntryWidget::HandleAutoStartCompleted(const bool bLoadedExistingScenario)
{
	CompleteExternallyStartedScenario(bLoadedExistingScenario);
}

void UScenarioEditorEntryWidget::RequestEditorWidgetInputMode()
{
	if (AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer()))
	{
		editorController->RequestEditorWidgetInputMode(this);
	}
}

void UScenarioEditorEntryWidget::ReleaseEditorWidgetInputMode()
{
	if (AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer()))
	{
		editorController->ReleaseEditorWidgetInputMode(this);
	}
}

bool UScenarioEditorEntryWidget::FinishSuccessfulStart(bool bLoadedExistingScenario)
{
	if (bShowAssetPaletteOnSuccessfulStart)
	{
		if (UScenarioEditorRootWidget* rootWidget = GetEditorRootWidget())
		{
			rootWidget->HandleEditorSessionStarted(bLoadedExistingScenario);
		}
		else
		{
			UE_LOG(LogScenarioEditorEntryWidget, Warning, TEXT("Editor root widget is unavailable."));
		}
	}

	OnScenarioEditorSessionStarted(bLoadedExistingScenario);
	HideAfterSuccessfulStartIfNeeded();
	return true;
}

void UScenarioEditorEntryWidget::HideAfterSuccessfulStartIfNeeded()
{
	if (bHideOnSuccessfulStart)
	{
		ReleaseEditorWidgetInputMode();
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

UScenarioEditorRootWidget* UScenarioEditorEntryWidget::GetEditorRootWidget() const
{
	if (AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer()))
	{
		return editorController->GetEditorRootWidget();
	}

	return nullptr;
}
