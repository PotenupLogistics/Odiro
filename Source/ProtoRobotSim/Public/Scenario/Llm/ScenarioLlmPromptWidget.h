#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scenario/Llm/ScenarioLlmAuthoringSubsystem.h"
#include "ScenarioLlmPromptWidget.generated.h"

class UButton;
class UEditableTextBox;
class UMultiLineEditableTextBox;
class UTextBlock;
class UWidget;

UCLASS(BlueprintType, Blueprintable)
class PROTOROBOTSIM_API UScenarioLlmPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|LLM")
	bool bLoadFirstEpisodeAfterGenerate = false;

	UFUNCTION(BlueprintCallable, Category = "Scenario|LLM")
	bool GenerateFromPromptTextBox();

	UFUNCTION(BlueprintCallable, Category = "Scenario|LLM")
	bool LoadGeneratedEpisode();

	UFUNCTION(BlueprintCallable, Category = "Scenario|LLM")
	bool RunGeneratedSimulation();

	UFUNCTION(BlueprintCallable, Category = "Scenario|LLM")
	void SetStatusText(const FString& message);

protected:
	UFUNCTION()
	void HandleGenerateButtonClicked();

	UFUNCTION()
	void HandleLoadGeneratedEpisodeButtonClicked();

	UFUNCTION()
	void HandleRunGeneratedSimulationButtonClicked();

	UFUNCTION()
	void HandleGenerationCompleted(const FScenarioLlmGenerationResult& result);

private:
	void BindControls();
	void BindLlmSubsystem();
	void UnbindLlmSubsystem();
	void ConfigureStatusTextBlock();
	void RequestEditorWidgetInputMode();
	void ReleaseEditorWidgetInputMode();
	bool TryGetPrompt(FString& outPrompt);
	bool TryGetEpisodeCount(int32& outEpisodeCount);
	UWidget* ResolveInputModeFocusWidget();
	UScenarioLlmAuthoringSubsystem* GetLlmAuthoringSubsystem() const;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LlmInputModeFocus;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UMultiLineEditableTextBox> PromptTextBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> EpisodeCountTextBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> GenerateButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> LoadGeneratedEpisodeButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> RunGeneratedSimulationButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusTextBlock;

	TWeakObjectPtr<UWidget> RequestedInputModeFocusWidget;
};
