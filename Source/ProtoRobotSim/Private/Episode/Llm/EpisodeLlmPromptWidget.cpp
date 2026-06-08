
#include "Episode/Llm/EpisodeLlmPromptWidget.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/TextBlock.h"
#include "Episode/Editor/EpisodeEditorController.h"
#include "Episode/EpisodeRunnerSubsystem.h"

void UEpisodeLlmPromptWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BindControls();
}

void UEpisodeLlmPromptWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindLlmSubsystem();
	ConfigureStatusTextBlock();
	RequestEditorWidgetInputMode();
	SetStatusText(TEXT("대기 중."));
}
	
void UEpisodeLlmPromptWidget::NativeDestruct()
{
	ReleaseEditorWidgetInputMode();
	UnbindLlmSubsystem();
	Super::NativeDestruct();
}

bool UEpisodeLlmPromptWidget::GenerateFromPromptTextBox()
{
	UEpisodeLlmAuthoringSubsystem* llmSubsystem = GetLlmAuthoringSubsystem();
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

bool UEpisodeLlmPromptWidget::LoadGeneratedEpisode()
{
	const UEpisodeLlmAuthoringSubsystem* llmSubsystem = GetLlmAuthoringSubsystem();
	if (!llmSubsystem)
	{
		SetStatusText(TEXT("LLM 서버와 연결되어 있지 않습니다."));
		return false;
	}

	const FEpisodeLlmGenerationResult result = llmSubsystem->GetLatestResult();
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

	AEpisodeEditorController* editorController = Cast<AEpisodeEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		SetStatusText(TEXT("소유 플레이어가 EpisodeEditorController가 아닙니다."));
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

bool UEpisodeLlmPromptWidget::RunGeneratedSimulation()
{
	const UEpisodeLlmAuthoringSubsystem* llmSubsystem = GetLlmAuthoringSubsystem();
	if (!llmSubsystem) return false;

	const FEpisodeLlmGenerationResult result = llmSubsystem->GetLatestResult();
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
	UEpisodeRunnerSubsystem* runnerSubsystem = gameInstance
		? gameInstance->GetSubsystem<UEpisodeRunnerSubsystem>()
		: nullptr;
	if (!runnerSubsystem)
	{
		SetStatusText(TEXT("EpisodeRunnerSubsystem을 사용할 수 없습니다."));
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

void UEpisodeLlmPromptWidget::SetStatusText(const FString& message)
{
	if (StatusTextBlock)
	{
		StatusTextBlock->SetText(FText::FromString(message));
	}
}

void UEpisodeLlmPromptWidget::HandleGenerateButtonClicked()
{
	GenerateFromPromptTextBox();
}

void UEpisodeLlmPromptWidget::HandleLoadGeneratedEpisodeButtonClicked()
{
	LoadGeneratedEpisode();
}

void UEpisodeLlmPromptWidget::HandleRunGeneratedSimulationButtonClicked()
{
	RunGeneratedSimulation();
}

void UEpisodeLlmPromptWidget::HandleGenerationCompleted(const FEpisodeLlmGenerationResult& result)
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

void UEpisodeLlmPromptWidget::BindControls()
{
	if (GenerateButton)
	{
		GenerateButton->OnClicked.RemoveDynamic(this, &UEpisodeLlmPromptWidget::HandleGenerateButtonClicked);
		GenerateButton->OnClicked.AddDynamic(this, &UEpisodeLlmPromptWidget::HandleGenerateButtonClicked);
	}

	if (LoadGeneratedEpisodeButton)
	{
		LoadGeneratedEpisodeButton->OnClicked.RemoveDynamic(
			this,
			&UEpisodeLlmPromptWidget::HandleLoadGeneratedEpisodeButtonClicked);
		LoadGeneratedEpisodeButton->OnClicked.AddDynamic(
			this,
			&UEpisodeLlmPromptWidget::HandleLoadGeneratedEpisodeButtonClicked);
	}

	if (RunGeneratedSimulationButton)
	{
		RunGeneratedSimulationButton->OnClicked.RemoveDynamic(
			this,
			&UEpisodeLlmPromptWidget::HandleRunGeneratedSimulationButtonClicked);
		RunGeneratedSimulationButton->OnClicked.AddDynamic(
			this,
			&UEpisodeLlmPromptWidget::HandleRunGeneratedSimulationButtonClicked);
	}
}

void UEpisodeLlmPromptWidget::BindLlmSubsystem()
{
	if (UEpisodeLlmAuthoringSubsystem* llmSubsystem = GetLlmAuthoringSubsystem())
	{
		llmSubsystem->OnGenerationCompleted.RemoveDynamic(
			this,
			&UEpisodeLlmPromptWidget::HandleGenerationCompleted);
		llmSubsystem->OnGenerationCompleted.AddDynamic(
			this,
			&UEpisodeLlmPromptWidget::HandleGenerationCompleted);
	}
}

void UEpisodeLlmPromptWidget::UnbindLlmSubsystem()
{
	if (UEpisodeLlmAuthoringSubsystem* llmSubsystem = GetLlmAuthoringSubsystem())
	{
		llmSubsystem->OnGenerationCompleted.RemoveDynamic(
			this,
			&UEpisodeLlmPromptWidget::HandleGenerationCompleted);
	}
}

void UEpisodeLlmPromptWidget::ConfigureStatusTextBlock()
{
	if (!StatusTextBlock)
	{
		return;
	}

	StatusTextBlock->SetAutoWrapText(true);
	StatusTextBlock->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
}

void UEpisodeLlmPromptWidget::RequestEditorWidgetInputMode()
{
	if (AEpisodeEditorController* editorController = Cast<AEpisodeEditorController>(GetOwningPlayer()))
	{
		UWidget* focusWidget = ResolveInputModeFocusWidget();
		RequestedInputModeFocusWidget = focusWidget;
		editorController->RequestEditorWidgetInputMode(focusWidget);
	}
}

void UEpisodeLlmPromptWidget::ReleaseEditorWidgetInputMode()
{
	if (AEpisodeEditorController* editorController = Cast<AEpisodeEditorController>(GetOwningPlayer()))
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

bool UEpisodeLlmPromptWidget::TryGetPrompt(FString& outPrompt)
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

bool UEpisodeLlmPromptWidget::TryGetEpisodeCount(int32& outEpisodeCount)
{
	outEpisodeCount = 0;
	if (!EpisodeCountTextBox)
	{
		if (const UEpisodeLlmAuthoringSubsystem* llmSubsystem = GetLlmAuthoringSubsystem())
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
		if (const UEpisodeLlmAuthoringSubsystem* llmSubsystem = GetLlmAuthoringSubsystem())
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

UWidget* UEpisodeLlmPromptWidget::ResolveInputModeFocusWidget()
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

UEpisodeLlmAuthoringSubsystem* UEpisodeLlmPromptWidget::GetLlmAuthoringSubsystem() const
{
	UGameInstance* gameInstance = GetGameInstance();
	return gameInstance ? gameInstance->GetSubsystem<UEpisodeLlmAuthoringSubsystem>() : nullptr;
}
