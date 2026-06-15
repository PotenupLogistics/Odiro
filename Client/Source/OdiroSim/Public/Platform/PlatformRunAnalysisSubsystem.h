#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PlatformRunAnalysisSubsystem.generated.h"

class IHttpRequest;
class IHttpResponse;

// HTTP completion payload for an AI analysis request built from canonical run artifacts.
USTRUCT(BlueprintType)
struct ODIROSIM_API FPlatformRunAnalysisResponse
{
	GENERATED_BODY()

	// True when the HTTP request completed with a 2xx response and the response was formatted for display.
	UPROPERTY(BlueprintReadOnly, Category = "Platform|AI")
	bool bSuccess = false;

	// HTTP status code returned by the analysis service, or zero for local request failures.
	UPROPERTY(BlueprintReadOnly, Category = "Platform|AI")
	int32 ResponseCode = 0;

	// User-facing text rendered in the MainMenu analysis panel.
	UPROPERTY(BlueprintReadOnly, Category = "Platform|AI")
	FString DisplayText;

	// Local or remote failure message shown when bSuccess is false.
	UPROPERTY(BlueprintReadOnly, Category = "Platform|AI")
	FString ErrorMessage;

	// Raw HTTP response body retained for diagnostics and fallback display.
	UPROPERTY(BlueprintReadOnly, Category = "Platform|AI")
	FString ResponseBody;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FPlatformRunAnalysisCompletedNative, const FPlatformRunAnalysisResponse&);

// Sends run_summary, episode_result, and episode_event artifacts to the platform AI analysis service.
UCLASS(BlueprintType, Config = Game)
class ODIROSIM_API UPlatformRunAnalysisSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Cancels any active HTTP request and releases native delegates during GameInstance shutdown.
	virtual void Deinitialize() override;

	// Native completion delegate used by platform widgets that do not need Blueprint assignment.
	FPlatformRunAnalysisCompletedNative OnAnalysisCompleted;

	// Builds a request from an episode result path and submits it to the configured analysis endpoint.
	UFUNCTION(BlueprintCallable, Category = "Platform|AI")
	bool RequestAnalysisForEpisodeResult(const FString& episodeResultPath);

	// Cancels the active analysis request if it is still pending.
	UFUNCTION(BlueprintCallable, Category = "Platform|AI")
	void CancelPendingAnalysisRequest();

	// Returns true while an HTTP request is owned by this subsystem.
	UFUNCTION(BlueprintPure, Category = "Platform|AI")
	bool IsAnalysisRequestPending() const { return PendingHttpRequest.IsValid(); }

	// Builds the AI request JSON from result.json plus inferred summary.json and events.jsonl siblings.
	static bool BuildAnalysisRequestJsonFromEpisodeResult(
		const FString& episodeResultPath,
		bool bFallbackOnly,
		FString& outRequestJson,
		TArray<FString>& outDiagnostics);

	// Converts the AI service response JSON into compact text for the MainMenu analysis panel.
	static FString BuildDisplayTextFromAnalysisResponse(
		const FString& responseBody,
		TArray<FString>& outDiagnostics);

	// HTTP endpoint that receives run_analysis_request payloads.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Platform|AI")
	FString AnalysisEndpointUrl = TEXT("http://127.0.0.1:8711/api/v1/analysis/run");

	// Per-request timeout in seconds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Platform|AI", meta = (ClampMin = "1.0"))
	float RequestTimeoutSeconds = 60.0f;

	// True requests deterministic fallback-only analysis from the AI service.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Platform|AI")
	bool bFallbackOnly = false;

private:
	// Handles the HTTP callback while ignoring stale completions from cancelled requests.
	void HandleAnalysisResponse(
		const FString& requestId,
		TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> httpRequest,
		TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> httpResponse,
		bool bWasSuccessful);

	// Broadcasts a local failure or HTTP error through the shared completion delegate.
	void BroadcastFailure(int32 responseCode, const FString& message, const FString& responseBody = FString());

	// Request identifier used to reject late callbacks after cancellation.
	FString PendingRequestId;

	// Owned HTTP request while a platform AI analysis call is in flight.
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> PendingHttpRequest;
};
