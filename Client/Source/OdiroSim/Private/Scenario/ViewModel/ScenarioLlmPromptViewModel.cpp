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
		SetStatusText(TEXT("프롬프트를 입력하세요."));
		return false;
	}
	if (ProjectScenarioJsonPath.TrimStartAndEnd().IsEmpty())
	{
		SetStatusText(TEXT("project scenario.json 경로가 필요합니다."));
		return false;
	}

	if (!UiSubsystem)
	{
		SetStatusText(TEXT("시나리오 에디터 UI 서브시스템을 찾을 수 없습니다."));
		return false;
	}

	FString failureReason;
	if (!UiSubsystem->RequestScenarioGeneration(trimmedPrompt, ProjectScenarioJsonPath, EpisodeCount, failureReason))
	{
		SetStatusText(failureReason.IsEmpty() ? TEXT("시나리오 생성 요청이 거부되었습니다.") : failureReason);
		return false;
	}

	SetStatusText(TEXT("시나리오 생성을 요청했습니다."));
	SetBusy(true);
	return true;
}

bool UScenarioLlmPromptViewModel::RequestGenerationFromInput(const FString& prompt, const int32 count)
{
	if (!UiSubsystem)
	{
		SetStatusText(TEXT("시나리오 에디터 UI 서브시스템을 찾을 수 없습니다."));
		return false;
	}

	FString scenarioJsonPath;
	FString projectPath;
	FString failureReason;
	if (!UiSubsystem->ResolveCurrentProjectScenarioPath(scenarioJsonPath, projectPath, failureReason))
	{
		SetStatusText(failureReason.IsEmpty() ? TEXT("project scenario.json 경로가 필요합니다.") : failureReason);
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
		SetStatusText(TEXT("시나리오 에디터 UI 서브시스템을 찾을 수 없습니다."));
		return false;
	}

	FString statusText;
	const bool bLoaded = UiSubsystem->LoadDemoProjectScenario(statusText);
	SetStatusText(statusText);
	return bLoaded;
}

bool UScenarioLlmPromptViewModel::RunGeneratedSimulation()
{
	if (!UiSubsystem)
	{
		SetStatusText(TEXT("시나리오 에디터 UI 서브시스템을 찾을 수 없습니다."));
		return false;
	}

	FString statusText;
	const bool bStarted = UiSubsystem->RunCurrentProjectScenario(statusText);
	SetStatusText(statusText);
	return bStarted;
}
