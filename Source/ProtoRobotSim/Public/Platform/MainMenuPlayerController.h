#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MainMenuPlayerController.generated.h"

class UMainMenuWidget;

// MainMenuMap에서 메뉴 UI를 viewport에 붙이고 입력 모드를 소유하는 PlayerController
UCLASS(BlueprintType, Blueprintable)
class PROTOROBOTSIM_API AMainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;

	UFUNCTION(BlueprintCallable, Category = "MainMenu|UI")
	UMainMenuWidget* ShowMainWidget();

	UFUNCTION(BlueprintCallable, Category = "MainMenu|UI")
	void RemoveMainWidget();

	UFUNCTION(BlueprintPure, Category = "MainMenu|UI")
	UMainMenuWidget* GetMainWidget() const { return MainWidget; }

protected:
	// 비어 있으면 `/Game/Widgets/MainMenu/WBP_MainMenu`를 기본으로 사용한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MainMenu|UI")
	TSubclassOf<UMainMenuWidget> MainWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MainMenu|UI")
	int32 MainWidgetZOrder = 0;

private:
	TSubclassOf<UMainMenuWidget> ResolveMainWidgetClass() const;
	void ApplyMainMenuInputMode(UMainMenuWidget* widget);

	UPROPERTY(Transient)
	TObjectPtr<UMainMenuWidget> MainWidget;
};
