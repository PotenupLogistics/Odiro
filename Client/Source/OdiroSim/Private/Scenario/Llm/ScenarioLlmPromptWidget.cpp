
#include "Scenario/Llm/ScenarioLlmPromptWidget.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/TextBlock.h"
#include "Scenario/Editor/ScenarioEditorController.h"

void UScenarioLlmPromptWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BindControls();
}

void UScenarioLlmPromptWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindLlmSubsystem();
	ConfigureStatusTextBlock();
	RequestEditorWidgetInputMode();
	SetStatusText(TEXT("대기 중."));
}
	
void UScenarioLlmPromptWidget::NativeDestruct()
{
	ReleaseEditorWidgetInputMode();
	UnbindLlmSubsystem();
	Super::NativeDestruct();
}

bool UScenarioLlmPromptWidget::GenerateFromPromptTextBox()
{
	UScenarioLlmAuthoringSubsystem* llmSubsystem = GetLlmAuthoringSubsystem();
	if (!llmSubsystem)
	{
		SetStatusText(TEXT("LLM 서버와 연결되어 있지 않습니다."));
		return false;
	}

	FString prompt;
	if (!TryGetPrompt(prompt)) return false;

	FString projectPath;
	if (!TryGetProjectPath(projectPath)) return false;
	llmSubsystem->SetTargetProjectPath(projectPath);

	SetStatusText(TEXT("생성 요청 중."));
	return llmSubsystem->GenerateScenariosFromPrompt(prompt, 1);
}

bool UScenarioLlmPromptWidget::LoadGeneratedScenario()
{
	const UScenarioLlmAuthoringSubsystem* llmSubsystem = GetLlmAuthoringSubsystem();
	if (!llmSubsystem)
	{
		SetStatusText(TEXT("LLM 서버와 연결되어 있지 않습니다."));
		return false;
	}

	const FScenarioLlmGenerationResult result = llmSubsystem->GetLatestResult();
	if (!result.bSuccess)
	{
		SetStatusText(TEXT("이용 가능한 LLM 생성 결과가 없습니다."));
		return false;
	}

	if (result.SavedScenarioJsonPath.IsEmpty())
	{
		SetStatusText(TEXT("저장된 scenario.json 경로가 없습니다."));
		return false;
	}

	SetStatusText(FString::Printf(
		TEXT("scenario.json 저장 완료: %s"),
		*result.SavedScenarioJsonPath));
	return true;
}

bool UScenarioLlmPromptWidget::RunGeneratedSimulation()
{
	SetStatusText(TEXT("RunQueue 실행은 제거되었습니다. MainMenu에서 user project run을 시작하세요."));
	return false;
}

void UScenarioLlmPromptWidget::SetStatusText(const FString& message)
{
	if (StatusTextBlock)
	{
		StatusTextBlock->SetText(FText::FromString(message));
	}
}

void UScenarioLlmPromptWidget::HandleGenerateButtonClicked()
{
	GenerateFromPromptTextBox();
}

void UScenarioLlmPromptWidget::HandleLoadGeneratedScenarioButtonClicked()
{
	LoadGeneratedScenario();
}

void UScenarioLlmPromptWidget::HandleRunGeneratedSimulationButtonClicked()
{
	RunGeneratedSimulation();
}

void UScenarioLlmPromptWidget::HandleGenerationCompleted(const FScenarioLlmGenerationResult& result)
{
	if (!result.bSuccess)
	{
		SetStatusText(result.Diagnostics.IsEmpty()
			? FString::Printf(TEXT("LLM 생성 실패: %s"), *result.Message)
			: FString::Printf(TEXT("LLM 생성 실패:\n%s"), *FString::Join(result.Diagnostics, TEXT("\n"))));
		return;
	}

	SetStatusText(FString::Printf(
		TEXT("scenario 생성 완료: %s"),
		*result.SavedScenarioJsonPath));

	if (bLoadFirstScenarioAfterGenerate)
	{
		LoadGeneratedScenario();
	}
}

void UScenarioLlmPromptWidget::BindControls()
{
	if (GenerateButton)
	{
		GenerateButton->OnClicked.RemoveDynamic(this, &UScenarioLlmPromptWidget::HandleGenerateButtonClicked);
		GenerateButton->OnClicked.AddDynamic(this, &UScenarioLlmPromptWidget::HandleGenerateButtonClicked);
	}

	if (LoadGeneratedScenarioButton)
	{
		LoadGeneratedScenarioButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioLlmPromptWidget::HandleLoadGeneratedScenarioButtonClicked);
		LoadGeneratedScenarioButton->OnClicked.AddDynamic(
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
	if (UScenarioLlmAuthoringSubsystem* llmSubsystem = GetLlmAuthoringSubsystem())
	{
		llmSubsystem->OnGenerationCompleted.RemoveDynamic(
			this,
			&UScenarioLlmPromptWidget::HandleGenerationCompleted);
		llmSubsystem->OnGenerationCompleted.AddDynamic(
			this,
			&UScenarioLlmPromptWidget::HandleGenerationCompleted);
	}
}

void UScenarioLlmPromptWidget::UnbindLlmSubsystem()
{
	if (UScenarioLlmAuthoringSubsystem* llmSubsystem = GetLlmAuthoringSubsystem())
	{
		llmSubsystem->OnGenerationCompleted.RemoveDynamic(
			this,
			&UScenarioLlmPromptWidget::HandleGenerationCompleted);
	}
}

void UScenarioLlmPromptWidget::ConfigureStatusTextBlock()
{
	if (!StatusTextBlock)
	{
		return;
	}

	StatusTextBlock->SetAutoWrapText(true);
	StatusTextBlock->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
}

void UScenarioLlmPromptWidget::RequestEditorWidgetInputMode()
{
	if (AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer()))
	{
		UWidget* focusWidget = ResolveInputModeFocusWidget();
		RequestedInputModeFocusWidget = focusWidget;
		editorController->RequestEditorWidgetInputMode(focusWidget);
	}
}

void UScenarioLlmPromptWidget::ReleaseEditorWidgetInputMode()
{
	if (AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer()))
	{
		UWidget* focusWidget = RequestedInputModeFocusWidget.Get();
		if (!focusWidget)
		{
			focusWidget = ResolveInputModeFocusWidget();
		}
		editorController->ReleaseEditorWidgetInputMode(focusWidget);
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

bool UScenarioLlmPromptWidget::TryGetProjectPath(FString& outProjectPath)
{
	outProjectPath.Reset();
	const UScenarioLlmAuthoringSubsystem* llmSubsystem = GetLlmAuthoringSubsystem();
	if (!ProjectPathTextBox)
	{
		if (llmSubsystem)
		{
			outProjectPath = llmSubsystem->GetResolvedTargetProjectPath().TrimStartAndEnd();
		}
	}
	else
	{
		outProjectPath = ProjectPathTextBox->GetText().ToString().TrimStartAndEnd();
		if (outProjectPath.IsEmpty() && llmSubsystem)
		{
			outProjectPath = llmSubsystem->GetResolvedTargetProjectPath().TrimStartAndEnd();
		}
	}

	if (outProjectPath.IsEmpty())
	{
		SetStatusText(TEXT("User project root를 입력해야 scenario.json을 저장할 수 있습니다."));
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

UScenarioLlmAuthoringSubsystem* UScenarioLlmPromptWidget::GetLlmAuthoringSubsystem() const
{
	UGameInstance* gameInstance = GetGameInstance();
	return gameInstance ? gameInstance->GetSubsystem<UScenarioLlmAuthoringSubsystem>() : nullptr;
}
