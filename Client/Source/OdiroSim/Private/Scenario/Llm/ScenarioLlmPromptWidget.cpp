
#include "Scenario/Llm/ScenarioLlmPromptWidget.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/TextBlock.h"
#include "Scenario/ScenarioEditorUiSubsystem.h"
#include "Scenario/ViewModel/ScenarioLlmPromptViewModel.h"
#include "UI/BaseButtonWidget.h"

void UScenarioLlmPromptWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BindControls();
}

void UScenarioLlmPromptWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindLlmSubsystem();
	RequestEditorWidgetInputMode();
	SetStatusText(TEXT("프롬프트를 입력하세요."));
}
	
void UScenarioLlmPromptWidget::NativeDestruct()
{
	ReleaseEditorWidgetInputMode();
	UnbindLlmSubsystem();
	Super::NativeDestruct();
}

bool UScenarioLlmPromptWidget::GenerateFromPromptTextBox()
{
	UScenarioLlmPromptViewModel* viewModel = GetLlmPromptViewModel();
	if (!viewModel)
	{
		SetStatusText(TEXT("Scenario LLM ViewModel is unavailable."));
		return false;
	}

	FString prompt;
	if (!TryGetPrompt(prompt)) return false;

	int32 episodeCount = 0;
	if (!TryGetEpisodeCount(episodeCount)) return false;

	const bool bRequested = viewModel->RequestGenerationFromInput(prompt, episodeCount);
	SetStatusText(viewModel->GetStatusText());
	return bRequested;
}

bool UScenarioLlmPromptWidget::LoadGeneratedScenario()
{
	UScenarioLlmPromptViewModel* viewModel = GetLlmPromptViewModel();
	if (!viewModel)
	{
		SetStatusText(TEXT("Scenario LLM ViewModel is unavailable."));
		return false;
	}

	const bool bLoaded = viewModel->LoadGeneratedScenario();
	SetStatusText(viewModel->GetStatusText());
	return bLoaded;
}

bool UScenarioLlmPromptWidget::RunGeneratedSimulation()
{
	UScenarioLlmPromptViewModel* viewModel = GetLlmPromptViewModel();
	if (!viewModel)
	{
		SetStatusText(TEXT("Scenario LLM ViewModel is unavailable."));
		return false;
	}

	const bool bStarted = viewModel->RunGeneratedSimulation();
	SetStatusText(viewModel->GetStatusText());
	return bStarted;
}

void UScenarioLlmPromptWidget::SetStatusText(const FString& message)
{
	if (UScenarioLlmPromptViewModel* viewModel = GetLlmPromptViewModel())
	{
		viewModel->SetStatusText(message);
	}

	if (StatusTextBlock)
	{
		StatusTextBlock->SetText(FText::FromString(message));
	}
}

void UScenarioLlmPromptWidget::HandleGenerateButtonClicked(UBaseButtonWidget*)
{
	GenerateFromPromptTextBox();
}

void UScenarioLlmPromptWidget::HandleLoadGeneratedScenarioButtonClicked(UBaseButtonWidget*)
{
	LoadGeneratedScenario();
}

void UScenarioLlmPromptWidget::HandleRunGeneratedSimulationButtonClicked()
{
	RunGeneratedSimulation();
}

void UScenarioLlmPromptWidget::HandleGenerationCompleted(const FScenarioLlmGenerationResult& result)
{
	if (UScenarioLlmPromptViewModel* viewModel = GetLlmPromptViewModel())
	{
		viewModel->SetBusy(false);
	}

	if (!result.bSuccess)
	{
		SetStatusText(result.Diagnostics.IsEmpty()
			? FString::Printf(TEXT("시나리오 생성에 실패했습니다: %s"), *result.Message)
			: FString::Printf(TEXT("시나리오 생성에 실패했습니다:\n%s"), *FString::Join(result.Diagnostics, TEXT("\n"))));
		return;
	}

	SetStatusText(TEXT("시나리오 생성이 완료되었습니다."));

	if (bLoadProjectScenarioAfterGenerate)
	{
		LoadGeneratedScenario();
	}
}

void UScenarioLlmPromptWidget::BindControls()
{
	if (GenerateButton)
	{
		GenerateButton->OnBaseClicked.RemoveDynamic(this, &UScenarioLlmPromptWidget::HandleGenerateButtonClicked);
		GenerateButton->OnBaseClicked.AddDynamic(this, &UScenarioLlmPromptWidget::HandleGenerateButtonClicked);
	}

	if (LoadGeneratedScenarioButton)
	{
		LoadGeneratedScenarioButton->OnBaseClicked.RemoveDynamic(
			this,
			&UScenarioLlmPromptWidget::HandleLoadGeneratedScenarioButtonClicked);
		LoadGeneratedScenarioButton->OnBaseClicked.AddDynamic(
			this,
			&UScenarioLlmPromptWidget::HandleLoadGeneratedScenarioButtonClicked);
	}

	if (RunGeneratedSimulationButton)
	{
		RunGeneratedSimulationButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioLlmPromptWidget::HandleRunGeneratedSimulationButtonClicked);
		RunGeneratedSimulationButton->OnClicked.AddDynamic(
			this,
			&UScenarioLlmPromptWidget::HandleRunGeneratedSimulationButtonClicked);
	}
}

void UScenarioLlmPromptWidget::BindLlmSubsystem()
{
	if (UScenarioEditorUiSubsystem* uiSubsystem = GetEditorUiSubsystem())
	{
		uiSubsystem->BindScenarioGenerationCompleted(
			this,
			GET_FUNCTION_NAME_CHECKED(UScenarioLlmPromptWidget, HandleGenerationCompleted));
	}
}

void UScenarioLlmPromptWidget::UnbindLlmSubsystem()
{
	if (UScenarioEditorUiSubsystem* uiSubsystem = GetEditorUiSubsystem())
	{
		uiSubsystem->UnbindScenarioGenerationCompleted(this);
	}
}

void UScenarioLlmPromptWidget::RequestEditorWidgetInputMode()
{
	if (UScenarioEditorUiSubsystem* uiSubsystem = GetEditorUiSubsystem())
	{
		UWidget* focusWidget = ResolveInputModeFocusWidget();
		RequestedInputModeFocusWidget = focusWidget;
		uiSubsystem->RequestEditorWidgetInputMode(focusWidget);
	}
}

void UScenarioLlmPromptWidget::ReleaseEditorWidgetInputMode()
{
	if (UScenarioEditorUiSubsystem* uiSubsystem = GetEditorUiSubsystem())
	{
		UWidget* focusWidget = RequestedInputModeFocusWidget.Get();
		if (!focusWidget)
		{
			focusWidget = ResolveInputModeFocusWidget();
		}
		uiSubsystem->ReleaseEditorWidgetInputMode(focusWidget);
		RequestedInputModeFocusWidget.Reset();
	}
}

bool UScenarioLlmPromptWidget::TryGetPrompt(FString& outPrompt)
{
	outPrompt.Reset();
	if (!PromptTextBox) return false;

	outPrompt = PromptTextBox->GetText().ToString().TrimStartAndEnd();
	if (outPrompt.IsEmpty())
	{
		SetStatusText(TEXT("프롬프트를 입력하세요."));
		return false;
	}

	return true;
}

bool UScenarioLlmPromptWidget::TryGetEpisodeCount(int32& outEpisodeCount)
{
	outEpisodeCount = 0;
	if (!ScenarioCountTextBox)
	{
		const UScenarioEditorUiSubsystem* uiSubsystem = GetEditorUiSubsystem();
		outEpisodeCount = uiSubsystem ? uiSubsystem->GetDefaultScenarioGenerationEpisodeCount() : 1;
		return true;
	}

	const FString text = ScenarioCountTextBox->GetText().ToString().TrimStartAndEnd();
	if (text.IsEmpty())
	{
		const UScenarioEditorUiSubsystem* uiSubsystem = GetEditorUiSubsystem();
		outEpisodeCount = uiSubsystem ? uiSubsystem->GetDefaultScenarioGenerationEpisodeCount() : 1;
		return true;
	}

	if (!text.IsNumeric())
	{
		SetStatusText(TEXT("생성 횟수는 정수여야 합니다."));
		return false;
	}

	outEpisodeCount = FCString::Atoi(*text);
	if (outEpisodeCount <= 0)
	{
		SetStatusText(TEXT("생성 횟수는 1 이상이어야 합니다."));
		return false;
	}

	return true;
}

UWidget* UScenarioLlmPromptWidget::ResolveInputModeFocusWidget()
{
	if (LlmInputModeFocus)
	{
		return LlmInputModeFocus;
	}

	if (PromptTextBox)
	{
		return PromptTextBox;
	}

	return this;
}

UScenarioEditorUiSubsystem* UScenarioLlmPromptWidget::GetEditorUiSubsystem() const
{
	return UScenarioEditorUiSubsystem::ResolveForWorldContext(this);
}

UScenarioLlmPromptViewModel* UScenarioLlmPromptWidget::GetLlmPromptViewModel() const
{
	const UScenarioEditorUiSubsystem* uiSubsystem = GetEditorUiSubsystem();
	return uiSubsystem ? uiSubsystem->GetLlmPromptViewModel() : nullptr;
}
