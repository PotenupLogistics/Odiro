#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Shared/SimulationSetupTypes.h"
#include "ProjectExperimentRunRowWidget.generated.h"

class UBaseButtonWidget;
class UBaseProgressBarWidget;
class UOdiroListItemViewModel;
class UTextBlock;
class UWidget;

class UProjectExperimentRunRowWidget;

// Project experiment status table row에서 분석 요청을 상위 화면으로 전달한다.
DECLARE_MULTICAST_DELEGATE_OneParam(FProjectExperimentRunRowAnalyzeRequestedNative, UProjectExperimentRunRowWidget*);

// Project run 한 줄의 UMG-owned layout과 state visual을 데이터로 갱신하는 widget.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UProjectExperimentRunRowWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// Designer/default row text를 bound WBP controls에 반영한다.
	virtual void NativePreConstruct() override;

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
		int32 progressTotalCount,
		const FString& successRateLabel,
		const FString& totalDurationLabel,
		bool bCanAnalyze);

	// 공통 item ViewModel의 run id/path와 표시 상태를 run row UI에 반영한다.
	void InitializeFromItemViewModel(
		UOdiroListItemViewModel* itemViewModel,
		ESimulationRunState state,
		bool bCompleted,
		int32 progressTotalCount,
		const FString& successRateLabel,
		const FString& totalDurationLabel,
		bool bCanAnalyze);

	// Run table header나 preview row가 사용할 수 있는 임의 컬럼 텍스트를 설정한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|RunRow")
	void SetDisplayTexts(
		FText runIdText,
		FText progressText,
		FText progressCountText,
		FText successRateText,
		FText totalDurationText,
		FText actionText);

	// Progress column에서 bar와 count label 표시 여부를 설정한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|RunRow")
	void SetProgressPresentation(bool bInShowProgressBar, bool bInShowProgressCountText);

	// Action column에서 label과 analyze button 중 어느 쪽을 표시할지 설정한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|RunRow")
	void SetActionPresentation(bool bInShowActionText, bool bInShowAnalyzeButton);

	// row가 대표하는 run directory를 반환한다.
	const FString& GetRunDirectory() const { return RunDirectory; }

	// 분석 버튼이 눌렸을 때 broadcast된다.
	FProjectExperimentRunRowAnalyzeRequestedNative OnAnalyzeRequested;

private:
	// 분석 버튼 click을 native delegate로 변환한다.
	UFUNCTION()
	void HandleAnalyzeClicked(UBaseButtonWidget* button);

	// WBP가 소유한 state visual 중 현재 state만 보이게 한다.
	void RefreshStateVisibility(ESimulationRunState state) const;

	// 특정 state visual visibility를 갱신한다.
	static void SetStateBoxVisibility(UWidget* stateBox, bool bVisible);

	// 현재 display property를 WBP text/button에 적용한다.
	void ApplyDisplayTexts() const;

	// project run directory 원본 경로.
	FString RunDirectory;

	// 현재 row에서 분석 요청을 허용하는지 여부.
	bool bAnalyzeEnabled = false;

	// InitializeRunRow로 실제 run data가 반영된 row인지 여부.
	bool bHasRunData = false;

	// Row 표시 데이터를 제공하는 project run item ViewModel 참조.
	UPROPERTY(Transient)
	TObjectPtr<UOdiroListItemViewModel> ItemViewModel;

	// 첫 번째 run/id 컬럼 기본 텍스트.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunRow", meta = (AllowPrivateAccess = "true"))
	FText RunIdDisplayText;

	// Header 호환용 progress 컬럼 기본 텍스트.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunRow", meta = (AllowPrivateAccess = "true"))
	FText ProgressDisplayText;

	// Progress column에서 진행 횟수/총 횟수로 표시할 기본 텍스트.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunRow", meta = (AllowPrivateAccess = "true"))
	FText ProgressCountDisplayText;

	// 세 번째 success rate 컬럼 기본 텍스트.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunRow", meta = (AllowPrivateAccess = "true"))
	FText SuccessRateDisplayText;

	// 네 번째 duration/median 컬럼 기본 텍스트.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunRow", meta = (AllowPrivateAccess = "true"))
	FText TotalDurationDisplayText;

	// 다섯 번째 action 컬럼 기본 텍스트.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunRow", meta = (AllowPrivateAccess = "true"))
	FText ActionDisplayText = NSLOCTEXT("ProjectExperimentRunRow", "DefaultActionText", "상세 보기");

	// progress column에서 progress bar를 표시할지 여부.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunRow", meta = (AllowPrivateAccess = "true"))
	bool bShowProgressBar = false;

	// progress column에서 진행 횟수/총 횟수 텍스트를 표시할지 여부.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunRow", meta = (AllowPrivateAccess = "true"))
	bool bShowProgressCountText = false;

	// action column에서 일반 텍스트 label을 표시할지 여부.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunRow", meta = (AllowPrivateAccess = "true"))
	bool bShowActionText = false;

	// action column에서 analyze button을 표시할지 여부.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunRow", meta = (AllowPrivateAccess = "true"))
	bool bShowAnalyzeButton = true;

	// run id 표시 텍스트.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RunIdText;

	// 진행 횟수/총 횟수 표시 텍스트.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ProgressCountText;

	// 진행률을 표시하는 base progress bar.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseProgressBarWidget> ProgressBar;

	// summary.json 기반 성공률 표시 텍스트.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SuccessRateText;

	// summary.json 기반 총 실행 시간 표시 텍스트.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TotalDurationText;

	// 분석 상세를 여는 버튼.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> AnalyzeButton;

	// action column label 표시 텍스트.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ActionLabelText;

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
