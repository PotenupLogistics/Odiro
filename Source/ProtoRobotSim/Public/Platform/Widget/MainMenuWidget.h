#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "Types/SlateEnums.h"
#include "MainMenuWidget.generated.h"

class UButton;
class UComboBoxString;
class UEditableTextBox;
class USimulatorLaunchSubsystem;
class UScrollBox;
class UTextBlock;
class UVerticalBox;

// MainMenuMap에서 SimulationSetup 선택과 run status 확인을 제공하는 최소 C++ widget
UCLASS(BlueprintType, Blueprintable)
class PROTOROBOTSIM_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "MainMenu|Simulation")
	void RefreshSetupOptions();

	UFUNCTION(BlueprintCallable, Category = "MainMenu|Simulation")
	void RefreshFromSubsystem();

protected:
	UFUNCTION()
	void HandleSetupSelectionChanged(FString selectedItem, ESelectInfo::Type selectionType);

	UFUNCTION()
	void HandleLoadClicked();

	UFUNCTION()
	void HandleSaveFpsClicked();

	UFUNCTION()
	void HandleStartClicked();

	UFUNCTION()
	void HandleRefreshClicked();

private:
	void BuildWidgetTreeIfNeeded();
	void BindControls();
	void LoadSelectedSetup();
	void HandleRunInfoChanged(const struct FSimulatorRunInfo& runInfo);
	void UpdateStatusText(const FString& extraMessage = FString());
	void UpdateReportAndLogText();
	void SetDiagnosticsText(const FString& message);
	FString GetSelectedSetupPath() const;
	USimulatorLaunchSubsystem* GetSimulatorLaunchSubsystem() const;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> RootBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> SetupComboBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> SetupPathTextBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> RunIdTextBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> FixedStepFpsTextBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> LoadButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> SaveFpsButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> StartButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> RefreshButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusTextBlock;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReportTextBlock;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LogPreviewTextBlock;

	FTimerHandle RefreshTimerHandle;
};
