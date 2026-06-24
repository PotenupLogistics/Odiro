#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scenario/Llm/ScenarioLlmAuthoringSubsystem.h"
#include "ScenarioLlmPromptWidget.generated.h"

class UButton;
class UEditableTextBox;
class UMultiLineEditableTextBox;
class UScenarioEditorUiSubsystem;
class UScenarioLlmPromptViewModel;
class UTextBlock;
class UWidget;

UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioLlmPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Binds optional UMG controls once after construction.
	virtual void NativeOnInitialized() override;
	// Attaches generation delegates and input mode while the prompt panel is visible.
	virtual void NativeConstruct() override;
	// Releases delegates and editor input mode owned by this widget.
	virtual void NativeDestruct() override;

	// Loads the generated project scenario into the editor as soon as generation completes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|LLM")
	bool bLoadProjectScenarioAfterGenerate = false;

	// Sends the prompt text with the current <UserProject>/scenario.json context.
	UFUNCTION(BlueprintCallable, Category = "Scenario|LLM")
	bool GenerateFromPromptTextBox();

	// Loads the generated or current <UserProject>/scenario.json into the editor draft.
	UFUNCTION(BlueprintCallable, Category = "Scenario|LLM")
	bool LoadGeneratedScenario();

	// Saves the current draft and launches a project run under <UserProject>/runs/<RunId>.
	UFUNCTION(BlueprintCallable, Category = "Scenario|LLM")
	bool RunGeneratedSimulation();

	// Updates the optional status text binding.
	UFUNCTION(BlueprintCallable, Category = "Scenario|LLM")
	void SetStatusText(const FString& message);

protected:
	// Handles the Generate button click.
	UFUNCTION()
	void HandleGenerateButtonClicked();

	// Handles the Load button click.
	UFUNCTION()
	void HandleLoadGeneratedScenarioButtonClicked();

	// Handles the Run button click.
	UFUNCTION()
	void HandleRunGeneratedSimulationButtonClicked();

	// Handles the generation completion delegate from the authoring subsystem.
	UFUNCTION()
	void HandleGenerationCompleted(const FScenarioLlmGenerationResult& result);

private:
	// Binds optional UMG button delegates.
	void BindControls();
	// Subscribes this widget to generation completion events.
	void BindLlmSubsystem();
	// Removes generation completion event subscriptions owned by this widget.
	void UnbindLlmSubsystem();
	// Applies wrapping behavior to the optional status text block.
	void ConfigureStatusTextBlock();
	// Requests editor UI input focus while the prompt panel is active.
	void RequestEditorWidgetInputMode();
	// Releases the editor UI input focus request made by this widget.
	void ReleaseEditorWidgetInputMode();
	// Reads and validates the prompt text.
	bool TryGetPrompt(FString& outPrompt);
	// Reads and validates the requested project episode count.
	bool TryGetEpisodeCount(int32& outEpisodeCount);
	// Returns the widget that should receive text input focus.
	UWidget* ResolveInputModeFocusWidget();
	// Returns the world-owned scenario editor UI subsystem.
	UScenarioEditorUiSubsystem* GetEditorUiSubsystem() const;
	// Returns the prompt ViewModel owned by the scenario editor UI subsystem.
	UScenarioLlmPromptViewModel* GetLlmPromptViewModel() const;

	// Optional focus widget used when requesting editor input mode.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LlmInputModeFocus;

	// Prompt input field bound by the UMG widget.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UMultiLineEditableTextBox> PromptTextBox;

	// Optional episode count input field bound by the UMG widget.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> ScenarioCountTextBox;

	// Optional generate button bound by the UMG widget.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> GenerateButton;

	// Optional load button bound by the UMG widget.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> LoadGeneratedScenarioButton;

	// Optional run button bound by the UMG widget.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> RunGeneratedSimulationButton;

	// Optional status text block bound by the UMG widget.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusTextBlock;

	// Focus widget currently registered with the editor controller.
	TWeakObjectPtr<UWidget> RequestedInputModeFocusWidget;
};
