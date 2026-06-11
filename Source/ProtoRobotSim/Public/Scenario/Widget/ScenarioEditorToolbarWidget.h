#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ScenarioEditorToolbarWidget.generated.h"

class UButton;
class UTextBlock;
class UWidget;

// EpisodeEditorMap에서 저장과 MainMenu 복귀를 제공하는 최소 toolbar.
UCLASS(BlueprintType, Blueprintable)
class PROTOROBOTSIM_API UScenarioEditorToolbarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Toolbar")
	FString DefaultSavePath = TEXT("Json/Input/EpisodeSetupNew.json");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Toolbar")
	FString MainMenuMapId = TEXT("MainMenuMap");

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Toolbar")
	bool SaveEpisode();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Toolbar")
	void ReturnToMainMenu();

protected:
	UFUNCTION()
	void HandleSaveButtonClicked();

	UFUNCTION()
	void HandleReturnButtonClicked();

private:
	void BindControls();
	void RequestEditorWidgetInputMode();
	void ReleaseEditorWidgetInputMode();
	void SetStatusText(const FString& message);
	UWidget* ResolveInputModeFocusWidget() const;
	FString ResolveSavePath() const;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UWidget> ToolbarInputModeFocus;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> SaveButton;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> ReturnToMainMenuButton;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusTextBlock;

	TWeakObjectPtr<UWidget> RequestedInputModeFocusWidget;
};
