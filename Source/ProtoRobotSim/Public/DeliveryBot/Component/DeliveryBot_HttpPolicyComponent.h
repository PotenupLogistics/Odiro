#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shared/Struct/DeliveryBot/Policy/DeliveryBotHttpPolicyResponseInfo.h"
#include "DeliveryBot_HttpPolicyComponent.generated.h"

class IHttpRequest;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDeliveryBotHttpPolicyResponseSignature, bool, bWasSuccessful, int32, ResponseCode, const FString&, ResponseBody);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDeliveryBotHttpPolicyParsedResponseSignature,	const FDeliveryBotHttpPolicyResponseInfo&,	ResponseInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDeliveryBotHttpGridResponseSignature, bool, bWasSuccessful,	int32, ResponseCode, const FString&, ResponseBody);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDeliveryBotHttpEpisodeStartResponseSignature, bool, bWasSuccessful, int32, ResponseCode, const FString&, ResponseBody);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDeliveryBotHttpEpisodeConfigUpdateResponseSignature, bool, bWasSuccessful, int32, ResponseCode, const FString&, ResponseBody);


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROTOROBOTSIM_API UDeliveryBot_HttpPolicyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeliveryBot_HttpPolicyComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void CancelActiveRequest();
	void CancelActiveGridRequest();
	void CancelActiveEpisodeStartRequest();
	void CancelActiveEpisodeConfigUpdateRequest();
	
public:
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool SendObservationJson(const FString& observationJson);

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool TryParsePolicyResponseJson(const FString& responseBody, FDeliveryBotHttpPolicyResponseInfo& outResponseInfo) const;

	// 생성된 Grid JSON을 Python 서버의 /grid/update로 보낸다.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool SendGridJson(const FString& gridJson);
	
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool SendEpisodeStartJson(const FString& episodeStartJson);
	
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool SendEpisodeConfigUpdateJson(const FString& configUpdateJson);
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
	
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|HttpPolicy")
	bool IsEpisodeStartRequestInFlight() const
	{
		return bEpisodeStartRequestInFlight;
	}
	
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|HttpPolicy")
	bool IsEpisodeConfigUpdateRequestInFlight() const
	{
		return bEpisodeConfigUpdateRequestInFlight;
	}
	
	
public:
	UPROPERTY(BlueprintAssignable, Category = "DeliveryBot|HttpPolicy")
	FDeliveryBotHttpPolicyParsedResponseSignature OnParsedPolicyResponse;
	
	UPROPERTY(BlueprintAssignable, Category = "DeliveryBot|HttpPolicy")
	FDeliveryBotHttpPolicyResponseSignature OnPolicyResponse;

	// Grid update 응답을 외부 컴포넌트에서 받을 수 있게 한다.
	UPROPERTY(BlueprintAssignable, Category = "DeliveryBot|HttpPolicy")
	FDeliveryBotHttpGridResponseSignature OnGridResponse;
	
	UPROPERTY(BlueprintAssignable, Category = "DeliveryBot|HttpPolicy")
	FDeliveryBotHttpEpisodeStartResponseSignature OnEpisodeStartResponse;
	
	UPROPERTY(BlueprintAssignable, Category = "DeliveryBot|HttpPolicy")
	FDeliveryBotHttpEpisodeConfigUpdateResponseSignature OnEpisodeConfigUpdateResponse;
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|HttpPolicy")
	FString PolicyServerUrl{ TEXT("http://127.0.0.1:8000/policy/action") };

	// Python 서버의 Grid update endpoint.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|HttpPolicy")
	FString GridServerUrl{ TEXT("http://127.0.0.1:8000/grid/update") };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|HttpPolicy")
	FString EpisodeStartServerUrl{ TEXT("http://127.0.0.1:8000/episode/start") };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|HttpPolicy")
	FString EpisodeConfigUpdateServerUrl{ TEXT("http://127.0.0.1:8000/episode/config/update") };
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|HttpPolicy")
	float RequestTimeoutSecond{ 2.0f };
	
	
private:
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveRequest;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveGridRequest;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveEpisodeStartRequest;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveEpisodeConfigUpdateRequest;
	
	bool bEpisodeStartRequestInFlight{ false };
	bool bGridRequestInFlight{ false };
	bool bRequestInFlight{ false };
	bool bIsEndingPlay{ false };
	bool bEpisodeConfigUpdateRequestInFlight{ false };
};
