#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ScenarioLlmAuthoringSubsystem.generated.h"

class IHttpRequest;

USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioLlmGenerationResult
{
	GENERATED_BODY()

	// AI scenario 생성과 저장이 모두 성공했으면 true.
	UPROPERTY(BlueprintReadOnly, Category = "Scenario|LLM")
	bool bSuccess = false;

	// Agents HTTP 응답 코드.
	UPROPERTY(BlueprintReadOnly, Category = "Scenario|LLM")
	int32 HttpStatusCode = 0;

	// 사용자에게 보여줄 요약 메시지.
	UPROPERTY(BlueprintReadOnly, Category = "Scenario|LLM")
	FString Message;

	// 저장된 user project scenario.json 경로.
	UPROPERTY(BlueprintReadOnly, Category = "Scenario|LLM")
	FString SavedScenarioJsonPath;

	// 절대 경로로 정규화한 SavedScenarioJsonPath.
	UPROPERTY(BlueprintReadOnly, Category = "Scenario|LLM")
	FString ResolvedSavedScenarioJsonPath;

	// 생성된 scenario_id.
	UPROPERTY(BlueprintReadOnly, Category = "Scenario|LLM")
	FString ScenarioId;

	// 검증과 저장 중 발견한 메시지.
	UPROPERTY(BlueprintReadOnly, Category = "Scenario|LLM")
	TArray<FString> Diagnostics;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FScenarioLlmGenerationCompletedSignature, const FScenarioLlmGenerationResult&, Result);

// AI 서버로 자연어 기반 scenario 생성을 요청하고 user project scenario.json으로 저장하는 책임.
UCLASS(BlueprintType)
class ODIROSIM_API UScenarioLlmAuthoringSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	// Agents 서버 base URL.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|LLM")
	FString BaseUrl = TEXT("http://127.0.0.1:8711");

	// Project scenario v2 생성 endpoint.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|LLM")
	FString GenerateEndpoint = TEXT("/api/v2/scenarios/generate");

	// scenario.json을 저장할 사용자 project root.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|LLM")
	FString TargetProjectPath;

	// 기존 UI count 입력과의 호환용 기본값. v2는 항상 scenario 하나를 생성한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|LLM", meta = (ClampMin = "1", ClampMax = "20"))
	int32 DefaultScenarioCount = 1;

	// HTTP 요청 timeout.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|LLM", meta = (ClampMin = "1.0"))
	float RequestTimeoutSeconds = 120.0f;

	UPROPERTY(BlueprintAssignable, Category = "Scenario|LLM")
	FScenarioLlmGenerationCompletedSignature OnGenerationCompleted;

	UFUNCTION(BlueprintCallable, Category = "Scenario|LLM")
	bool GenerateScenariosFromPrompt(const FString& prompt, int32 scenarioCount);

	UFUNCTION(BlueprintCallable, Category = "Scenario|LLM")
	void SetTargetProjectPath(const FString& projectPath);

	UFUNCTION(BlueprintCallable, Category = "Scenario|LLM")
	void CancelPendingRequest();

	UFUNCTION(BlueprintPure, Category = "Scenario|LLM")
	bool IsRequestPending() const { return PendingHttpRequest.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Scenario|LLM")
	FScenarioLlmGenerationResult GetLatestResult() const { return LatestResult; }

	UFUNCTION(BlueprintPure, Category = "Scenario|LLM")
	FString GetLatestScenarioJsonPath() const { return LatestResult.SavedScenarioJsonPath; }

	UFUNCTION(BlueprintPure, Category = "Scenario|LLM")
	FString GetResolvedTargetProjectPath() const;

private:
	void HandleGenerateResponse(int32 responseCode, const FString& responseBody, bool bWasSuccessful);
	void CompleteRequest(const FScenarioLlmGenerationResult& result);
	bool TryBuildRequestBody(const FString& prompt, int32 scenarioCount, FString& outBody, FScenarioLlmGenerationResult& outFailure) const;
	bool TryValidateAndSaveScenario(
		const FString& responseBody,
		int32 responseCode,
		FScenarioLlmGenerationResult& outResult) const;
	FString ResolveTargetProjectPath() const;
	FString ResolveTargetScenarioPath() const;

	static FString BuildUrl(const FString& baseUrl, const FString& endpoint);
	static FString TruncateForDiagnostic(const FString& value);
	static FString NormalizePath(FString path);

	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> PendingHttpRequest;

	UPROPERTY(Transient)
	FScenarioLlmGenerationResult LatestResult;
};
