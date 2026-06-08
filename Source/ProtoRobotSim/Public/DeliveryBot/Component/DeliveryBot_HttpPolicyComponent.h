#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shared/Struct/DeliveryBot/Policy/DeliveryBotHttpPolicyResponseInfo.h"
#include "Shared/Struct/DeliveryBot/Policy/DeliveryBotPolicyCatalogSourceInfo.h"
#include "DeliveryBot_HttpPolicyComponent.generated.h"

class IHttpRequest;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDeliveryBotHttpPolicyResponseSignature, bool, bWasSuccessful, int32, ResponseCode, const FString&, ResponseBody);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDeliveryBotHttpPolicyParsedResponseSignature,	const FDeliveryBotHttpPolicyResponseInfo&,	ResponseInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDeliveryBotHttpGridResponseSignature, bool, bWasSuccessful,	int32, ResponseCode, const FString&, ResponseBody);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDeliveryBotHttpEpisodeStartResponseSignature, bool, bWasSuccessful, int32, ResponseCode, const FString&, ResponseBody);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDeliveryBotHttpEpisodeConfigUpdateResponseSignature, bool, bWasSuccessful, int32, ResponseCode, const FString&, ResponseBody);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDeliveryBotHttpPolicyCatalogSourcesResponseSignature, bool, bWasSuccessful, int32, ResponseCode, const FString&, ResponseBody);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDeliveryBotHttpPolicyCatalogSourcesParsedResponseSignature, const FDeliveryBotPolicyCatalogSourcesInfo&, SourcesInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDeliveryBotHttpPolicyCatalogResponseSignature, bool, bWasSuccessful, int32, ResponseCode, const FString&, ResponseBody);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDeliveryBotHttpPolicyCatalogParsedResponseSignature, const FDeliveryBotPolicyCatalogInfo&, CatalogInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(	FDeliveryBotHttpPolicySpecUpdateResponseSignature,	bool,	bWasSuccessful,	int32,	ResponseCode,	const FString&,	ResponseBody);



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
	void CancelActivePolicyCatalogSourcesRequest();
	void CancelActivePolicyCatalogRequest();
	void CancelActivePolicySpecUpdateRequest();
	
	
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
	
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool RequestPolicyCatalogSources();

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool TryParsePolicyCatalogSourcesJson(const FString& responseBody, FDeliveryBotPolicyCatalogSourcesInfo& outSourcesInfo) const;
	
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool RequestPolicyCatalogSource(const FString& catalogId);

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool TryParsePolicyCatalogJson(const FString& responseBody, FDeliveryBotPolicyCatalogInfo& outCatalogInfo) const;
	

	// 완성된 policySpec JSON을 Python에 POST한다.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool SendPolicySpecUpdateJson(const FString& policySpecUpdateJson);

	// 임시 테스트용 함수
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool SendDefaultRuntimePolicySpecUpdate();
	//  정책 ID 배열을 받아서 우선순위 순서대로 JSON을 만들고 Python에 보냄.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool SendRuntimePolicySpecUpdateByPolicyIds(const FString& catalogId,int32 catalogVersion, const TArray<FString>& enabledPolicyIds);
	// 임시 테스트용 함수. 장애물 감속/정지 정책 없이 normal_path_follow만 켬.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool SendNormalOnlyRuntimePolicySpecUpdate();
	
	// policySpec JSON 파일을 읽어서 문자열로 반환한다.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool LoadPolicySpecUpdateJsonFile(const FString& policySpecJsonFilePath, FString& outPolicySpecUpdateJson) const;

	// policySpec JSON 파일을 읽어서 Python에 POST한다.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool SendPolicySpecUpdateJsonFile(const FString& policySpecJsonFilePath);
	
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

	UFUNCTION(BlueprintPure, Category = "DeliveryBot|HttpPolicy")
	bool IsPolicyCatalogSourcesRequestInFlight() const
	{
		return bPolicyCatalogSourcesRequestInFlight;
	}
	
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|HttpPolicy")
	bool IsPolicySpecUpdateRequestInFlight() const
	{
		return bPolicySpecUpdateRequestInFlight;
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

	UPROPERTY(BlueprintAssignable, Category = "DeliveryBot|HttpPolicy")
	FDeliveryBotHttpPolicyCatalogSourcesResponseSignature OnPolicyCatalogSourcesResponse;

	UPROPERTY(BlueprintAssignable, Category = "DeliveryBot|HttpPolicy")
	FDeliveryBotHttpPolicyCatalogSourcesParsedResponseSignature OnParsedPolicyCatalogSourcesResponse;
	
	UPROPERTY(BlueprintAssignable, Category = "DeliveryBot|HttpPolicy")
	FDeliveryBotHttpPolicyCatalogResponseSignature OnPolicyCatalogResponse;

	UPROPERTY(BlueprintAssignable, Category = "DeliveryBot|HttpPolicy")
	FDeliveryBotHttpPolicyCatalogParsedResponseSignature OnParsedPolicyCatalogResponse;
	
	UPROPERTY(BlueprintAssignable, Category = "DeliveryBot|HttpPolicy")
	FDeliveryBotHttpPolicySpecUpdateResponseSignature OnPolicySpecUpdateResponse;
	
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|HttpPolicy")
	FString PolicyCatalogSourcesServerUrl{ TEXT("http://127.0.0.1:8000/policy/catalog/sources") };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|HttpPolicy")
	FString PolicyCatalogSourceServerUrl{ TEXT("http://127.0.0.1:8000/policy/catalog/source") };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|HttpPolicy")
	FString PolicySpecUpdateServerUrl{ TEXT("http://127.0.0.1:8000/policy/spec/update") };
	
	
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|HttpPolicy")
	float RequestTimeoutSecond{ 2.0f };
	
	
private:
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveRequest;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveGridRequest;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveEpisodeStartRequest;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveEpisodeConfigUpdateRequest;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActivePolicyCatalogSourcesRequest;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActivePolicyCatalogRequest;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActivePolicySpecUpdateRequest;

	
	bool bEpisodeStartRequestInFlight{ false };
	bool bGridRequestInFlight{ false };
	bool bRequestInFlight{ false };
	bool bIsEndingPlay{ false };
	bool bEpisodeConfigUpdateRequestInFlight{ false };
	bool bPolicyCatalogSourcesRequestInFlight{ false };
	bool bPolicyCatalogRequestInFlight{ false };
	bool bPolicySpecUpdateRequestInFlight{ false };
	
};
