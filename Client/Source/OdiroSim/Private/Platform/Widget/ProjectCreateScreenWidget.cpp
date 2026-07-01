#include "Platform/Widget/ProjectCreateScreenWidget.h"

#include "Blueprint/UserWidget.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/WrapBoxSlot.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Platform/Widget/ProjectPresetCardWidget.h"
#include "UI/BaseButtonWidget.h"
#include "UI/BaseTextInputWidget.h"
#include "UI/BaseTextWidget.h"

#if WITH_EDITOR
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#endif

namespace
{
	// 경로 입력을 dialog/create command용 absolute normalized path로 맞춘다.
	FString NormalizeProjectCreateWidgetPath(FString path)
	{
		path = path.TrimStartAndEnd();
		if (path.IsEmpty())
		{
			return FString();
		}

		path = FPaths::IsRelative(path)
			? FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), path))
			: FPaths::ConvertRelativePathToFull(path);
		FPaths::NormalizeFilename(path);
		return path;
	}

}

void UProjectCreateScreenWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
}

void UProjectCreateScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureProjectCreateScreenViewModel();
	ApplyViewModelConfiguration();
	BindControls();
	RefreshFromViewModel();
}

void UProjectCreateScreenWidget::NativeDestruct()
{
	UnbindControls();
	for (UProjectPresetCardWidget* cardWidget : PresetCards)
	{
		if (cardWidget)
		{
			cardWidget->OnSelectedRequested.RemoveAll(this);
		}
	}
	PresetCards.Reset();

	Super::NativeDestruct();
}

void UProjectCreateScreenWidget::SetViewModel(UProjectCreateScreenViewModel* viewModel)
{
	ProjectCreateScreenViewModel = viewModel;
	if (ProjectCreateScreenViewModel)
	{
		ApplyViewModelConfiguration();
		ProjectCreateScreenViewModel->InitializeForGameInstance(GetGameInstance());
	}
	RefreshFromViewModel();
}

void UProjectCreateScreenWidget::RefreshFromViewModel()
{
	UProjectCreateScreenViewModel* viewModel = EnsureProjectCreateScreenViewModel();
	if (!viewModel)
	{
		return;
	}

	if (ProjectNameInput && !ProjectNameInput->GetText().ToString().Equals(viewModel->GetProjectName(), ESearchCase::CaseSensitive))
	{
		ProjectNameInput->SetText(FText::FromString(viewModel->GetProjectName()));
	}
	if (ProjectParentFolderInput
		&& !ProjectParentFolderInput->GetText().ToString().Equals(viewModel->GetProjectParentFolder(), ESearchCase::CaseSensitive))
	{
		ProjectParentFolderInput->SetText(FText::FromString(viewModel->GetProjectParentFolder()));
	}
	if (ProjectParentFolderText)
	{
		ProjectParentFolderText->SetText(FText::FromString(viewModel->GetProjectParentFolder()));
	}
	if (ProjectPathText)
	{
		ProjectPathText->SetText(FText::FromString(viewModel->GetProjectPath()));
	}
	if (SelectedScenarioText)
	{
		SelectedScenarioText->SetText(FText::FromString(viewModel->GetSelectedScenarioSummary()));
	}
	if (SelectedProfileText)
	{
		SelectedProfileText->SetText(FText::FromString(viewModel->GetSelectedProfileSummary()));
	}
	if (SelectedPolicyText)
	{
		SelectedPolicyText->SetText(FText::FromString(viewModel->GetSelectedPolicySummary()));
	}
	if (DiagnosticsText)
	{
		const FString diagnostics = viewModel->GetDiagnosticsText();
		DiagnosticsText->SetText(FText::FromString(diagnostics));
		DiagnosticsText->SetVisibility(diagnostics.TrimStartAndEnd().IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}

	const bool bBusy = viewModel->IsBusy();
	if (ProjectNameInput)
	{
		ProjectNameInput->SetDisabled(bBusy);
	}
	if (ProjectParentFolderInput)
	{
		ProjectParentFolderInput->SetDisabled(bBusy);
	}
	if (BrowseFolderButton)
	{
		BrowseFolderButton->SetDisabled(bBusy);
	}
	if (CreateProjectButton)
	{
		CreateProjectButton->SetDisabled(!viewModel->CanCreateProject() || bBusy);
	}

	RefreshPresetCards();
}

bool UProjectCreateScreenWidget::CreateCurrentProject()
{
	UProjectCreateScreenViewModel* viewModel = EnsureProjectCreateScreenViewModel();
	if (!viewModel)
	{
		return false;
	}

	TArray<FString> diagnostics;
	const bool bCreated = viewModel->CreateProject(diagnostics);
	RefreshFromViewModel();
	if (bCreated)
	{
		OnProjectCreated.Broadcast(this, viewModel->GetProjectPath());
	}
	return bCreated;
}

UProjectCreateScreenViewModel* UProjectCreateScreenWidget::EnsureProjectCreateScreenViewModel()
{
	if (!ProjectCreateScreenViewModel)
	{
		ProjectCreateScreenViewModel = NewObject<UProjectCreateScreenViewModel>(this);
		if (ProjectCreateScreenViewModel)
		{
			ApplyViewModelConfiguration();
			ProjectCreateScreenViewModel->InitializeForGameInstance(GetGameInstance());
		}
	}
	return ProjectCreateScreenViewModel;
}

void UProjectCreateScreenWidget::ApplyViewModelConfiguration()
{
	if (ProjectCreateScreenViewModel)
	{
		FProjectCreateScreenDefaultValues defaultValues = DefaultValues;
		if (defaultValues.ProjectName.TrimStartAndEnd().IsEmpty() && ProjectNameInput)
		{
			defaultValues.ProjectName = ProjectNameInput->GetText().ToString();
		}
		if (defaultValues.ProjectParentFolder.TrimStartAndEnd().IsEmpty() && ProjectParentFolderInput)
		{
			defaultValues.ProjectParentFolder = ProjectParentFolderInput->GetText().ToString();
		}
		if (defaultValues.ProjectParentFolder.TrimStartAndEnd().IsEmpty() && ProjectParentFolderText)
		{
			defaultValues.ProjectParentFolder = ProjectParentFolderText->GetText().ToString();
		}

		ProjectCreateScreenViewModel->SetDiagnosticMessages(DiagnosticMessages);
		ProjectCreateScreenViewModel->SetDefaultValues(defaultValues);
	}
}

void UProjectCreateScreenWidget::BindControls()
{
	if (ProjectNameInput)
	{
		ProjectNameInput->OnTextChanged.RemoveDynamic(this, &UProjectCreateScreenWidget::HandleProjectNameChanged);
		ProjectNameInput->OnTextCommitted.RemoveDynamic(this, &UProjectCreateScreenWidget::HandleProjectNameChanged);
		ProjectNameInput->OnTextChanged.AddDynamic(this, &UProjectCreateScreenWidget::HandleProjectNameChanged);
		ProjectNameInput->OnTextCommitted.AddDynamic(this, &UProjectCreateScreenWidget::HandleProjectNameChanged);
	}
	if (ProjectParentFolderInput)
	{
		ProjectParentFolderInput->OnTextChanged.RemoveDynamic(this, &UProjectCreateScreenWidget::HandleProjectParentFolderChanged);
		ProjectParentFolderInput->OnTextCommitted.RemoveDynamic(this, &UProjectCreateScreenWidget::HandleProjectParentFolderChanged);
		ProjectParentFolderInput->OnTextChanged.AddDynamic(this, &UProjectCreateScreenWidget::HandleProjectParentFolderChanged);
		ProjectParentFolderInput->OnTextCommitted.AddDynamic(this, &UProjectCreateScreenWidget::HandleProjectParentFolderChanged);
	}
	if (BrowseFolderButton)
	{
		BrowseFolderButton->OnBaseClicked.RemoveDynamic(this, &UProjectCreateScreenWidget::HandleBrowseFolderClicked);
		BrowseFolderButton->OnBaseClicked.AddDynamic(this, &UProjectCreateScreenWidget::HandleBrowseFolderClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnBaseClicked.RemoveDynamic(this, &UProjectCreateScreenWidget::HandleCancelClicked);
		CancelButton->OnBaseClicked.AddDynamic(this, &UProjectCreateScreenWidget::HandleCancelClicked);
	}
	if (CreateProjectButton)
	{
		CreateProjectButton->OnBaseClicked.RemoveDynamic(this, &UProjectCreateScreenWidget::HandleCreateProjectClicked);
		CreateProjectButton->OnBaseClicked.AddDynamic(this, &UProjectCreateScreenWidget::HandleCreateProjectClicked);
	}
}

void UProjectCreateScreenWidget::UnbindControls()
{
	if (ProjectNameInput)
	{
		ProjectNameInput->OnTextChanged.RemoveDynamic(this, &UProjectCreateScreenWidget::HandleProjectNameChanged);
		ProjectNameInput->OnTextCommitted.RemoveDynamic(this, &UProjectCreateScreenWidget::HandleProjectNameChanged);
	}
	if (ProjectParentFolderInput)
	{
		ProjectParentFolderInput->OnTextChanged.RemoveDynamic(this, &UProjectCreateScreenWidget::HandleProjectParentFolderChanged);
		ProjectParentFolderInput->OnTextCommitted.RemoveDynamic(this, &UProjectCreateScreenWidget::HandleProjectParentFolderChanged);
	}
	if (BrowseFolderButton)
	{
		BrowseFolderButton->OnBaseClicked.RemoveDynamic(this, &UProjectCreateScreenWidget::HandleBrowseFolderClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnBaseClicked.RemoveDynamic(this, &UProjectCreateScreenWidget::HandleCancelClicked);
	}
	if (CreateProjectButton)
	{
		CreateProjectButton->OnBaseClicked.RemoveDynamic(this, &UProjectCreateScreenWidget::HandleCreateProjectClicked);
	}
}

void UProjectCreateScreenWidget::RefreshPresetCards()
{
	for (UProjectPresetCardWidget* cardWidget : PresetCards)
	{
		if (cardWidget)
		{
			cardWidget->OnSelectedRequested.RemoveAll(this);
		}
	}
	PresetCards.Reset();

	UProjectCreateScreenViewModel* viewModel = EnsureProjectCreateScreenViewModel();
	RefreshPresetCardPanel(
		ScenarioPresetCardPanel,
		viewModel ? viewModel->GetScenarioPresetItems() : TArray<FProjectCreatePresetItem>());
	RefreshPresetCardPanel(
		ProfilePresetCardPanel,
		viewModel ? viewModel->GetProfilePresetItems() : TArray<FProjectCreatePresetItem>());
	RefreshPresetCardPanel(
		PolicyPresetCardPanel,
		viewModel ? viewModel->GetPolicyPresetItems() : TArray<FProjectCreatePresetItem>());
}

void UProjectCreateScreenWidget::RefreshPresetCardPanel(
	UPanelWidget* panel,
	const TArray<FProjectCreatePresetItem>& items)
{
	if (!panel)
	{
		return;
	}

	panel->ClearChildren();
	const TSubclassOf<UProjectPresetCardWidget> cardClass = ResolveProjectPresetCardWidgetClass();
	if (!cardClass || !GetWorld())
	{
		return;
	}

	for (int32 itemIndex = 0; itemIndex < items.Num(); ++itemIndex)
	{
		const FProjectCreatePresetItem& item = items[itemIndex];
		UProjectPresetCardWidget* cardWidget = CreateWidget<UProjectPresetCardWidget>(GetWorld(), cardClass);
		if (!cardWidget)
		{
			continue;
		}

		cardWidget->InitializeCard(item);
		cardWidget->OnSelectedRequested.AddUObject(this, &UProjectCreateScreenWidget::HandlePresetCardSelected);
		panel->AddChild(cardWidget);
		ConfigurePresetCardSlot(cardWidget, itemIndex == items.Num() - 1);
		PresetCards.Add(cardWidget);
	}
}

void UProjectCreateScreenWidget::ConfigurePresetCardSlot(UWidget* cardWidget, const bool bLastCard) const
{
	UScrollBoxSlot* scrollSlot = cardWidget ? Cast<UScrollBoxSlot>(cardWidget->Slot) : nullptr;
	if (scrollSlot)
	{
		scrollSlot->SetHorizontalAlignment(PresetCardHorizontalAlignment);
		scrollSlot->SetVerticalAlignment(PresetCardVerticalAlignment);
		const float spacing = FMath::Max(PresetCardSpacing, 0.0f);
		scrollSlot->SetPadding(bLastCard || spacing <= 0.0f ? FMargin() : FMargin(0.0f, 0.0f, spacing, 0.0f));
		return;
	}

	UWrapBoxSlot* wrapSlot = cardWidget ? Cast<UWrapBoxSlot>(cardWidget->Slot) : nullptr;
	if (!wrapSlot)
	{
		return;
	}

	const float spacing = FMath::Max(PresetCardSpacing, 0.0f);
	wrapSlot->SetHorizontalAlignment(PresetCardHorizontalAlignment);
	wrapSlot->SetVerticalAlignment(PresetCardVerticalAlignment);
	wrapSlot->SetPadding(spacing <= 0.0f ? FMargin() : FMargin(0.0f, 0.0f, spacing, spacing));
}

TSubclassOf<UProjectPresetCardWidget> UProjectCreateScreenWidget::ResolveProjectPresetCardWidgetClass() const
{
	return ProjectPresetCardWidgetClass;
}

bool UProjectCreateScreenWidget::BrowseForProjectParentFolder(FString& outFolder) const
{
	outFolder.Reset();

#if WITH_EDITOR
	IDesktopPlatform* desktopPlatform = FDesktopPlatformModule::Get();
	if (!desktopPlatform)
	{
		return false;
	}

	const FString initialFolder = NormalizeProjectCreateWidgetPath(ProjectCreateScreenViewModel
		? ProjectCreateScreenViewModel->GetProjectParentFolder()
		: FPlatformProcess::UserDir());
	const void* parentWindowHandle = FSlateApplication::IsInitialized()
		? FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr)
		: nullptr;
	FString selectedFolder;
	if (!desktopPlatform->OpenDirectoryDialog(
		parentWindowHandle,
		BrowseDialogTitle.ToString(),
		initialFolder,
		selectedFolder))
	{
		return false;
	}

	outFolder = NormalizeProjectCreateWidgetPath(selectedFolder);
	return !outFolder.IsEmpty();
#else
	return false;
#endif
}

void UProjectCreateScreenWidget::HandleProjectNameChanged(UBaseTextInputWidget*, const FText& text)
{
	if (UProjectCreateScreenViewModel* viewModel = EnsureProjectCreateScreenViewModel())
	{
		viewModel->SetProjectName(text.ToString());
	}
	RefreshFromViewModel();
}

void UProjectCreateScreenWidget::HandleProjectParentFolderChanged(UBaseTextInputWidget*, const FText& text)
{
	if (UProjectCreateScreenViewModel* viewModel = EnsureProjectCreateScreenViewModel())
	{
		viewModel->SetProjectParentFolder(text.ToString());
	}
	RefreshFromViewModel();
}

void UProjectCreateScreenWidget::HandleBrowseFolderClicked(UBaseButtonWidget*)
{
	UProjectCreateScreenViewModel* viewModel = EnsureProjectCreateScreenViewModel();
	if (!viewModel)
	{
		return;
	}

	FString selectedFolder;
	if (!BrowseForProjectParentFolder(selectedFolder))
	{
		viewModel->SetDiagnosticsText(DiagnosticMessages.ParentFolderSelectionCanceled);
		RefreshFromViewModel();
		return;
	}

	viewModel->SetProjectParentFolder(selectedFolder);
	viewModel->ClearDiagnostics();
	RefreshFromViewModel();
}

void UProjectCreateScreenWidget::HandleCancelClicked(UBaseButtonWidget*)
{
	OnCancelRequested.Broadcast(this);
}

void UProjectCreateScreenWidget::HandleCreateProjectClicked(UBaseButtonWidget*)
{
	CreateCurrentProject();
}

void UProjectCreateScreenWidget::HandlePresetCardSelected(UProjectPresetCardWidget* cardWidget)
{
	if (!cardWidget)
	{
		return;
	}

	if (UProjectCreateScreenViewModel* viewModel = EnsureProjectCreateScreenViewModel())
	{
		viewModel->SelectPreset(cardWidget->GetPresetCategory(), cardWidget->GetPresetId());
	}
	RefreshFromViewModel();
}
