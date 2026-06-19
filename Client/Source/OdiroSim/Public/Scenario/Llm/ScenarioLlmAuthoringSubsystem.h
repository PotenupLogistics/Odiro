#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ScenarioLlmAuthoringSubsystem.generated.h"

class IHttpRequest;

USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioLlmGenerationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|LLM")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|LLM")
	int32 HttpStatusCode = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|LLM")
	FString Message;

	// User project scenario.json path produced or targeted by the generation request.
	UPROPERTY(BlueprintReadOnly, Category = "Scenario|LLM")
	FString ProjectScenarioJsonPath;

	// Episode count requested for the next project run.
	UPROPERTY(BlueprintReadOnly, Category = "Scenario|LLM")
	int32 EpisodeCount = 0;

	// Optional run id returned by a future AI service that also launches analysis or generation jobs.
	UPROPERTY(BlueprintReadOnly, Category = "Scenario|LLM")
	FString RunId;

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|LLM")
	TArray<FString> Diagnostics;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FScenarioLlmGenerationCompletedSignature, const FScenarioLlmGenerationResult&, Result);

// Owns natural-language scenario generation requests for the editor's user project scenario.json flow.
UCLASS(BlueprintType)
class ODIROSIM_API UScenarioLlmAuthoringSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Cancels any outstanding HTTP request owned by this subsystem.
	virtual void Deinitialize() override;

	// Base URL of the AI authoring service.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|LLM")
	FString BaseUrl = TEXT("http://127.0.0.1:8711");

	// Scenario generation endpoint path under BaseUrl.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|LLM")
	FString GenerateEndpoint = TEXT("/api/v1/scenarios/generate");

	// Default project episode count requested when the prompt UI leaves it blank.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|LLM", meta = (ClampMin = "1", ClampMax = "20"))
	int32 DefaultEpisodeCount = 1;

	// Timeout for one generation HTTP request.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|LLM", meta = (ClampMin = "1.0"))
	float RequestTimeoutSeconds = 120.0f;

	// Broadcasts after a generation request succeeds, fails, or is rejected locally.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|LLM")
	FScenarioLlmGenerationCompletedSignature OnGenerationCompleted;

	// Sends an AI generation request scoped to a user project scenario.json.
	UFUNCTION(BlueprintCallable, Category = "Scenario|LLM")
	bool GenerateProjectScenarioFromPrompt(
		const FString& prompt,
		const FString& projectScenarioJsonPath,
		int32 episodeCount);

	// Cancels the in-flight request without broadcasting a completion result.
	UFUNCTION(BlueprintCallable, Category = "Scenario|LLM")
	void CancelPendingRequest();

	// True while an HTTP generation request is in flight.
	UFUNCTION(BlueprintPure, Category = "Scenario|LLM")
	bool IsRequestPending() const { return PendingHttpRequest.IsValid(); }

	// Most recent local or HTTP generation result.
	UFUNCTION(BlueprintPure, Category = "Scenario|LLM")
	FScenarioLlmGenerationResult GetLatestResult() const { return LatestResult; }

private:
	// Converts one HTTP response into the minimal project scenario result contract.
	void HandleGenerateResponse(int32 responseCode, const FString& responseBody, bool bWasSuccessful);
	// Stores and broadcasts one completed request result.
	void CompleteRequest(const FScenarioLlmGenerationResult& result);
	// Serializes the request body expected by the project scenario generation bridge.
	bool TryBuildRequestBody(
		const FString& prompt,
		const FString& projectScenarioJsonPath,
		int32 episodeCount,
		FString& outBody,
		FScenarioLlmGenerationResult& outFailure) const;

	// Joins BaseUrl and endpoint without duplicating separators.
	static FString BuildUrl(const FString& baseUrl, const FString& endpoint);
	// Truncates large response bodies for diagnostics.
	static FString TruncateForDiagnostic(const FString& value);
	// Reads the project scenario path from a generation response without owning the full AI schema.
	static bool TryReadScenarioPathFromResponse(const FString& responseBody, FString& outScenarioJsonPath, FString& outMessage, FString& outRunId);
	// Resolves a project scenario path against the UE project directory when needed.
	static FString ResolveProjectScenarioJsonPath(const FString& projectScenarioJsonPath);
	// True when a path targets the user project scenario.json file.
	static bool IsProjectScenarioJsonPath(const FString& projectScenarioJsonPath);

	// In-flight HTTP request owned by this subsystem.
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> PendingHttpRequest;

	// Most recent local or HTTP generation result.
	UPROPERTY(Transient)
	FScenarioLlmGenerationResult LatestResult;

	// Episode count associated with the current in-flight request.
	UPROPERTY(Transient)
	int32 PendingEpisodeCount = 0;
};
