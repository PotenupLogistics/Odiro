#pragma once

#include "CoreMinimal.h"
#include "ProjectRunResultDashboard.generated.h"

// AI 개선 제안 row가 사용하는 표시 심각도.
UENUM(BlueprintType)
enum class EProjectRunAiSuggestionSeverity : uint8
{
	Info,
	Low,
	Medium,
	High,
};

// Result dashboard의 episode replay 카드 하나에 필요한 데이터.
USTRUCT(BlueprintType)
struct ODIROSIM_API FProjectRunEpisodeDashboardItem
{
	GENERATED_BODY()

	// 6자리 episode id.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	FString EpisodeId;

	// episode 실행 시간 초 단위.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	double DurationSeconds = 0.0;

	// API 응답 display.duration이 제공하는 episode 시간 표시 문자열.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	FString DurationLabel;

	// episode가 성공으로 판정되었는지 여부.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	bool bSuccess = false;

	// summary.json의 outcome 문자열.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	FString Outcome;

	// API 응답 display.outcome이 제공하는 outcome 표시 문자열.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	FString OutcomeLabel;

	// summary.json의 terminal_reason 문자열.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	FString TerminalReason;

	// episode별 주요 충돌 카운트 합계.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	int32 CollisionCount = 0;

	// Episode replay artifact가 저장된 episode directory 경로.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	FString EpisodeDirectory;

	// True when episode/replay/replay.meta.json and episode/replay/replay.frames.bin exist.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	bool bReplayAvailable = false;

	// 존재할 경우 episode preview.png 절대 경로.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	FString PreviewImagePath;
};

// Result dashboard의 AI 제안 row 하나에 필요한 데이터.
USTRUCT(BlueprintType)
struct ODIROSIM_API FProjectRunAiSuggestionDashboardItem
{
	GENERATED_BODY()

	// 표시 심각도.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	EProjectRunAiSuggestionSeverity Severity = EProjectRunAiSuggestionSeverity::Info;

	// 사용자에게 표시할 심각도 label.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	FString SeverityLabel;

	// AI 응답의 제안 제목.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	FString Title;

	// AI 응답의 자유 형식 제안 본문.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	FString Message;

	// AI 응답의 제안 사유.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	FString Reason;

	// AI 응답의 권장 조치.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	FString Recommendation;

	// AI 응답이 가리키는 설정 parameter 이름.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	FString ParameterName;

	// AI 응답의 현재 parameter 값.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	FString CurrentValue;

	// AI 응답의 제안 parameter 값.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	FString SuggestedValue;
};

// Result dashboard의 핵심 분석 포인트 card 하나에 필요한 데이터.
USTRUCT(BlueprintType)
struct ODIROSIM_API FProjectRunAnalysisInsightDashboardItem
{
	GENERATED_BODY()

	// 표시 심각도.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	EProjectRunAiSuggestionSeverity Severity = EProjectRunAiSuggestionSeverity::Info;

	// 사용자에게 표시할 심각도 label.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	FString SeverityLabel;

	// 분석 포인트 제목.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	FString Title;

	// 분석 포인트 설명.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	FString Description;
};

// Result dashboard 화면 하나를 채우는 집계 데이터.
USTRUCT(BlueprintType)
struct ODIROSIM_API FProjectRunResultDashboardData
{
	GENERATED_BODY()

	// summary.json을 성공적으로 읽었는지 여부.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	bool bSummaryLoaded = false;

	// review JSON을 성공적으로 읽었는지 여부.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	bool bAiLoaded = false;

	// dashboard가 나타내는 run id.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	FString RunId;

	// 모든 episode duration 합계.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	double TotalDurationSeconds = 0.0;

	// run_overview.display.total_play_time 또는 summary 기반 표시 문자열.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	FString TotalPlayTimeLabel;

	// summary rows 기준 episode 수.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	int32 EpisodeCount = 0;

	// 성공 episode 수.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	int32 SuccessCount = 0;

	// run_overview.display.success_rate 또는 summary 기반 표시 문자열.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	FString SuccessRateLabel;

	// 주요 충돌 카운트 합계.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	int32 CollisionCount = 0;

	// run_overview.display.collision_count 또는 summary 기반 표시 문자열.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	FString CollisionCountLabel;

	// AI 분석 요약 문장.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	FString AiSummary;

	// episode replay card 데이터.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	TArray<FProjectRunEpisodeDashboardItem> Episodes;

	// AI 개선 제안 row 데이터.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	TArray<FProjectRunAiSuggestionDashboardItem> Suggestions;

	// 핵심 분석 포인트 row 데이터.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	TArray<FProjectRunAnalysisInsightDashboardItem> Insights;

	// AI 분석 중 발생한 비치명 경고 표시 문자열.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	TArray<FString> Warnings;

	// JSON 읽기/해석 중 발생한 비치명 진단.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ProjectRun")
	TArray<FString> Diagnostics;
};

// Project run result JSON을 dashboard view-model로 변환한다.
struct ODIROSIM_API FProjectRunResultDashboardJson
{
	// run directory의 summary/review JSON에서 dashboard 데이터를 만든다.
	static bool BuildFromRunDirectory(
		const FString& runDirectory,
		FProjectRunResultDashboardData& outDashboardData);

	// summary.json 문자열에서 run/episode 집계를 만든다.
	static bool BuildFromSummaryJsonString(
		const FString& summaryJson,
		const FString& runDirectory,
		FProjectRunResultDashboardData& outDashboardData);

	// run review directory의 AI JSON을 기존 dashboard 데이터에 병합한다.
	static bool AppendAiFromRunDirectory(
		const FString& runDirectory,
		FProjectRunResultDashboardData& outDashboardData);

	// analysis_run_response_v2 JSON 문자열을 AI 요약과 제안으로 병합한다.
	static bool AppendAiFromAnalysisResponseJsonString(
		const FString& responseJson,
		FProjectRunResultDashboardData& outDashboardData);

	// review/<id>/recommendations.json 문자열을 AI 요약과 제안으로 병합한다.
	static bool AppendAiFromRecommendationsJsonString(
		const FString& recommendationsJson,
		FProjectRunResultDashboardData& outDashboardData);
};
