#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shared/Struct/DeliveryBot/Policy/DeliveryBotHttpPolicyResponseInfo.h"
#include "DeliveryBot_HttpPolicyComponent.generated.h"

class IHttpRequest;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDeliveryBotHttpPolicyResponseSignature, bool, bWasSuccessful, int32, ResponseCode, const FString&, ResponseBody);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDeliveryBotHttpPolicyParsedResponseSignature,	const FDeliveryBotHttpPolicyResponseInfo&,	ResponseInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDeliveryBotHttpGridResponseSignature, bool, bWasSuccessful,	int32, ResponseCode, const FString&, ResponseBody);


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROTOROBOTSIM_API UDeliveryBot_HttpPolicyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeliveryBot_HttpPolicyComponent();

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool SendObservationJson(const FString& observationJson);

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool TryParsePolicyResponseJson(const FString& responseBody, FDeliveryBotHttpPolicyResponseInfo& outResponseInfo) const;

	// 생성된 Grid JSON을 Python 서버의 /grid/update로 보낸다.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool SendGridJson(const FString& gridJson);
	
public:
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|HttpPolicy")
	bool IsRequestInFlight() const
	{
		return bRequestInFlight;
	}

	// Grid 전송 HTTP 요청이 아직 응답 대기 중인지 확인한다.
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|HttpPolicy")
	bool IsGridRequestInFlight() const
	{
		return bGridRequestInFlight;
	}
	
	
	
public:
	UPROPERTY(BlueprintAssignable, Category = "DeliveryBot|HttpPolicy")
	FDeliveryBotHttpPolicyParsedResponseSignature OnParsedPolicyResponse;
	
	UPROPERTY(BlueprintAssignable, Category = "DeliveryBot|HttpPolicy")
	FDeliveryBotHttpPolicyResponseSignature OnPolicyResponse;

	// Grid update 응답을 외부 컴포넌트에서 받을 수 있게 한다.
	UPROPERTY(BlueprintAssignable, Category = "DeliveryBot|HttpPolicy")
	FDeliveryBotHttpGridResponseSignature OnGridResponse;
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|HttpPolicy")
	FString PolicyServerUrl{ TEXT("http://127.0.0.1:8000/policy/action") };

	// Python 서버의 Grid update endpoint.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|HttpPolicy")
	FString GridServerUrl{ TEXT("http://127.0.0.1:8000/grid/update") };
	
	
private:
	void CancelActiveRequest();
	void CancelActiveGridRequest();

	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|HttpPolicy")
	float RequestTimeoutSecond{ 2.0f };
	
	
private:
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveRequest;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveGridRequest;
	
	bool bGridRequestInFlight{ false };
	bool bRequestInFlight{ false };
	bool bIsEndingPlay{ false };
};
