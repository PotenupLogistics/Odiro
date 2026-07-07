#pragma once

#include "CoreMinimal.h"
#include "Platform/ViewModel/ProjectCreateScreenViewModel.h"
#include "UI/BaseThumbnailCardWidget.h"
#include "ProjectPresetCardWidget.generated.h"

class UBaseButtonWidget;
class UBaseTextWidget;
class UTexture2D;

DECLARE_MULTICAST_DELEGATE_OneParam(FProjectPresetCardSelectedNative, class UProjectPresetCardWidget*);

// Project Create 화면의 preset 하나를 표시하는 thumbnail card.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UProjectPresetCardWidget : public UBaseThumbnailCardWidget
{
	GENERATED_BODY()

public:
	// Preset item 값을 카드 UI에 반영한다.
	void InitializeCard(const FProjectCreatePresetItem& item);

	// 선택 상태를 card UI state에 반영한다.
	void SetSelected(bool bInSelected);

	// 카드가 대표하는 preset category를 반환한다.
	EProjectCreatePresetCategory GetPresetCategory() const { return Category; }

	// 카드가 대표하는 preset id를 반환한다.
	FString GetPresetId() const { return PresetId; }

	// Card 선택 요청을 parent widget에 알린다.
	FProjectPresetCardSelectedNative OnSelectedRequested;

protected:
	// Runtime click delegate를 WBP-owned 버튼에 연결한다.
	virtual void NativeConstruct() override;

	// Runtime click delegate를 WBP-owned 버튼에서 해제한다.
	virtual void NativeDestruct() override;

	// Button이 없는 WBP에서도 카드 click을 선택 요청으로 처리한다.
	virtual FReply NativeOnMouseButtonDown(const FGeometry& inGeometry, const FPointerEvent& inMouseEvent) override;

private:
	// Card button click을 선택 요청으로 변환한다.
	UFUNCTION()
	void HandleCardClicked(UBaseButtonWidget* button);

	// Preset thumbnail.png를 transient texture로 읽어 media에 적용한다.
	bool ApplyPresetThumbnail(const FString& thumbnailPath);

	// Runtime preset thumbnail texture를 BaseThumbnailCard의 rounded media 경로로 전달한다.
	void SetPresetThumbnailTexture(UTexture2D* thumbnailTexture);

	// Card 전체 click target. Visual layout은 WBP가 소유한다.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> CardButton;

	// Preset 표시 이름 text. Visual layout은 WBP가 소유한다.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextWidget> PresetTitleText;

	// Preset 보조 설명 text. Visual layout은 WBP가 소유한다.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextWidget> PresetSubtitleText;

	// Runtime에서 읽은 thumbnail.png texture의 card 수명 동안의 소유 참조.
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CardThumbnailTexture;

	// 이 card가 대표하는 preset category.
	EProjectCreatePresetCategory Category = EProjectCreatePresetCategory::Scenario;

	// 이 card가 대표하는 preset id.
	FString PresetId;

	// 이 card의 현재 선택 상태.
	bool bSelected = false;
};
