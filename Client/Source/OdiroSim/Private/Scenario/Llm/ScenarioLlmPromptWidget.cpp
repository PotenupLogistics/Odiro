
#include "Scenario/Llm/ScenarioLlmPromptWidget.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/TextBlock.h"
#include "Scenario/Editor/ScenarioEditorController.h"
#include "Widget/WidgetTextStyleCatalog.h"

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
	UWidgetTextStyleCatalog::ApplyMultiLineEditableTextBoxStyle(PromptTextBox.Get(), EWidgetTextStyleRole::Value);
	UWidgetTextStyleCatalog::ApplyEditableTextBoxStyle(ScenarioCountTextBox.Get(), EWidgetTextStyleRole::Value);
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(StatusTextBlock.Get(), EWidgetTextStyleRole::Value);
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

	int32 scenarioCount = 0;
	if (!TryGetScenarioCount(scenarioCount)) return false;

	SetStatusText(TEXT("생성 요청 중."));
	return llmSubsystem->GenerateScenariosFromPrompt(prompt, scenarioCount);
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

	if (result.FirstScenarioSourceJsonPath.IsEmpty())
	{
		SetStatusText(TEXT("생성된 시나리오 경로가 없습니다."));
		return false;
	}

	AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		SetStatusText(TEXT("소유 플레이어가 ScenarioEditorController가 아닙니다."));
		return false;
	}

	FString resolvedJsonFilePath;
	TArray<FString> diagnostics;
	if (!editorController->LoadScenarioSetupJsonFile(
			result.FirstScenarioSourceJsonPath,
			resolvedJsonFilePath,
			diagnostics))
	{
		SetStatusText(diagnostics.IsEmpty()
			? FString::Printf(TEXT("생성된 ScenarioSetup 불러오기 실패: %s"), *result.FirstScenarioSourceJsonPath)
			: FString::Printf(TEXT("ScenarioSetup 불러오기 실패:\n%s"), *FString::Join(diagnostics, TEXT("\n"))));
		return false;
	}

	SetStatusText(FString::Printf(TEXT("ScenarioSetup 불러오기: %s"), *resolvedJsonFilePath));
	return true;
}

bool UScenarioLlmPromptWidget::RunGeneratedSimulation()
{
	SetStatusText(TEXT("LLM 생성 결과 즉시 실행은 experiment folder 실행 흐름으로 교체 예정입니다."));
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
		TEXT("LLM 생성 완료: %s"),
		*result.Message));

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

bool UScenarioLlmPromptWidget::TryGetScenarioCount(int32& outScenarioCount)
{
	outScenarioCount = 0;
	if (!ScenarioCountTextBox)
	{
		if (const UScenarioLlmAuthoringSubsystem* llmSubsystem = GetLlmAuthoringSubsystem())
		{
			outScenarioCount = llmSubsystem->DefaultScenarioCount;
			return true;
		}

		outScenarioCount = 1;
		return true;
	}

	const FString text = ScenarioCountTextBox->GetText().ToString().TrimStartAndEnd();
	if (text.IsEmpty())
	{
		if (const UScenarioLlmAuthoringSubsystem* llmSubsystem = GetLlmAuthoringSubsystem())
		{
			outScenarioCount = llmSubsystem->DefaultScenarioCount;
			return true;
		}

		outScenarioCount = 1;
		return true;
	}

	if (!text.IsNumeric())
	{
		SetStatusText(TEXT("생성 횟수는 정수여야 합니다."));
		return false;
	}

	outScenarioCount = FCString::Atoi(*text);
	if (outScenarioCount <= 0)
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

UScenarioLlmAuthoringSubsystem* UScenarioLlmPromptWidget::GetLlmAuthoringSubsystem() const
{
	UGameInstance* gameInstance = GetGameInstance();
	return gameInstance ? gameInstance->GetSubsystem<UScenarioLlmAuthoringSubsystem>() : nullptr;
}
