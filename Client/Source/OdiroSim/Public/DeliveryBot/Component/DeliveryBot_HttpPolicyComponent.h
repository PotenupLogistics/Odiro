#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HttpFwd.h"
#include "Shared/Struct/DeliveryBot/Drive/DeliveryBotMovementInfo.h"
#include "Shared/Struct/DeliveryBot/Perception/DeliveryBotPointCloudCaptureConfigInfo.h"
#include "Shared/Struct/DeliveryBot/Result/DeliveryBotPythonCaptureRefInfo.h"
#include "Shared/Struct/DeliveryBot/Result/DeliveryBotPolicyDecisionResultInfo.h"
#include "DeliveryBot_HttpPolicyComponent.generated.h"

class FJsonObject;
class FJsonValue;
class UDeliveryBotPythonProcessSubsystem;
struct FDeliveryBotPolicyEventSnapshot;
struct FDeliveryBotObservationInfo;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ODIROSIM_API UDeliveryBot_HttpPolicyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeliveryBot_HttpPolicyComponent();

	void RequestStartScenario();				// Python 서버에 scenario start 요청을 예약한다
	void UpdatePolicy(float deltaTime);			// start 재시도와 decide 반복 요청을 갱신한다
	void EndScenario(
		const FString& status,
		TFunction<void(bool, const FString&)> onComplete); // Python /scenario/end 완료 여부를 반환한다

	void ConfigureProjectActionLogging(const FString& projectOutputEpisodeId); // project actions.jsonl 기록 대상 output episode를 고정한다
	bool ConfigureProjectEpisodeOutput(
		const FString& projectOutputEpisodeId,
		const FString& projectEpisodeOutputDirectory,
		const FString& projectEpisodeOutputRelativeDirectory); // project episode artifact의 절대/상대 출력 루트를 고정한다

public:
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Python")
	bool IsScenarioStarted() const { return bScenarioStarted; } // Python scenario 시작 여부를 반환한다

	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Python")
	FDeliveryBotPolicyDecisionResultInfo GetLastPolicyDecisionResult() const // 마지막 Python policy decision 결과를 반환한다
	{
		return LastPolicyDecisionResult;
	}

	void GetLastPythonCaptureRefs(TArray<FDeliveryBotPythonCaptureRefInfo>& outCaptureRefs) const // 최근 Python capture refs를 복사한다
	{
		outCaptureRefs = LastPolicyDecisionResult.CaptureRefs;
	}

private:
	bool TryStartScenario(); // Python 서버에 /scenario/start 요청을 보낸다
	bool BuildStartPayload(FString& outPayload); // /scenario/start 요청 body를 만든다
	void DrawPythonPathDebug(const TSharedPtr<FJsonObject>& responseObject) const; // Python response.path 좌표를 월드에 그린다
	bool TryParsePythonPathDebugPoint(const TSharedPtr<FJsonValue>& pointValue, FVector& outLocationCm) const; // path debug point JSON을 FVector로 변환한다
	bool BuildDecidePayload(FString& outPayload); // /scenario/decide 요청 body를 만든다
	bool RequestDecision(float deltaTime); // Python 서버에 /scenario/decide 요청을 보낸다
	bool TryParseMoveCommand(const FHttpResponsePtr& response, FDeliveryBotMoveCommandInfo& outMoveCommand); // decide 응답을 이동 명령으로 변환한다
	TArray<FDeliveryBotPythonCaptureRefInfo> BuildPythonCaptureRefs(const TSharedPtr<FJsonObject>& responseObject) const; // Python response.captures를 struct 배열로 변환한다
	FDeliveryBotPolicyDecisionInfo BuildPythonDecisionInfo(const TSharedPtr<FJsonObject>& responseObject) const; // Python response.decision을 struct로 변환한다
	void LogPythonCaptureRefs(const TArray<FDeliveryBotPythonCaptureRefInfo>& captureRefs) const; // Python capture refs를 로그로 남긴다
	void StorePolicyDecisionError(const FString& errorCode, const FString& errorMessage); // 마지막 policy decision을 error 상태로 저장한다
	void EmitPolicyEventSnapshot(const FDeliveryBotPolicyEventSnapshot& snapshot) const; // Python policy event snapshot을 EvaluationSubsystem으로 전달한다
	void EmitPolicyServerFailureEvent(const FString& endpoint, const FHttpResponsePtr& response, const FString& errorCode, const FString& errorMessage, bool bRetryable, bool bTerminalFailure) const; // Python server/HTTP 계층 실패를 평가 이벤트로 전달한다
	void EmitPolicyFailureEvent(const FString& endpoint, const TSharedPtr<FJsonObject>& responseObject, const FString& errorCode, const FString& errorMessage, bool bRetryable, bool bTerminalFailure) const; // Python policy 계약/판단 실패를 평가 이벤트로 전달한다
	void EmitPolicyEventsFromOkResponse(const TSharedPtr<FJsonObject>& responseObject) const; // 정상 policy 응답 안의 의미 있는 이벤트를 평가 이벤트로 전달한다
	bool TryBuildRepathEventSnapshot(const TSharedPtr<FJsonObject>& sourceObject, const TSharedPtr<FJsonObject>& responseObject, FDeliveryBotPolicyEventSnapshot& outSnapshot) const; // RePath 이벤트 snapshot을 만든다


private:
	FDeliveryBotPointCloudCaptureConfigInfo BuildEffectivePointCloudCaptureConfigInfo(const FDeliveryBotPointCloudCaptureConfigInfo& setupPointCloudConfigInfo) const; // setup JSON 우선, 없으면 컴포넌트 기본값으로 Point Cloud 설정을 만든다
	FDeliveryBotPointCloudCaptureConfigInfo SanitizePointCloudCaptureConfigInfo(const FDeliveryBotPointCloudCaptureConfigInfo& pointCloudConfigInfo) const; // Python으로 보내기 전 Point Cloud 설정값을 안전한 값으로 보정한다
	TSharedRef<FJsonObject> BuildPointCloudOptionsObject(const FDeliveryBotPointCloudCaptureConfigInfo& pointCloudConfigInfo) const; // Python point cloud capture 옵션 JSON을 만든다
	void LogPointCloudStartConfig(const FDeliveryBotPointCloudCaptureConfigInfo& pointCloudConfigInfo) const; // /scenario/start에 전달되는 Point Cloud 설정을 로그로 남긴다
	TSharedRef<FJsonObject> BuildArtifactSpecObject() const; // Python capture artifact 저장 경로 JSON을 만든다
	FString BuildPointCloudCaptureRootDirectory() const; // Point Cloud capture 저장 루트 경로를 만든다
	FString BuildPointCloudCaptureRunId(const FDeliveryBotObservationInfo& observation) const; // 사람이 읽기 쉬운 point cloud capture run id를 만든다
	bool BuildEndPayload(const FString& status, FString& outPayload) const; // /scenario/end 요청 body를 만든다

private:
	// Python 서버에 POST 요청을 보내고 선택적으로 timeout을 적용한다.
	bool SendPostRequest(
		const FString& endpoint,
		const FString& payload,
		TFunction<void(FHttpResponsePtr, bool)> onComplete,
		float timeoutSeconds = 0.f);
	FString ResolveProjectEpisodeId() const; // project run의 현재 output episode id를 가져온다
	void WriteProjectActionRecord(
		const FString& projectEpisodeId,
		const TSharedPtr<FJsonObject>& requestObject,
		const FHttpResponsePtr& response,
		bool bActionSucceeded) const; // project actions.jsonl에 decide 결과를 기록한다

	TSharedRef<FJsonObject> BuildLocationObject(const FVector& location, float yawDegree = 0.f) const; // 위치 JSON 객체를 만든다
	bool BuildPythonGridObject(TSharedPtr<FJsonObject>& outGridObject) const; // Python 서버용 grid JSON 객체를 만든다
	bool BuildMessagePayload(const FString& messageType, const TSharedRef<FJsonObject>& requestObject, FString& outPayload) const; // request 객체를 Python message envelope로 감싼다
	void ResetScenarioState(); // scenario 진행 상태를 초기화한다
	bool TryGetPythonResponseObject(const FHttpResponsePtr& response, TSharedPtr<FJsonObject>& outResponseObject) const; // envelope 응답에서 response 객체를 가져온다
	bool IsPythonResponseOk(const FHttpResponsePtr& response) const; // envelope 응답의 response.status가 ok인지 확인한다
	UDeliveryBotPythonProcessSubsystem* GetPythonProcessSubsystem() const; // Python process subsystem을 가져온다

private:
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Python")
	float StartRetryIntervalSeconds{ 0.5f }; // Python start 재시도 간격

	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Python")
	float DecideIntervalSeconds{ 0.1f }; // Python decide 요청 간격

	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Python")
	bool bSendFullLidarRaysToPythonPolicy{ false }; // /scenario/decide payload에 정책용 ray만 보낼지 전체 ray를 보낼지 결정

	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Python|Debug")
	bool bDrawPythonPathDebug{ true }; // Python path debug draw 표시 여부

	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Python|Debug")
	float PythonPathDebugHeightCm{ 40.f }; // Python path debug draw 높이 보정

	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Python|Debug")
	float PythonPathDebugLineThickness{ 5.f }; // Python path debug draw 선 두께

private:
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Python|Capture")
	FString PythonObservationProfile{ TEXT("point_cloud_capture") }; // setup JSON에 Point Cloud 설정이 없을 때 사용할 legacy observation profile

	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Python|Capture")
	bool bEnablePythonPointCloudCapture{ true }; // setup JSON에 Point Cloud 설정이 없을 때 사용할 legacy capture 저장 여부

	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Python|Capture")
	bool bLogPythonPointCloudStartConfig{ false }; // /scenario/start에 전달되는 최종 Point Cloud 설정 로그 출력 여부

	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Python|Capture", meta = (ClampMin = "0"))
	int32 PythonPointCloudScenarioNumber{ 1 }; // point cloud capture 폴더에 사용할 시나리오 번호

	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Python|Capture", meta = (ClampMin = "1", UIMin = "1"))
	int32 PythonPointCloudCaptureEveryNSensorFrames{ 10 }; // setup JSON에 Point Cloud 설정이 없을 때 사용할 legacy 저장 간격

	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Python|Capture", meta = (ClampMin = "1", UIMin = "1"))
	int32 PythonPointCloudMaxPoints{ 4096 }; // setup JSON에 Point Cloud 설정이 없을 때 사용할 legacy frame당 최대 point 수

	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Python|Capture", meta = (ClampMin = "0", UIMin = "0"))
	float PythonPointCloudRangeLimitM{ 0.f }; // setup JSON에 Point Cloud 설정이 없을 때 사용할 legacy 거리 제한

	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Python|Capture")
	bool bPythonPointCloudIncludeGroundPoints{ true }; // setup JSON에 Point Cloud 설정이 없을 때 사용할 legacy ground 저장 여부


private:
	FDeliveryBotPolicyDecisionResultInfo LastPolicyDecisionResult; // 마지막 Python decide 결과와 capture refs

private:
	FString EpisodeId; // 현재 Python scenario/capture run id
	FString ProjectActionEpisodeId; // runner가 지정한 user project output episode id
	bool bProjectEpisodeOutputRequired{ false }; // project run에서 Python artifact 출력 경로가 필수인지 여부
	FString ProjectEpisodeOutputDirectory; // Python artifact를 저장할 project episode 절대 경로
	FString ProjectEpisodeOutputRelativeDirectory; // run 폴더 기준 project episode 상대 경로
	FString ProjectEpisodeOutputErrorCode; // /scenario/start로 전달할 project episode 경로 설정 오류 코드
	FString ProjectEpisodeOutputErrorMessage; // /scenario/start로 전달할 project episode 경로 설정 오류 설명
	FString RobotInstanceId; // Python payload에 사용하는 robot instance id
	int32 LastDecisionSequence{ 0 }; // 마지막 decide 요청 sequence
	TSharedPtr<FJsonObject> LastDecisionRequestObject;

	float StartRetryElapsedSeconds{ 0.f }; // start 재시도 누적 시간
	float DecideElapsedSeconds{ 0.f }; // decide 요청 누적 시간
	float LastDecisionRunTimeSeconds{ 0.f }; // 마지막 decide 요청 runtime
	
	bool bStartRequested{ false };
	bool bScenarioStarted{ false };
	bool bStartRequestInFlight{ false };
	bool bDecisionRequestInFlight{ false };
	bool bEndRequestInFlight{ false };
	bool bLoggedStartWaitingForPython{ false }; // Python 서버 준비 대기 로그 중복 방지

	// Python /scenario/end 요청 자체의 최대 대기 시간이다.
	static constexpr float EndRequestTimeoutSeconds = 2.f;
};
