#include "Scenario/ViewModel/ScenarioLlmPromptViewModel.h"

#include "Scenario/ScenarioEditorUiSubsystem.h"

void UScenarioLlmPromptViewModel::InitializeForSubsystem(UScenarioEditorUiSubsystem* uiSubsystem)
{
	UiSubsystem = uiSubsystem;
}

void UScenarioLlmPromptViewModel::SetPromptText(const FString& prompt)
{
	UE_MVVM_SET_PROPERTY_VALUE(PromptText, prompt);
}

void UScenarioLlmPromptViewModel::SetProjectScenarioJsonPath(const FString& scenarioJsonPath)
{
	UE_MVVM_SET_PROPERTY_VALUE(ProjectScenarioJsonPath, scenarioJsonPath);
}

void UScenarioLlmPromptViewModel::SetEpisodeCount(const int32 count)
{
	UE_MVVM_SET_PROPERTY_VALUE(EpisodeCount, FMath::Max(1, count));
}

void UScenarioLlmPromptViewModel::SetStatusText(const FString& message)
{
	UE_MVVM_SET_PROPERTY_VALUE(StatusText, message);
}

bool UScenarioLlmPromptViewModel::RequestGeneration()
{
	const FString trimmedPrompt = PromptText.TrimStartAndEnd();
	if (trimmedPrompt.IsEmpty())
	{
		SetStatusText(TEXT("Prompt is required."));
		return false;
	}
	if (ProjectScenarioJsonPath.TrimStartAndEnd().IsEmpty())
	{
		SetStatusText(TEXT("Project scenario.json path is required."));
		return false;
	}

	if (!UiSubsystem)
	{
		SetStatusText(TEXT("ScenarioEditorUiSubsystem unavailable."));
		return false;
	}

	FString failureReason;
	if (!UiSubsystem->RequestScenarioGeneration(trimmedPrompt, ProjectScenarioJsonPath, EpisodeCount, failureReason))
	{
		SetStatusText(failureReason.IsEmpty() ? TEXT("Scenario generation request rejected.") : failureReason);
		return false;
	}

	SetStatusText(TEXT("Scenario generation requested."));
	SetBusy(true);
	return true;
}

bool UScenarioLlmPromptViewModel::RequestGenerationFromInput(const FString& prompt, const int32 count)
{
	if (!UiSubsystem)
	{
		SetStatusText(TEXT("ScenarioEditorUiSubsystem unavailable."));
		return false;
	}

	FString scenarioJsonPath;
	FString projectPath;
	FString failureReason;
	if (!UiSubsystem->ResolveCurrentProjectScenarioPath(scenarioJsonPath, projectPath, failureReason))
	{
		SetStatusText(failureReason.IsEmpty() ? TEXT("Project scenario.json path is required.") : failureReason);
		return false;
	}

	SetPromptText(prompt);
	SetProjectScenarioJsonPath(scenarioJsonPath);
	SetEpisodeCount(count);
	return RequestGeneration();
}

bool UScenarioLlmPromptViewModel::LoadGeneratedScenario()
{
	if (!UiSubsystem)
	{
		SetStatusText(TEXT("ScenarioEditorUiSubsystem unavailable."));
		return false;
	}

	FString statusText;
	const bool bLoaded = UiSubsystem->LoadLatestGeneratedProjectScenario(statusText);
	SetStatusText(statusText);
	return bLoaded;
}

bool UScenarioLlmPromptViewModel::RunGeneratedSimulation()
{
	if (!UiSubsystem)
	{
		SetStatusText(TEXT("ScenarioEditorUiSubsystem unavailable."));
		return false;
	}

	FString statusText;
	const bool bStarted = UiSubsystem->RunCurrentProjectScenario(statusText);
	SetStatusText(statusText);
	return bStarted;
}
