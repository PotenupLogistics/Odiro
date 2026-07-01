#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Engine/TimerHandle.h"
#include "WindowsControlWidget.generated.h"

class SWindow;
class UButton;
class UTextBlock;

// Windows caption button에 표시할 system glyph 종류.
UENUM(BlueprintType)
enum class EPlatformWindowCaptionGlyph : uint8
{
	Minimize,
	Maximize,
	Restore,
	Close
};

// WBP가 소유하는 Windows caption button layout에 native window command를 연결하는 widget.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UWindowsControlWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// TextBlock에 Windows caption glyph와 font를 적용한다.
	UFUNCTION(BlueprintCallable, Category = "Window Controls")
	static bool ApplyWindowsCaptionGlyph(UTextBlock* textBlock, EPlatformWindowCaptionGlyph glyph);

	// Designer preview와 runtime에서 caption glyph를 갱신한다.
	virtual void NativePreConstruct() override;

	// Button click delegate를 native window command에 연결한다.
	virtual void NativeConstruct() override;

	// Button click delegate를 해제한다.
	virtual void NativeDestruct() override;

	// 현재 window state에 맞춰 maximize/restore icon과 Blueprint visual state를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Window Controls")
	void RefreshWindowControlButtons();

	// 현재 game window를 최소화한다.
	UFUNCTION(BlueprintCallable, Category = "Window Controls")
	void MinimizeWindow();

	// 현재 game window의 maximize/restore state를 전환한다.
	UFUNCTION(BlueprintCallable, Category = "Window Controls")
	void ToggleMaximizeRestoreWindow();

	// 현재 game window에 close 요청을 보낸다.
	UFUNCTION(BlueprintCallable, Category = "Window Controls")
	void CloseWindow();

	// 현재 game window가 maximize 상태인지 반환한다.
	UFUNCTION(BlueprintPure, Category = "Window Controls")
	bool IsWindowMaximized() const;

protected:
	// Caption glyph font size override; 0 preserves the TextBlock Widget Blueprint font size.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Window Controls|Defaults", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CaptionGlyphFontSize = 0.0f;

	// WBP가 maximize/restore state별 hover, tooltip, visual state를 갱신하는 extension point.
	UFUNCTION(BlueprintImplementableEvent, Category = "Window Controls")
	void BP_OnMaximizeRestoreStateChanged(bool bIsMaximized);

private:
	// Button click delegate를 연결한다.
	void BindControls();

	// Button click delegate를 해제한다.
	void UnbindControls();

	// WBP TextBlock에 Windows caption glyph를 적용한다.
	void ApplyCaptionGlyphs();

	// TextBlock에 WBP default font size로 Windows caption glyph를 적용한다.
	bool ApplyConfiguredWindowsCaptionGlyph(UTextBlock* textBlock, EPlatformWindowCaptionGlyph glyph) const;

	// 이 widget이 실제로 표시되는 Slate window를 반환한다.
	TSharedPtr<SWindow> ResolveGameWindow() const;

	// PIE close button이 Editor 종료 대신 play session 종료로 처리되어야 하는지 반환한다.
	bool ShouldEndPlayForClose() const;

	// PIE minimize/maximize button이 editor immersive toggle로 처리되어야 하는지 반환한다.
	bool ShouldToggleEditorImmersiveForWindowCommand() const;

	// Editor의 F11 immersive viewport command를 실행한다.
	bool ToggleEditorImmersiveViewport() const;

#if WITH_EDITOR
	// PIE 중 외부 F11 command로 바뀐 immersive 상태를 icon state에 반영하는 timer를 시작한다.
	void StartEditorImmersiveStateRefreshTimer();

	// PIE immersive 상태 동기화 timer를 정리한다.
	void StopEditorImmersiveStateRefreshTimer();

	// 현재 Editor immersive 상태를 읽어 icon state와 Blueprint state를 갱신한다.
	void HandleEditorImmersiveStateRefresh();

	// 현재 active level viewport의 immersive 상태를 반환한다.
	bool IsEditorImmersiveViewportActive() const;
#endif

	// Minimize button click을 window command로 변환한다.
	UFUNCTION()
	void HandleMinimizeClicked();

	// Maximize/restore button click을 window command로 변환한다.
	UFUNCTION()
	void HandleMaximizeRestoreClicked();

	// Close button click을 window command로 변환한다.
	UFUNCTION()
	void HandleCloseClicked();

	// WBP layout이 소유하는 minimize button.
	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> MinimizeButton;

	// WBP layout이 소유하는 maximize/restore button.
	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> MaximizeRestoreButton;

	// WBP layout이 소유하는 close button.
	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;

	// WBP layout이 소유하는 minimize glyph text.
	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> MinimizeText;

	// WBP layout이 소유하는 maximize/restore glyph text.
	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> MaximizeRestoreText;

	// WBP layout이 소유하는 close glyph text.
	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> CloseText;

	// 마지막으로 Blueprint에 전달한 maximize state.
	bool bLastKnownMaximized = false;

	// 마지막 maximize state cache가 유효한지 여부.
	bool bHasCachedMaximizedState = false;

	// PIE에서 maximize/restore button이 대체하는 editor immersive 상태.
	bool bIsEditorImmersiveMaximizeActive = false;

#if WITH_EDITOR
	// PIE에서 외부 F11 command를 감지하기 위한 editor-only timer handle.
	FTimerHandle EditorImmersiveStateRefreshTimerHandle;
#endif
};
