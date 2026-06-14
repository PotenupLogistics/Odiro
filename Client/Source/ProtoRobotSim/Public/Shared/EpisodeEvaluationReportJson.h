#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodeResultTypes.h"

class FJsonObject;

// FEpisodeRunRecord를 LLM 해석용 episode_evaluation_report JSON으로 직렬화하는 유틸리티.
struct PROTOROBOTSIM_API FEpisodeEvaluationReportJson
{
	static bool TryWriteReportJson(
		const FEpisodeRunRecord& Record,
		FString& OutJson,
		TArray<FString>& OutDiagnostics);

private:
	static TSharedRef<FJsonObject> MakeReportObject(const FEpisodeRunRecord& Record);
};
