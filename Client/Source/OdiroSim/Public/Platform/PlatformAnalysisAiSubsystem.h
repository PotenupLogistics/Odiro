#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PlatformAnalysisAiSubsystem.generated.h"

class IHttpRequest;
class IHttpResponse;

USTRUCT(BlueprintType)
struct ODIROSIM_API FPlatformAnalysisAiResponse
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Platform|AI")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Platform|AI")
	int32 ResponseCode = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Platform|AI")
	FString DisplayText;

	UPROPERTY(BlueprintReadOnly, Category = "Platform|AI")
	FString ErrorMessage;

	UPROPERTY(BlueprintReadOnly, Category = "Platform|AI")
	FString ResponseBody;

	UPROPERTY(BlueprintReadOnly, Category = "Platform|AI")
	FString ProjectPath;

	UPROPERTY(BlueprintReadOnly, Category = "Platform|AI")
	FString RunId;

	UPROPERTY(BlueprintReadOnly, Category = "Platform|AI")
	FString RunDirectory;

	UPROPERTY(BlueprintReadOnly, Category = "Platform|AI")
	FString ReviewOutputPath;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FPlatformAnalysisAiCompletedNative, const FPlatformAnalysisAiResponse&);

// Platform UI에서 user project run을 결과 분석 AI 서버로 보내는 클라이언트.
UCLASS(BlueprintType, Config = Game)
class ODIROSIM_API UPlatformAnalysisAiSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Command line endpoint override를 적용한다.
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	FPlatformAnalysisAiCompletedNative OnAnalysisCompleted;


	UFUNCTION(BlueprintCallable, Category = "Platform|AI")
	bool RequestAnalysisForProjectRun(const FString& projectPath, const FString& runId);

	UFUNCTION(BlueprintCallable, Category = "Platform|AI")
	void CancelPendingAnalysisRequest();

	UFUNCTION(BlueprintPure, Category = "Platform|AI")
	bool IsAnalysisRequestPending() const { return PendingHttpRequest.IsValid(); }

	static bool BuildAnalysisRequestJsonForProjectRun(
		const FString& projectPath,
		const FString& runId,
		FString& outRequestJson,
		TArray<FString>& outDiagnostics);

	static FString BuildDisplayTextFromAnalysisResponse(
		const FString& responseBody,
		TArray<FString>& outDiagnostics);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Platform|AI")
	FString ProjectRunAnalysisEndpointUrl = TEXT("http://127.0.0.1:8711/api/v2/analysis/run");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Platform|AI", meta = (ClampMin = "1.0"))
	float RequestTimeoutSeconds = 60.0f;

private:
	void HandleAnalysisResponse(
		const FString& requestId,
		TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> httpRequest,
		TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> httpResponse,
		bool bWasSuccessful);

	void BroadcastFailure(
		int32 responseCode,
		const FString& message,
		const FString& responseBody = FString(),
		const FString& projectPath = FString(),
		const FString& runId = FString(),
		const FString& runDirectory = FString(),
		const FString& reviewOutputPath = FString());

	FString PendingRequestId;
	FString PendingProjectPath;
	FString PendingRunId;
	FString PendingRunDirectory;
	FString PendingReviewOutputPath;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> PendingHttpRequest;
};
