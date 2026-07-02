#pragma once

#include "CoreMinimal.h"
#include "Platform/ViewModel/ProjectCreateScreenViewModel.h"
#include "UI/BaseWidget.h"
#include "ProjectCreateScreenWidget.generated.h"

class UBaseButtonWidget;
class UBaseTextInputWidget;
class UBaseTextWidget;
class UPanelWidget;
class UProjectCreateScreenViewModel;
class UProjectCreateScreenWidget;
class UProjectPresetCardWidget;
class UTextBlock;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FProjectCreateScreenCancelRequested, UProjectCreateScreenWidget*, ProjectCreateScreen);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FProjectCreateScreenProjectCreated, UProjectCreateScreenWidget*, ProjectCreateScreen, const FString&, ProjectPath);

// Master Widget 내부에 배치되는 project 생성 content panel.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UProjectCreateScreenWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// 외부 Master Widget이 소유한 ViewModel을 주입한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ProjectCreate")
	void SetViewModel(UProjectCreateScreenViewModel* viewModel);

	// 현재 연결된 ProjectCreate ViewModel을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ProjectCreate")
	UProjectCreateScreenViewModel* GetViewModel() const { return ProjectCreateScreenViewModel; }

	// ViewModel 상태를 다시 읽어 카드/입력/진단 표시를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ProjectCreate")
	void RefreshFromViewModel();

	// 현재 form state로 project 생성을 요청한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ProjectCreate")
	bool CreateCurrentProject();

	// 생성 취소를 Master Widget에 알린다.
	UPROPERTY(BlueprintAssignable, Category = "Platform|ProjectCreate|Events")
	FProjectCreateScreenCancelRequested OnCancelRequested;

	// Project 생성 완료를 Master Widget에 알린다.
	UPROPERTY(BlueprintAssignable, Category = "Platform|ProjectCreate|Events")
	FProjectCreateScreenProjectCreated OnProjectCreated;

protected:
	// Designer preview lifecycle hook.
	virtual void NativePreConstruct() override;

	// Runtime binding과 내부 ViewModel을 초기화한다.
	virtual void NativeConstruct() override;

	// Runtime binding을 해제한다.
	virtual void NativeDestruct() override;

private:
	// 내부 ViewModel이 없으면 생성하고 초기화한다.
	UProjectCreateScreenViewModel* EnsureProjectCreateScreenViewModel();

	// WBP default value를 ViewModel에 주입한다.
	void ApplyViewModelConfiguration();

	// 버튼/input delegate를 연결한다.
	void BindControls();

	// 버튼/input delegate를 해제한다.
	void UnbindControls();

	// Preset 카드 목록을 다시 생성한다.
	void RefreshPresetCards();

	// 한 preset section의 card 목록을 다시 생성한다.
	void RefreshPresetCardPanel(UPanelWidget* panel, const TArray<struct FProjectCreatePresetItem>& items);

	// 동적으로 생성한 preset card의 WBP-editable slot 간격을 적용한다.
	void ConfigurePresetCardSlot(UWidget* cardWidget, bool bLastCard) const;

	// 카드 class 설정을 해석한다.
	TSubclassOf<UProjectPresetCardWidget> ResolveProjectPresetCardWidgetClass() const;

	// OS folder picker로 project parent folder를 고른다.
	bool BrowseForProjectParentFolder(FString& outFolder) const;

	// Project name input 변경을 ViewModel에 반영한다.
	UFUNCTION()
	void HandleProjectNameChanged(UBaseTextInputWidget* widget, const FText& text);

	// Project parent folder input 변경을 ViewModel에 반영한다.
	UFUNCTION()
	void HandleProjectParentFolderChanged(UBaseTextInputWidget* widget, const FText& text);

	// Parent folder browse button click을 처리한다.
	UFUNCTION()
	void HandleBrowseFolderClicked(UBaseButtonWidget* button);

	// 취소 button click을 처리한다.
	UFUNCTION()
	void HandleCancelClicked(UBaseButtonWidget* button);

	// Project 생성 button click을 처리한다.
	UFUNCTION()
	void HandleCreateProjectClicked(UBaseButtonWidget* button);

	// Preset card click을 선택 요청으로 처리한다.
	void HandlePresetCardSelected(UProjectPresetCardWidget* cardWidget);

	// Scenario preset 카드가 들어갈 WBP-owned panel.
	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UPanelWidget> ScenarioPresetCardPanel;

	// Profile preset 카드가 들어갈 WBP-owned panel.
	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UPanelWidget> ProfilePresetCardPanel;

	// Policy preset 카드가 들어갈 WBP-owned panel.
	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UPanelWidget> PolicyPresetCardPanel;

	// Project name WBP-owned input.
	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UBaseTextInputWidget> ProjectNameInput;

	// Parent folder WBP-owned input. 읽기 전용 layout이면 없을 수 있다.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextInputWidget> ProjectParentFolderInput;

	// Parent folder WBP-owned text display. input 대신 사용할 수 있다.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextWidget> ProjectParentFolderText;

	// 생성될 project path WBP-owned text display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextWidget> ProjectPathText;

	// 선택된 scenario summary WBP-owned text.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextWidget> SelectedScenarioText;

	// 선택된 profile summary WBP-owned text.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextWidget> SelectedProfileText;

	// 선택된 policy summary WBP-owned text.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextWidget> SelectedPolicyText;

	// 선택된 setting summary WBP-owned text.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextWidget> SelectedSettingText;

	// ViewModel diagnostics를 표시할 WBP-owned text.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DiagnosticsText;

	// Parent folder picker를 여는 WBP-owned button.
	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UBaseButtonWidget> BrowseFolderButton;

	// 취소 action WBP-owned button.
	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UBaseButtonWidget> CancelButton;

	// Project 생성 action WBP-owned button.
	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UBaseButtonWidget> CreateProjectButton;

	// 반복 생성할 preset card Widget Blueprint class.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectCreate", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UProjectPresetCardWidget> ProjectPresetCardWidgetClass;

	// 동적으로 생성된 preset card 사이의 가로 간격.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectCreate|Layout", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float PresetCardSpacing = 12.0f;

	// 동적으로 생성된 preset card의 panel slot 가로 정렬.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectCreate|Layout", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<EHorizontalAlignment> PresetCardHorizontalAlignment = HAlign_Left;

	// 동적으로 생성된 preset card의 panel slot 세로 정렬.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectCreate|Layout", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<EVerticalAlignment> PresetCardVerticalAlignment = VAlign_Top;

	// WBP에서 수정 가능한 오류 상황별 진단 문구.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectCreate|Diagnostics", meta = (AllowPrivateAccess = "true"))
	FProjectCreateScreenDiagnosticMessages DiagnosticMessages;

	// WBP에서 수정 가능한 create form 기본값.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectCreate|Defaults", meta = (AllowPrivateAccess = "true"))
	FProjectCreateScreenDefaultValues DefaultValues;

	// Parent folder picker dialog title.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectCreate|Dialog", meta = (AllowPrivateAccess = "true"))
	FText BrowseDialogTitle;

	// 이 screen이 소비하는 독립 ProjectCreate ViewModel.
	UPROPERTY(Transient)
	TObjectPtr<UProjectCreateScreenViewModel> ProjectCreateScreenViewModel;

	// 현재 생성된 preset card 인스턴스.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProjectPresetCardWidget>> PresetCards;
};
