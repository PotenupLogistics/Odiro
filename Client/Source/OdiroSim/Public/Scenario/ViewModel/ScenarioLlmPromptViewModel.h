#pragma once

#include "CoreMinimal.h"
#include "Platform/ViewModel/OdiroViewModelBase.h"
#include "ScenarioLlmPromptViewModel.generated.h"

class UScenarioEditorUiSubsystem;

// Scenario LLM prompt panel의 입력/상태와 generation command facade.
UCLASS(BlueprintType)
class ODIROSIM_API UScenarioLlmPromptViewModel : public UOdiroViewModelBase
{
	GENERATED_BODY()

public:
	// World subsystem 소유 관계를 연결한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|LlmPrompt")
	void InitializeForSubsystem(UScenarioEditorUiSubsystem* uiSubsystem);

	// Prompt 입력 문자열을 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|LlmPrompt")
	void SetPromptText(const FString& prompt);

	// 대상 project scenario.json 경로를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|LlmPrompt")
	void SetProjectScenarioJsonPath(const FString& scenarioJsonPath);

	// 요청할 episode count를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|LlmPrompt")
	void SetEpisodeCount(int32 count);

	// 상태 표시 문자열을 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|LlmPrompt")
	void SetStatusText(const FString& message);

	// LLM generation 요청을 authoring subsystem에 위임한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|LlmPrompt")
	bool RequestGeneration();

	// 입력 prompt와 episode count로 현재 project scenario.json 대상 generation을 요청한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|LlmPrompt")
	bool RequestGenerationFromInput(const FString& prompt, int32 count);

	// 최신 generation 결과 또는 현재 project scenario.json을 editor draft로 읽는다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|LlmPrompt")
	bool LoadGeneratedScenario();

	// 현재 editor draft를 저장하고 project run을 시작한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|LlmPrompt")
	bool RunGeneratedSimulation();

	// Prompt 입력 문자열을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|LlmPrompt")
	FString GetPromptText() const { return PromptText; }

	// 대상 project scenario.json 경로를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|LlmPrompt")
	FString GetProjectScenarioJsonPath() const { return ProjectScenarioJsonPath; }

	// 요청 episode count를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|LlmPrompt")
	int32 GetEpisodeCount() const { return EpisodeCount; }

	// 상태 표시 문자열을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|LlmPrompt")
	FString GetStatusText() const { return StatusText; }

private:
	// Subsystem을 통해 GameInstance/LLM subsystem에 접근한다.
	UPROPERTY(Transient)
	TObjectPtr<UScenarioEditorUiSubsystem> UiSubsystem;

	// 사용자 natural-language prompt.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|LlmPrompt", meta = (AllowPrivateAccess = "true"))
	FString PromptText;

	// Generation 결과를 저장할 project scenario.json 경로.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|LlmPrompt", meta = (AllowPrivateAccess = "true"))
	FString ProjectScenarioJsonPath;

	// Generation request episode count.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|LlmPrompt", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 EpisodeCount = 1;

	// Panel status 표시 문자열.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|LlmPrompt", meta = (AllowPrivateAccess = "true"))
	FString StatusText = TEXT("Ready");
};
