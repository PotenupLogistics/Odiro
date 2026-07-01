#pragma once

#include "CoreMinimal.h"
#include "Platform/Widget/OdiroCommonUserWidget.h"
#include "UI/BaseWidgetTypes.h"
#include "WindowActionBarWidget.generated.h"

class UBaseButtonWidget;
class UHorizontalBox;
class UTexture2D;

// Action button 요청을 native parent surface에 전달하는 event.
DECLARE_MULTICAST_DELEGATE_OneParam(FWindowActionRequestedNative, FName);

// Action button 요청을 Blueprint parent surface에 전달하는 event.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWindowActionRequested, FName, ActionId);

// Window action bar에 표시할 icon-only button 구성.
USTRUCT(BlueprintType)
struct ODIROSIM_API FWindowActionButtonConfig
{
	GENERATED_BODY()

	// Logical action id.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Action Bar")
	FName ActionId;

	// Button 표시 여부.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Action Bar")
	bool bVisible = true;

	// Button 입력 가능 여부.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Action Bar")
	bool bEnabled = true;

	// Tooltip and accessible action label; visible text is intentionally hidden by WindowActionBarWidget.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Action Bar")
	FText Label;

	// Button semantic variant.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Action Bar")
	EBaseWidgetVariant Variant = EBaseWidgetVariant::Neutral;

	// true일 때만 Variant를 runtime button에 적용한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Action Bar")
	bool bOverrideVariant = false;

	// Button size scale.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Action Bar")
	EBaseWidgetSize Size = EBaseWidgetSize::Small;

	// true일 때만 Size를 runtime button에 적용한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Action Bar")
	bool bOverrideSize = false;

	// Primary variant shortcut.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Action Bar")
	bool bPrimary = false;

	// true일 때만 bPrimary를 runtime button에 적용한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Action Bar")
	bool bOverridePrimary = false;

	// Optional icon texture.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Action Bar")
	TObjectPtr<UTexture2D> Icon = nullptr;

	// Optional icon glyph text shown when Icon is empty.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Action Bar")
	FText IconGlyphText;

	// Icon box and image/glyph size; 0 keeps the Button Widget Blueprint default.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Action Bar", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "64.0"))
	float IconSize = 0.0f;

	// Square desired-size constraints.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Action Bar")
	FBaseWidgetSizeConstraints SizeConstraints;
};

// Screen별 icon-only action button 목록을 구성하는 standalone action bar widget.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UWindowActionBarWidget : public UOdiroCommonUserWidget
{
	GENERATED_BODY()

public:
	// 기본 button class와 WBP default action 목록을 preview에 반영한다.
	virtual void NativePreConstruct() override;

	// Runtime action button delegate를 native event로 연결한다.
	virtual void NativeConstruct() override;

	// Runtime action button delegate 연결을 해제한다.
	virtual void NativeDestruct() override;

	// Action button 목록을 현재 screen 설정으로 교체한다.
	UFUNCTION(BlueprintCallable, Category = "Window|Action Bar")
	void SetActionButtons(const TArray<FWindowActionButtonConfig>& actions);

	// 단일 action button을 추가하거나 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Window|Action Bar")
	void SetActionButtonConfig(FName actionId, const FWindowActionButtonConfig& config);

	// 단일 action button 표시 여부를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Window|Action Bar")
	void SetActionButtonVisible(FName actionId, bool bVisible);

	// Action button 목록을 비운다.
	UFUNCTION(BlueprintCallable, Category = "Window|Action Bar")
	void ClearActionButtons();

	// Action button click을 native owner에게 전달한다.
	FWindowActionRequestedNative OnActionRequestedNative;

	// Action button click을 Blueprint owner에게 전달한다.
	UPROPERTY(BlueprintAssignable, Category = "Window|Action Bar|Events")
	FWindowActionRequested OnActionRequested;

protected:
	// Action button을 런타임 생성할 WBP class. WBP default로 설정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Window|Action Bar")
	TSubclassOf<UBaseButtonWidget> ActionButtonWidgetClass;

	// Designer preview와 초기 runtime에 사용할 action 목록.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Window|Action Bar")
	TArray<FWindowActionButtonConfig> DefaultActionButtons;

private:
	// 현재 config로 action button widget 목록을 재생성한다.
	void RebuildActionButtons();

	// Button click delegate를 연결한다.
	void BindControls();

	// Button click delegate 연결을 해제한다.
	void UnbindControls();

	// 단일 action button에 config 값을 반영한다.
	void ConfigureActionButton(UBaseButtonWidget* actionButton, const FWindowActionButtonConfig& config) const;

	// WBP default action 목록을 runtime config로 복사한다.
	void EnsureActionButtonConfigsFromDefaults();

	// 단일 action button의 native delegate를 연결한다.
	void BindActionButton(UBaseButtonWidget* actionButton);

	// 단일 action button의 native delegate 연결을 해제한다.
	void UnbindActionButton(UBaseButtonWidget* actionButton);

	// Action button click source를 logical action id로 변환한다.
	FName ResolveActionIdByButton(const UBaseButtonWidget* actionButton) const;

	// 저장된 config index를 반환한다.
	int32 FindActionConfigIndex(FName actionId) const;

	// Action button click을 event로 변환한다.
	UFUNCTION()
	void HandleActionClicked(UBaseButtonWidget* button);

	// Runtime action buttons를 담는 WBP-owned container.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> ActionButtonContainer;

	// 현재 screen action button config 목록.
	UPROPERTY(Transient)
	TArray<FWindowActionButtonConfig> ActionButtonConfigs;

	// 생성된 action button widget.
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UBaseButtonWidget>> ActionButtonsById;
};
