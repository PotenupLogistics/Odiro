#pragma once

#include "CoreMinimal.h"
#include "Platform/Widget/OdiroCommonUserWidget.h"
#include "Shared/SimulationSetupTypes.h"
#include "ProjectExperimentRunRowWidget.generated.h"

class UButton;
class UOdiroListItemViewModel;
class UTextBlock;
class UWidget;

class UProjectExperimentRunRowWidget;

// Project experiment status table row에서 분석 요청을 상위 화면으로 전달한다.
DECLARE_MULTICAST_DELEGATE_OneParam(FProjectExperimentRunRowAnalyzeRequestedNative, UProjectExperimentRunRowWidget*);

// Project run 한 줄의 UMG-owned layout과 state visual을 데이터로 갱신하는 widget.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UProjectExperimentRunRowWidget : public UOdiroCommonUserWidget
{
	GENERATED_BODY()

public:
	// 버튼 바인딩을 설정한다.
	virtual void NativeConstruct() override;

	// 버튼 바인딩을 해제한다.
	virtual void NativeDestruct() override;

	// run row 표시값과 state visibility를 갱신한다.
	void InitializeRunRow(
		const FString& runDirectory,
		const FString& runId,
		ESimulationRunState state,
		bool bCompleted,
		const FString& successRateLabel,
		const FString& totalDurationLabel,
		bool bCanAnalyze);

	// 공통 item ViewModel의 run id/path와 표시 상태를 run row UI에 반영한다.
	void InitializeFromItemViewModel(
		UOdiroListItemViewModel* itemViewModel,
		ESimulationRunState state,
		bool bCompleted,
		const FString& successRateLabel,
		const FString& totalDurationLabel,
		bool bCanAnalyze);

	// row가 대표하는 run directory를 반환한다.
	const FString& GetRunDirectory() const { return RunDirectory; }

	// 분석 버튼이 눌렸을 때 broadcast된다.
	FProjectExperimentRunRowAnalyzeRequestedNative OnAnalyzeRequested;

private:
	// 분석 버튼 click을 native delegate로 변환한다.
	UFUNCTION()
	void HandleAnalyzeClicked();

	// WBP가 소유한 state visual 중 현재 state만 보이게 한다.
	void RefreshStateVisibility(ESimulationRunState state) const;

	// 특정 state visual visibility를 갱신한다.
	static void SetStateBoxVisibility(UWidget* stateBox, bool bVisible);

	// project run directory 원본 경로.
	FString RunDirectory;

	// 현재 row에서 분석 요청을 허용하는지 여부.
	bool bAnalyzeEnabled = false;

	// Row 표시 데이터를 제공하는 project run item ViewModel 참조.
	UPROPERTY(Transient)
	TObjectPtr<UOdiroListItemViewModel> ItemViewModel;

	// run id 표시 텍스트.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RunIdText;

	// 완료 run은 100%, 그 외 상태는 dash로 표시하는 상태 텍스트.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ProgressText;

	// summary.json 기반 성공률 표시 텍스트.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SuccessRateText;

	// summary.json 기반 총 실행 시간 표시 텍스트.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TotalDurationText;

	// 분석 상세를 여는 버튼.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> AnalyzeButton;

	// 대기 state visual container.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PendingStateBox;

	// 진행 중 state visual container.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> RunningStateBox;

	// 완료 state visual container.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CompletedStateBox;

	// 실패 state visual container.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> FailedStateBox;

	// 중단 state visual container.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CanceledStateBox;
};
