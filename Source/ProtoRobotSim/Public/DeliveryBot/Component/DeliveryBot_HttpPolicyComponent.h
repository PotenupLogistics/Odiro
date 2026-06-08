#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shared/Struct/DeliveryBot/Policy/DeliveryBotHttpPolicyResponseInfo.h"
#include "Shared/Struct/DeliveryBot/Policy/DeliveryBotPolicyCatalogSourceInfo.h"
#include "DeliveryBot_HttpPolicyComponent.generated.h"

class IHttpRequest;

// 정책 판단 응답 이벤트: observation 전송 후 /policy/action 응답을 알린다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDeliveryBotHttpPolicyResponseSignature, bool, bWasSuccessful, int32, ResponseCode, const FString&, ResponseBody);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDeliveryBotHttpPolicyParsedResponseSignature, const FDeliveryBotHttpPolicyResponseInfo&, ResponseInfo);

// 서버 동기화 응답 이벤트: grid, episode, config 갱신 결과를 알린다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDeliveryBotHttpGridResponseSignature, bool, bWasSuccessful, int32, ResponseCode, const FString&, ResponseBody);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDeliveryBotHttpEpisodeStartResponseSignature, bool, bWasSuccessful, int32, ResponseCode, const FString&, ResponseBody);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDeliveryBotHttpEpisodeConfigUpdateResponseSignature, bool, bWasSuccessful, int32, ResponseCode, const FString&, ResponseBody);

// 정책 catalog 응답 이벤트: catalog 목록과 선택된 catalog 정보를 알린다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDeliveryBotHttpPolicyCatalogSourcesResponseSignature, bool, bWasSuccessful, int32, ResponseCode, const FString&, ResponseBody);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDeliveryBotHttpPolicyCatalogSourcesParsedResponseSignature, const FDeliveryBotPolicyCatalogSourcesInfo&, SourcesInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDeliveryBotHttpPolicyCatalogResponseSignature, bool, bWasSuccessful, int32, ResponseCode, const FString&, ResponseBody);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDeliveryBotHttpPolicyCatalogParsedResponseSignature, const FDeliveryBotPolicyCatalogInfo&, CatalogInfo);

// 정책 spec 갱신 응답 이벤트: 실행 중 정책 사용 여부/우선순위 변경 결과를 알린다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDeliveryBotHttpPolicySpecUpdateResponseSignature, bool, bWasSuccessful, int32, ResponseCode, const FString&, ResponseBody);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROTOROBOTSIM_API UDeliveryBot_HttpPolicyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeliveryBot_HttpPolicyComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;


public: // 정책 판단 요청: 현재 observation을 Python 정책 서버에 보내고 action 응답을 받는다.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool SendObservationJson(const FString& observationJson);

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool TryParsePolicyResponseJson(const FString& responseBody, FDeliveryBotHttpPolicyResponseInfo& outResponseInfo) const;

public: // Grid 동기화: 생성된 Grid JSON을 Python 서버의 /grid/update로 보낸다.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool SendGridJson(const FString& gridJson);

public: // Episode 동기화: episode 시작 정보와 런타임 config 변경을 Python 서버에 보낸다.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool SendEpisodeStartJson(const FString& episodeStartJson);

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool SendEpisodeConfigUpdateJson(const FString& configUpdateJson);

public: // 정책 catalog 조회/선택: 사용 가능한 catalog 목록을 받고 선택 catalog를 활성화한다.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool RequestPolicyCatalogSources();

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool TryParsePolicyCatalogSourcesJson(const FString& responseBody, FDeliveryBotPolicyCatalogSourcesInfo& outSourcesInfo) const;

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool RequestPolicyCatalogSource(const FString& catalogId);

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool TryParsePolicyCatalogJson(const FString& responseBody, FDeliveryBotPolicyCatalogInfo& outCatalogInfo) const;

public: // 런타임 정책 spec 갱신: 정책 사용 여부와 우선순위를 Python 서버에 반영한다.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool SendPolicySpecUpdateJson(const FString& policySpecUpdateJson);

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool SendDefaultRuntimePolicySpecUpdate();

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool SendRuntimePolicySpecUpdateByPolicyIds(const FString& catalogId, int32 catalogVersion, const TArray<FString>& enabledPolicyIds);

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool SendNormalOnlyRuntimePolicySpecUpdate();

public: // 정책 spec 파일 처리: JSON 파일을 읽거나 읽은 내용을 바로 Python 서버에 보낸다.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool LoadPolicySpecUpdateJsonFile(const FString& policySpecJsonFilePath, FString& outPolicySpecUpdateJson) const;

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool SendPolicySpecUpdateJsonFile(const FString& policySpecJsonFilePath);

public: // 요청 상태 조회: 같은 종류의 HTTP 요청이 중복 전송되지 않도록 확인한다.
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|HttpPolicy")
	bool IsRequestInFlight() const { return bRequestInFlight; }

	UFUNCTION(BlueprintPure, Category = "DeliveryBot|HttpPolicy")
	bool IsGridRequestInFlight() const { return bGridRequestInFlight; }

	UFUNCTION(BlueprintPure, Category = "DeliveryBot|HttpPolicy")
	bool IsEpisodeStartRequestInFlight() const { return bEpisodeStartRequestInFlight; }

	UFUNCTION(BlueprintPure, Category = "DeliveryBot|HttpPolicy")
	bool IsEpisodeConfigUpdateRequestInFlight() const { return bEpisodeConfigUpdateRequestInFlight; }

	UFUNCTION(BlueprintPure, Category = "DeliveryBot|HttpPolicy")
	bool IsPolicyCatalogSourcesRequestInFlight() const { return bPolicyCatalogSourcesRequestInFlight; }

	UFUNCTION(BlueprintPure, Category = "DeliveryBot|HttpPolicy")
	bool IsPolicySpecUpdateRequestInFlight() const { return bPolicySpecUpdateRequestInFlight; }

public:	// 정책 판단 응답 이벤트
	UPROPERTY(BlueprintAssignable, Category = "DeliveryBot|HttpPolicy")
	FDeliveryBotHttpPolicyResponseSignature OnPolicyResponse;

	UPROPERTY(BlueprintAssignable, Category = "DeliveryBot|HttpPolicy")
	FDeliveryBotHttpPolicyParsedResponseSignature OnParsedPolicyResponse;

public:	// 서버 동기화 응답 이벤트
	UPROPERTY(BlueprintAssignable, Category = "DeliveryBot|HttpPolicy")
	FDeliveryBotHttpGridResponseSignature OnGridResponse;

	UPROPERTY(BlueprintAssignable, Category = "DeliveryBot|HttpPolicy")
	FDeliveryBotHttpEpisodeStartResponseSignature OnEpisodeStartResponse;

	UPROPERTY(BlueprintAssignable, Category = "DeliveryBot|HttpPolicy")
	FDeliveryBotHttpEpisodeConfigUpdateResponseSignature OnEpisodeConfigUpdateResponse;

public:	// 정책 catalog 응답 이벤트
	UPROPERTY(BlueprintAssignable, Category = "DeliveryBot|HttpPolicy")
	FDeliveryBotHttpPolicyCatalogSourcesResponseSignature OnPolicyCatalogSourcesResponse;

	UPROPERTY(BlueprintAssignable, Category = "DeliveryBot|HttpPolicy")
	FDeliveryBotHttpPolicyCatalogSourcesParsedResponseSignature OnParsedPolicyCatalogSourcesResponse;

	UPROPERTY(BlueprintAssignable, Category = "DeliveryBot|HttpPolicy")
	FDeliveryBotHttpPolicyCatalogResponseSignature OnPolicyCatalogResponse;

	UPROPERTY(BlueprintAssignable, Category = "DeliveryBot|HttpPolicy")
	FDeliveryBotHttpPolicyCatalogParsedResponseSignature OnParsedPolicyCatalogResponse;

public:	// 정책 spec 갱신 응답 이벤트
	UPROPERTY(BlueprintAssignable, Category = "DeliveryBot|HttpPolicy")
	FDeliveryBotHttpPolicySpecUpdateResponseSignature OnPolicySpecUpdateResponse;


protected: // Python 정책 서버 endpoint 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|HttpPolicy")
	FString PolicyServerUrl{ TEXT("http://127.0.0.1:8000/policy/action") };

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

protected: // HTTP 요청 공통 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|HttpPolicy")
	float RequestTimeoutSecond{ 2.0f };

private:
	// 활성 HTTP 요청 핸들
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveRequest;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveGridRequest;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveEpisodeStartRequest;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveEpisodeConfigUpdateRequest;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActivePolicyCatalogSourcesRequest;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActivePolicyCatalogRequest;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActivePolicySpecUpdateRequest;

private: // 활성 HTTP 요청 취소 처리
	void CancelActiveRequest();
	void CancelActiveGridRequest();
	void CancelActiveEpisodeStartRequest();
	void CancelActiveEpisodeConfigUpdateRequest();
	void CancelActivePolicyCatalogSourcesRequest();
	void CancelActivePolicyCatalogRequest();
	void CancelActivePolicySpecUpdateRequest();
	
private: // 요청 진행 상태
	bool bRequestInFlight{ false };
	bool bGridRequestInFlight{ false };
	bool bEpisodeStartRequestInFlight{ false };
	bool bEpisodeConfigUpdateRequestInFlight{ false };
	bool bPolicyCatalogSourcesRequestInFlight{ false };
	bool bPolicyCatalogRequestInFlight{ false };
	bool bPolicySpecUpdateRequestInFlight{ false };

private: // 종료 중 콜백 무시 플래그
	bool bIsEndingPlay{ false };
	
};