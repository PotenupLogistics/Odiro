#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HttpFwd.h"
#include "Shared/Struct/DeliveryBot/Drive/DeliveryBotMovementInfo.h"
#include "Shared/Struct/DeliveryBot/Result/DeliveryBotPythonCaptureRefInfo.h"
#include "Shared/Struct/DeliveryBot/Result/DeliveryBotPolicyDecisionResultInfo.h"
#include "DeliveryBot_HttpPolicyComponent.generated.h"

class FJsonObject;
class FJsonValue;
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
	
	// 마지막 Python policy decision 결과를 반환한다
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Python") 
	FDeliveryBotPolicyDecisionResultInfo GetLastPolicyDecisionResult() const 
	{
		return LastPolicyDecisionResult;
	} 

	void GetLastPythonCaptureRefs(TArray<FDeliveryBotPythonCaptureRefInfo>& outCaptureRefs) const // 최근 Python capture refs를 복사한다
	{	
		outCaptureRefs = LastPolicyDecisionResult.CaptureRefs;
	}
	
	
private:
	bool TryStartScenario();							// Python 서버에 /scenario/start 요청을 보낸다
	bool BuildStartPayload(FString& outPayload);		// /scenario/start 요청 body를 만든다

private:
	void DrawPythonPathDebug(const TSharedPtr<FJsonObject>& responseObject) const; // Python response.path 좌표를 월드에 그린다.
	bool TryParsePythonPathDebugPoint(const TSharedPtr<FJsonValue>& pointValue, FVector& outLocationCm) const; // path debug point JSON을 FVector로 변환한다
	bool BuildDecidePayload(FString& outPayload);		// /scenario/decide 요청 body를 만든다
	bool RequestDecision(float deltaTime);				// Python 서버에 /scenario/decide 요청을 보낸다
	// decide 응답을 이동 명령으로 변환한다
	bool TryParseMoveCommand(const FHttpResponsePtr& response, FDeliveryBotMoveCommandInfo& outMoveCommand);
	TArray<FDeliveryBotPythonCaptureRefInfo> BuildPythonCaptureRefs(const TSharedPtr<FJsonObject>& responseObject) const; // Python response.captures를 struct 배열로 변환한다
	FDeliveryBotPolicyDecisionInfo BuildPythonDecisionInfo(const TSharedPtr<FJsonObject>& responseObject) const; // Python response.decision을 struct로 변환한다
	void LogPythonCaptureRefs(const TArray<FDeliveryBotPythonCaptureRefInfo>& captureRefs) const; // Python capture refs를 로그로 남긴다
	void StorePolicyDecisionError(const FString& errorCode, const FString& errorMessage); // 마지막 policy decision을 error 상태로 저장한다

private:
	TSharedRef<FJsonObject> BuildPointCloudOptionsObject() const; // Python point cloud capture 옵션 JSON을 만든다
	TSharedRef<FJsonObject> BuildArtifactSpecObject() const; // Python capture artifact 저장 경로 JSON을 만든다
	bool BuildEndPayload(const FString& status, FString& outPayload) const; // /scenario/end 요청 body를 만든다

private:
	// Python 서버에 POST 요청을 보낸다
	bool SendPostRequest(const FString& endpoint, const FString& payload, TFunction<void(FHttpResponsePtr, bool)> onComplete);
	FString ResolveProjectEpisodeId() const; // project run의 현재 output episode id를 가져온다
	void WriteProjectActionRecord(
		const FString& projectEpisodeId,
		const TSharedPtr<FJsonObject>& requestObject,
		const FHttpResponsePtr& response,
		bool bActionSucceeded) const; // project actions.jsonl에 decide 결과를 기록한다

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
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Python|Capture")
	FString PythonObservationProfile{ TEXT("point_cloud_capture") }; // Python LiDAR observation profile 이름

	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Python|Capture")
	bool bEnablePythonPointCloudCapture{ true }; // Python point cloud capture 저장 여부

	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Python|Capture")
	int32 PythonPointCloudCaptureEveryNSensorFrames{ 10 }; // point cloud capture 저장 sensor frame 간격

	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Python|Capture")
	int32 PythonPointCloudMaxPoints{ 4096 }; // point cloud frame당 최대 저장 point 수

	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Python|Capture")
	bool bPythonPointCloudIncludeGroundPoints{ true }; // tag 없는 ground point 저장 여부

	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Python|Capture")
	float PythonPointCloudRangeLimitM{ 0.f }; // 0보다 크면 point cloud 저장 거리 제한으로 사용

private:
	FDeliveryBotPolicyDecisionResultInfo LastPolicyDecisionResult; // 마지막 Python decide 결과와 capture refs
	
private:
	FString EpisodeId;
	FString ProjectActionEpisodeId; // runner가 지정한 user project output episode id
	FString RobotInstanceId;
	FString LastScenarioResultJson;

	int32 LastDecisionSequence{ 0 };
	TSharedPtr<FJsonObject> LastDecisionRequestObject;

	float StartRetryElapsedSeconds{ 0.f };
	float DecideElapsedSeconds{ 0.f };
	float LastDecisionRunTimeSeconds{ 0.f }; // 마지막 decide 요청 runtime
	
	bool bStartRequested{ false };
	bool bScenarioStarted{ false };
	bool bStartRequestInFlight{ false };
	bool bDecisionRequestInFlight{ false };
	bool bEndRequestInFlight{ false };
	bool bLoggedStartWaitingForPython{ false }; // Python 서버 준비 대기 로그 중복 방지
};
