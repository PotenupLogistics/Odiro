#include "Episode/Llm/EpisodeLlmPromptWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Episode/Editor/EpisodeEditorController.h"
#include "Episode/EpisodeRunnerSubsystem.h"
#include "Framework/Text/TextLayout.h"

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
	SetStatusText(TEXT("LLM authoring ready."));
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
		SetStatusText(TEXT("LLM authoring subsystem unavailable."));
		return false;
	}

	FString prompt;
	if (!TryGetPrompt(prompt))
	{
		return false;
	}

	int32 episodeCount = 0;
	if (!TryGetEpisodeCount(episodeCount))
	{
		return false;
	}

	SetStatusText(TEXT("Requesting LLM episode generation..."));
	return llmSubsystem->GenerateEpisodeFromPrompt(prompt, episodeCount);
}

bool UEpisodeLlmPromptWidget::LoadGeneratedEpisode()
{
	const UEpisodeLlmAuthoringSubsystem* llmSubsystem = GetLlmAuthoringSubsystem();
	if (!llmSubsystem)
	{
		SetStatusText(TEXT("LLM authoring subsystem unavailable."));
		return false;
	}

	const FEpisodeLlmGenerationResult result = llmSubsystem->GetLatestResult();
	if (!result.bSuccess)
	{
		SetStatusText(TEXT("No successful LLM generation result is available."));
		return false;
	}

	if (result.FirstEpisodeSetupJsonPath.IsEmpty())
	{
		SetStatusText(TEXT("Generated RunQueue does not contain an EpisodeSetup path."));
		return false;
	}

	AEpisodeEditorController* editorController = Cast<AEpisodeEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		SetStatusText(TEXT("Owning player is not an EpisodeEditorController."));
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
			? FString::Printf(TEXT("Generated EpisodeSetup load failed: %s"), *result.FirstEpisodeSetupJsonPath)
			: FString::Join(diagnostics, TEXT("\n")));
		return false;
	}

	SetStatusText(FString::Printf(TEXT("Loaded generated EpisodeSetup: %s"), *resolvedJsonFilePath));
	return true;
}

bool UEpisodeLlmPromptWidget::RunGeneratedSimulation()
{
	const UEpisodeLlmAuthoringSubsystem* llmSubsystem = GetLlmAuthoringSubsystem();
	if (!llmSubsystem)
	{
		SetStatusText(TEXT("LLM authoring subsystem unavailable."));
		return false;
	}

	const FEpisodeLlmGenerationResult result = llmSubsystem->GetLatestResult();
	if (!result.bSuccess)
	{
		SetStatusText(TEXT("No successful LLM generation result is available."));
		return false;
	}

	if (result.SavedRunQueueJsonPath.IsEmpty())
	{
		SetStatusText(TEXT("Generated RunQueue path is empty."));
		return false;
	}

	UGameInstance* gameInstance = GetGameInstance();
	UEpisodeRunnerSubsystem* runnerSubsystem = gameInstance
		? gameInstance->GetSubsystem<UEpisodeRunnerSubsystem>()
		: nullptr;
	if (!runnerSubsystem)
	{
		SetStatusText(TEXT("EpisodeRunnerSubsystem unavailable."));
		return false;
	}

	if (!runnerSubsystem->StartBatchFromRunQueueJsonFile(result.SavedRunQueueJsonPath))
	{
		SetStatusText(FString::Printf(TEXT("Generated RunQueue start failed: %s"), *result.SavedRunQueueJsonPath));
		return false;
	}

	SetStatusText(FString::Printf(TEXT("Started generated RunQueue: %s"), *result.SavedRunQueueJsonPath));
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
			? result.Message
			: FString::Join(result.Diagnostics, TEXT("\n")));
		return;
	}

	SetStatusText(FString::Printf(
		TEXT("Generated %d run(s). Saved RunQueue: %s"),
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
	if (!PromptTextBox)
	{
		SetStatusText(TEXT("PromptTextBox is not bound."));
		return false;
	}

	outPrompt = PromptTextBox->GetText().ToString().TrimStartAndEnd();
	if (outPrompt.IsEmpty())
	{
		SetStatusText(TEXT("Prompt must not be empty."));
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
		SetStatusText(TEXT("Episode count must be an integer."));
		return false;
	}

	outEpisodeCount = FCString::Atoi(*text);
	if (outEpisodeCount <= 0)
	{
		SetStatusText(TEXT("Episode count must be greater than zero."));
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
