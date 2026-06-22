#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ProjectTemplateCardWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE_OneParam(FProjectTemplateCardSelectedNative, class UProjectTemplateCardWidget*);
DECLARE_MULTICAST_DELEGATE_OneParam(FProjectTemplateCardContextNative, class UProjectTemplateCardWidget*);

// StartupMenu card item layout과 interaction binding을 담당하는 widget.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UProjectTemplateCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Widget Blueprint default palette를 초기화한다.
	UProjectTemplateCardWidget(const FObjectInitializer& objectInitializer);

	virtual void NativeConstruct() override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& inGeometry, const FPointerEvent& inMouseEvent) override;

	// Item id와 표시 이름을 card UI에 반영한다.
	void InitializeCard(const FString& itemId, const FString& displayName, const FString& subtitle = FString());

	// 선택 상태를 card UI state에 반영한다.
	void SetSelected(bool bInSelected);

	// 이 card가 대표하는 item id를 반환한다.
	FString GetItemId() const { return ItemId; }

	// Card 선택 요청을 parent widget에 알린다.
	FProjectTemplateCardSelectedNative OnSelectedRequested;

	// Card context action 요청을 parent widget에 알린다.
	FProjectTemplateCardContextNative OnContextRequested;

protected:
	// Card button click을 선택 요청으로 변환한다.
	UFUNCTION()
	void HandleCardClicked();

	// Card hover 시작 state를 갱신한다.
	UFUNCTION()
	void HandleCardHovered();

	// Card hover 종료 state를 갱신한다.
	UFUNCTION()
	void HandleCardUnhovered();

private:
	// 현재 active/hover state에 맞춰 WBP surface widget의 색을 갱신한다.
	void RefreshVisualState();

	// 현재 active state에 맞는 normal button 색을 반환한다.
	FLinearColor ResolveNormalBackgroundColor() const;

	// 현재 active state에 맞는 hover button 색을 반환한다.
	FLinearColor ResolveHoveredBackgroundColor() const;

	// 현재 active state에 맞는 pressed button 색을 반환한다.
	FLinearColor ResolvePressedBackgroundColor() const;

	// 현재 active/hover state에 맞는 text 색을 반환한다.
	FLinearColor ResolveTextColor() const;

	// Card 전체 click target. Visual layout은 WBP가 소유한다.
	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> CardButton;

	// Item 표시 이름 text. Visual layout은 WBP가 소유한다.
	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> CardNameLabel;

	// Item 보조 설명 text. Visual layout은 WBP가 소유한다.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CardSubtitleLabel;

	// 기본 card 배경색. WBP default에서 조정한다.
	UPROPERTY(EditDefaultsOnly, Category = "ProjectTemplateCard|Style")
	FLinearColor DefaultBackgroundColor;

	// Hover card 배경색. WBP default에서 조정한다.
	UPROPERTY(EditDefaultsOnly, Category = "ProjectTemplateCard|Style")
	FLinearColor HoverBackgroundColor;

	// Pressed card 배경색. WBP default에서 조정한다.
	UPROPERTY(EditDefaultsOnly, Category = "ProjectTemplateCard|Style")
	FLinearColor PressedBackgroundColor;

	// 선택 card 배경색. WBP default에서 조정한다.
	UPROPERTY(EditDefaultsOnly, Category = "ProjectTemplateCard|Style")
	FLinearColor ActiveBackgroundColor;

	// 선택 card hover 배경색. WBP default에서 조정한다.
	UPROPERTY(EditDefaultsOnly, Category = "ProjectTemplateCard|Style")
	FLinearColor ActiveHoverBackgroundColor;

	// 선택 card pressed 배경색. WBP default에서 조정한다.
	UPROPERTY(EditDefaultsOnly, Category = "ProjectTemplateCard|Style")
	FLinearColor ActivePressedBackgroundColor;

	// 기본 card text 색. WBP default에서 조정한다.
	UPROPERTY(EditDefaultsOnly, Category = "ProjectTemplateCard|Style")
	FLinearColor DefaultTextColor;

	// 선택 또는 hover card text 색. WBP default에서 조정한다.
	UPROPERTY(EditDefaultsOnly, Category = "ProjectTemplateCard|Style")
	FLinearColor ActiveTextColor;

	// 이 card가 대표하는 item id.
	FString ItemId;

	// 이 card의 현재 선택 상태.
	bool bSelected = false;

	// 이 card의 현재 hover 상태.
	bool bHovered = false;
};
