#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ScenarioLlmAuthoringSubsystem.generated.h"

class IHttpRequest;

USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FScenarioLlmGenerationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|LLM")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|LLM")
	int32 HttpStatusCode = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|LLM")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|LLM")
	FString SavedRunQueueJsonPath;

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|LLM")
	FString ResolvedSavedRunQueueJsonPath;

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|LLM")
	FString FirstScenarioSetupJsonPath;

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|LLM")
	FString FirstDeliveryBotSetupJsonPath;

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|LLM")
	int32 RunCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|LLM")
	TArray<FString> Diagnostics;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FScenarioLlmGenerationCompletedSignature, const FScenarioLlmGenerationResult&, Result);

// 시나리오 에디터에서 AI 서버로 자연어 기반 시나리오 생성 요청을 보내고 ScenarioSetup JSON을 받는 책임.
UCLASS(BlueprintType)
class PROTOROBOTSIM_API UScenarioLlmAuthoringSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|LLM")
	FString BaseUrl = TEXT("http://127.0.0.1:8711");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|LLM")
	FString GenerateEndpoint = TEXT("/api/v1/scenarios/generate");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|LLM")
	FString LatestRunQueueJsonPath = TEXT("Json/Input/EpisodeRunQueue_LlmLatest.json");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|LLM", meta = (ClampMin = "1", ClampMax = "20"))
	int32 DefaultEpisodeCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|LLM", meta = (ClampMin = "1.0"))
	float RequestTimeoutSeconds = 120.0f;

	UPROPERTY(BlueprintAssignable, Category = "Scenario|LLM")
	FScenarioLlmGenerationCompletedSignature OnGenerationCompleted;

	UFUNCTION(BlueprintCallable, Category = "Scenario|LLM")
	bool GenerateEpisodeFromPrompt(const FString& prompt, int32 episodeCount);

	UFUNCTION(BlueprintCallable, Category = "Scenario|LLM")
	void CancelPendingRequest();

	UFUNCTION(BlueprintPure, Category = "Scenario|LLM")
	bool IsRequestPending() const { return PendingHttpRequest.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Scenario|LLM")
	FScenarioLlmGenerationResult GetLatestResult() const { return LatestResult; }

	UFUNCTION(BlueprintPure, Category = "Scenario|LLM")
	FString GetLatestRunQueueJsonPath() const { return LatestRunQueueJsonPath; }

private:
	void HandleGenerateResponse(int32 responseCode, const FString& responseBody, bool bWasSuccessful);
	void CompleteRequest(const FScenarioLlmGenerationResult& result);
	bool TryBuildRequestBody(const FString& prompt, int32 episodeCount, FString& outBody, FScenarioLlmGenerationResult& outFailure) const;
	bool TryValidateAndSaveRunQueue(
		const FString& responseBody,
		int32 responseCode,
		FScenarioLlmGenerationResult& outResult) const;

	static FString BuildUrl(const FString& baseUrl, const FString& endpoint);
	static FString TruncateForDiagnostic(const FString& value);

	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> PendingHttpRequest;

	UPROPERTY(Transient)
	FScenarioLlmGenerationResult LatestResult;
};
