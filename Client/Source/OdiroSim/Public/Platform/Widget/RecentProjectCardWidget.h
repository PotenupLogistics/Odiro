#pragma once

#include "CoreMinimal.h"
#include "UI/BaseThumbnailCardWidget.h"
#include "Platform/ViewModel/StartupScreenViewModel.h"
#include "RecentProjectCardWidget.generated.h"

class UBaseButtonWidget;
class UBaseTextWidget;
class UTexture2D;

DECLARE_MULTICAST_DELEGATE_OneParam(FRecentProjectCardSelectedNative, class URecentProjectCardWidget*);
DECLARE_MULTICAST_DELEGATE_TwoParams(
	FRecentProjectCardContextMenuNative,
	class URecentProjectCardWidget*,
	FVector2D);

// StartupScreen의 최근 project 하나를 표시하는 thumbnail card.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API URecentProjectCardWidget : public UBaseThumbnailCardWidget
{
	GENERATED_BODY()

public:
	// Widget Blueprint default 상태를 초기화한다.
	URecentProjectCardWidget(const FObjectInitializer& objectInitializer);

	// 최근 project item 값을 카드 UI에 반영한다.
	void InitializeCard(const FStartupScreenRecentProjectItem& item);

	// 선택 상태를 card UI state에 반영한다.
	void SetSelected(bool bInSelected);

	// 카드가 대표하는 project root 절대 경로를 반환한다.
	FString GetProjectPath() const { return ProjectPath; }

	// Card 선택/open 요청을 parent widget에 알린다.
	FRecentProjectCardSelectedNative OnSelectedRequested;

	// Card context menu 요청을 parent widget에 알린다.
	FRecentProjectCardContextMenuNative OnContextMenuRequested;

protected:
	// BaseThumbnailCard의 token 기반 surface 동기화를 그대로 사용한다.
	virtual void SynchronizeBaseProperties() override;

	// Runtime click delegate를 WBP-owned 버튼에 연결한다.
	virtual void NativeConstruct() override;

	// Runtime click delegate를 WBP-owned 버튼에서 해제한다.
	virtual void NativeDestruct() override;

	// Child button이 pointer event를 처리하기 전에 context menu 요청을 잡는다.
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& inGeometry, const FPointerEvent& inMouseEvent) override;

	// Button이 없는 WBP에서도 카드 click을 선택 요청으로 처리한다.
	virtual FReply NativeOnMouseButtonDown(const FGeometry& inGeometry, const FPointerEvent& inMouseEvent) override;

private:
	// Card button click을 선택 요청으로 변환한다.
	UFUNCTION()
	void HandleCardClicked(UBaseButtonWidget* button);

	// ViewModel이 계산한 preview image 경로를 transient texture로 읽어 thumbnail image에 적용한다.
	bool ApplyProjectPreviewThumbnail(const FString& previewPath);

	// Card 전체 click target. Visual layout은 WBP가 소유한다.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> CardButton;

	// Project 표시 이름 text. Visual layout은 WBP가 소유한다.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextWidget> ProjectNameText;

	// Project 보조 설명 text. Visual layout은 WBP가 소유한다.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextWidget> ProjectSubtitleText;

	// Runtime에서 읽은 preview.png texture의 card 수명 동안의 소유 참조.
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CardThumbnailTexture;

	// 이 card가 대표하는 project root 절대 경로.
	FString ProjectPath;

	// 이 card의 현재 선택 상태.
	bool bSelected = false;
};
