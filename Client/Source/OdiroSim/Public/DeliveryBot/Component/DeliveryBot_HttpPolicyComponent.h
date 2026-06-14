#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HttpFwd.h"
#include "Shared/Struct/DeliveryBot/Drive/DeliveryBotMovementInfo.h"
#include "DeliveryBot_HttpPolicyComponent.generated.h"

class FJsonObject;
class UDeliveryBotPythonProcessSubsystem;
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ODIROSIM_API UDeliveryBot_HttpPolicyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeliveryBot_HttpPolicyComponent();

	void RequestStartScenario();				// Python 서버에 scenario start 요청을 예약한다
	void UpdatePolicy(float deltaTime);			// start 재시도와 decide 반복 요청을 갱신한다
	void EndScenario(const FString& status);	// Python 서버에 scenario end 요청을 보낸다


public:
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Python")
	bool IsScenarioStarted() const { return bScenarioStarted; } // Python scenario 시작 여부를 반환한다

	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Python")
	FString GetLastScenarioResultJson() const { return LastScenarioResultJson; } // 마지막 scenario result JSON을 반환한다

private:
	bool TryStartScenario();							// Python 서버에 /scenario/start 요청을 보낸다
	bool BuildStartPayload(FString& outPayload);		// /scenario/start 요청 body를 만든다

private:
	// decide 응답을 이동 명령으로 변환한다
	bool TryParseMoveCommand(const FHttpResponsePtr& response, FDeliveryBotMoveCommandInfo& outMoveCommand) const;
	void DrawPythonPathDebug(const TSharedPtr<FJsonObject>& responseObject) const; // Python path debug 좌표를 월드에 그린다
	bool TryParsePythonPathDebugPoint(const TSharedPtr<FJsonValue>& pointValue, FVector& outLocationCm) const; // path debug point JSON을 FVector로 변환한다
	bool BuildDecidePayload(FString& outPayload);		// /scenario/decide 요청 body를 만든다
	bool RequestDecision(float deltaTime);				// Python 서버에 /scenario/decide 요청을 보낸다


private:
	bool BuildEndPayload(const FString& status, FString& outPayload) const; // /scenario/end 요청 body를 만든다

private:
	// Python 서버에 POST 요청을 보낸다
	bool SendPostRequest(const FString& endpoint, const FString& payload, TFunction<void(FHttpResponsePtr, bool)> onComplete);

	TSharedRef<FJsonObject> BuildLocationObject(const FVector& location, float yawDegree = 0.f) const; // 위치 JSON 객체를 만든다
	bool BuildPythonGridObject(TSharedPtr<FJsonObject>& outGridObject) const; // Python 서버용 grid JSON 객체를 만든다
	bool BuildMessagePayload(const FString& messageType, const TSharedRef<FJsonObject>& requestObject, FString& outPayload) const; // request 객체를 Python message envelope으로 감싼다
	void ResetScenarioState(bool bKeepLastResult); // scenario 진행 상태를 초기화한다
	bool TryGetPythonResponseObject(const FHttpResponsePtr& response, TSharedPtr<FJsonObject>& outResponseObject) const; // envelope 응답에서 response 객체를 가져온다
	bool IsPythonResponseOk(const FHttpResponsePtr& response) const; // envelope 응답의 response.status가 ok인지 확인한다

	UDeliveryBotPythonProcessSubsystem* GetPythonProcessSubsystem() const; // Python process subsystem을 가져온다


private:
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Python")
	float StartRetryIntervalSeconds{ 0.5f };

	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Python")
	float DecideIntervalSeconds{ 0.1f };

	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Python|Debug")
	bool bDrawPythonPathDebug{ true };

	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Python|Debug")
	float PythonPathDebugHeightCm{ 40.f };

	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Python|Debug")
	float PythonPathDebugLineThickness{ 5.f };


private:
	FString EpisodeId;
	FString RobotInstanceId;
	FString LastScenarioResultJson;

	int32 LastDecisionSequence{ 0 };

	float StartRetryElapsedSeconds{ 0.f };
	float DecideElapsedSeconds{ 0.f };

	bool bStartRequested{ false };
	bool bScenarioStarted{ false };
	bool bStartRequestInFlight{ false };
	bool bDecisionRequestInFlight{ false };
	bool bEndRequestInFlight{ false };
};
