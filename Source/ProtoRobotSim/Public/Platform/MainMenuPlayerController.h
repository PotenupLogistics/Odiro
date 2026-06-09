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
	
	// 실행에 사용할 PolicySpec 파일명을 저장
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Policy")
	void SetSelectedPolicySpecFileName(const FString& policySpecFileName);

	// 현재 저장된 PolicySpec 파일명으로 DeliveryBot 시작
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Run")
	bool StartDeliveryBotRunWithSelectedPolicySpec();

	// 현재 선택된 PolicySpec 파일명을 확인
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Policy")
	FString GetSelectedPolicySpecFileName() const { return SelectedPolicySpecFileName; }
	
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

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Policy", meta = (AllowPrivateAccess = "true"))
	FString SelectedPolicySpecFileName{ TEXT("PolicySpec_DefaultDelivery") };
	
	
};
