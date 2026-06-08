#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MainMenuPlayerController.generated.h"

class UDeliveryBot_HttpPolicyComponent;
class UDeliveryBot_PolicyControllerComponent;
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

	// 파일명을 받아서 정책 확정 요청을 시작
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Run")
	bool StartDeliveryBotRunWithPolicySpecFile(const FString& policySpecFileName);
	
	
private:
	// /policy/spec/update 응답을 받고 성공이면 Episode Start로 넘어간다.
	UFUNCTION()
	void HandleRunPolicySpecUpdateResponse(bool bWasSuccessful, int32 responseCode, const FString& responseBody);
	
	// 월드의 BP_DeliveryBot을 찾아서 PolicyControllerComponent->SendEpisodeStartToPolicyServerOnce()를 호출한다.
	bool StartEpisodeAfterPolicyConfirmed();
	

	UDeliveryBot_HttpPolicyComponent* FindPolicyHttpComponent() const;
	UDeliveryBot_PolicyControllerComponent* FindDeliveryBotPolicyController() const;
	
	
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

	UPROPERTY(Transient)
	TObjectPtr<UDeliveryBot_HttpPolicyComponent> ActiveRunPolicyHttpComponent;
	
};
