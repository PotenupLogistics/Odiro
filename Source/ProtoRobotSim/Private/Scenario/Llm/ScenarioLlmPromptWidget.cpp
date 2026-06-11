
#include "Scenario/Llm/ScenarioLlmPromptWidget.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/TextBlock.h"
#include "Scenario/Editor/ScenarioEditorController.h"
#include "Scenario/ScenarioRunnerSubsystem.h"

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

	int32 episodeCount = 0;
	if (!TryGetEpisodeCount(episodeCount)) return false;

	SetStatusText(TEXT("생성 요청 중."));
	return llmSubsystem->GenerateEpisodeFromPrompt(prompt, episodeCount);
}

bool UScenarioLlmPromptWidget::LoadGeneratedEpisode()
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

	if (result.FirstEpisodeSetupJsonPath.IsEmpty())
	{
		SetStatusText(TEXT("생성된 RunQueue가 EpisodeSetup 경로를 포함하지 않습니다."));
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
	if (!editorController->LoadEpisodeSetupJsonFile(
			result.FirstEpisodeSetupJsonPath,
			resolvedJsonFilePath,
			diagnostics))
	{
		SetStatusText(diagnostics.IsEmpty()
			? FString::Printf(TEXT("생성된 EpisodeSetup 불러오기 실패: %s"), *result.FirstEpisodeSetupJsonPath)
			: FString::Printf(TEXT("EpisodeSetup 불러오기 실패:\n%s"), *FString::Join(diagnostics, TEXT("\n"))));
		return false;
	}

	SetStatusText(FString::Printf(TEXT("EpisodeSetup 불러오기: %s"), *resolvedJsonFilePath));
	return true;
}

bool UScenarioLlmPromptWidget::RunGeneratedSimulation()
{
	const UScenarioLlmAuthoringSubsystem* llmSubsystem = GetLlmAuthoringSubsystem();
	if (!llmSubsystem) return false;

	const FScenarioLlmGenerationResult result = llmSubsystem->GetLatestResult();
	if (!result.bSuccess)
	{
		SetStatusText(TEXT("성공한 LLM 생성 결과가 없습니다."));
		return false;
	}

	if (result.SavedRunQueueJsonPath.IsEmpty())
	{
		SetStatusText(TEXT("생성된 RunQueue 경로가 비어 있습니다."));
		return false;
	}

	UGameInstance* gameInstance = GetGameInstance();
	UScenarioRunnerSubsystem* runnerSubsystem = gameInstance
		? gameInstance->GetSubsystem<UScenarioRunnerSubsystem>()
		: nullptr;
	if (!runnerSubsystem)
	{
		SetStatusText(TEXT("ScenarioRunnerSubsystem을 사용할 수 없습니다."));
		return false;
	}

	if (!runnerSubsystem->StartBatchFromRunQueueJsonFile(result.SavedRunQueueJsonPath))
	{
		SetStatusText(FString::Printf(TEXT("생성된 RunQueue 실행 실패: %s"), *result.SavedRunQueueJsonPath));
		return false;
	}

	SetStatusText(FString::Printf(TEXT("생성된 RunQueue 실행 시작: %s"), *result.SavedRunQueueJsonPath));
	return true;
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

void UScenarioLlmPromptWidget::HandleLoadGeneratedEpisodeButtonClicked()
{
	LoadGeneratedEpisode();
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
		TEXT("%d개의 실행을 생성했습니다. 저장된 RunQueue: %s"),
		result.RunCount,
		*result.SavedRunQueueJsonPath));

	if (bLoadFirstEpisodeAfterGenerate)
	{
		LoadGeneratedEpisode();
	}
}

void UScenarioLlmPromptWidget::BindControls()
{
	if (GenerateButton)
	{
		GenerateButton->OnClicked.RemoveDynamic(this, &UScenarioLlmPromptWidget::HandleGenerateButtonClicked);
		GenerateButton->OnClicked.AddDynamic(this, &UScenarioLlmPromptWidget::HandleGenerateButtonClicked);
	}

	if (LoadGeneratedEpisodeButton)
	{
		LoadGeneratedEpisodeButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioLlmPromptWidget::HandleLoadGeneratedEpisodeButtonClicked);
		LoadGeneratedEpisodeButton->OnClicked.AddDynamic(
			this,
			&UScenarioLlmPromptWidget::HandleLoadGeneratedEpisodeButtonClicked);
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

bool UScenarioLlmPromptWidget::TryGetEpisodeCount(int32& outEpisodeCount)
{
	outEpisodeCount = 0;
	if (!EpisodeCountTextBox)
	{
		if (const UScenarioLlmAuthoringSubsystem* llmSubsystem = GetLlmAuthoringSubsystem())
		{
			outEpisodeCount = llmSubsystem->DefaultEpisodeCount;
			return true;
		}

		outEpisodeCount = 1;
		return true;
	}

	const FString text = EpisodeCountTextBox->GetText().ToString().TrimStartAndEnd();
	if (text.IsEmpty())
	{
		if (const UScenarioLlmAuthoringSubsystem* llmSubsystem = GetLlmAuthoringSubsystem())
		{
			outEpisodeCount = llmSubsystem->DefaultEpisodeCount;
			return true;
		}

		outEpisodeCount = 1;
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

UScenarioLlmAuthoringSubsystem* UScenarioLlmPromptWidget::GetLlmAuthoringSubsystem() const
{
	UGameInstance* gameInstance = GetGameInstance();
	return gameInstance ? gameInstance->GetSubsystem<UScenarioLlmAuthoringSubsystem>() : nullptr;
}
