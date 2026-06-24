#pragma once

#include "CoreMinimal.h"
#include "Platform/ProjectRunResultDashboard.h"
#include "Platform/ViewModel/OdiroListItemViewModel.h"
#include "ExperimentResultItemViewModels.generated.h"

// Project run result dashboard의 episode replay card item ViewModel.
UCLASS(BlueprintType)
class ODIROSIM_API UExperimentResultEpisodeViewModel : public UOdiroListItemViewModel
{
	GENERATED_BODY()

public:
	// Dashboard episode 데이터를 card 표시 상태로 변환한다.
	void InitializeFromDashboardItem(const FProjectRunEpisodeDashboardItem& episodeItem);

	// Episode id를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentResult")
	FString GetEpisodeId() const { return EpisodeId; }

	// Episode id를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentResult")
	void SetEpisodeId(const FString& episodeId);

	// Episode 실행 시간 표시 문자열을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentResult")
	FString GetDurationLabel() const { return DurationLabel; }

	// Episode 실행 시간 표시 문자열을 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentResult")
	void SetDurationLabel(const FString& durationLabel);

	// Episode 성공 표시 상태를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentResult")
	bool IsSuccess() const { return bSuccess; }

	// Episode 성공 표시 상태를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentResult")
	void SetSuccess(bool bInSuccess);

	// Episode preview image 표시 여부를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentResult")
	bool HasPreviewImage() const { return bHasPreviewImage; }

	// Episode preview image 표시 여부를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentResult")
	void SetHasPreviewImage(bool bInHasPreviewImage);

private:
	// 6자리 episode id.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentResult", meta = (AllowPrivateAccess = "true"))
	FString EpisodeId;

	// Episode 실행 시간 표시 문자열.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentResult", meta = (AllowPrivateAccess = "true"))
	FString DurationLabel;

	// Episode 성공 표시 상태.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentResult", meta = (AllowPrivateAccess = "true"))
	bool bSuccess = false;

	// Episode preview image 존재 여부.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentResult", meta = (AllowPrivateAccess = "true"))
	bool bHasPreviewImage = false;
};

// Project run result dashboard의 AI suggestion row item ViewModel.
UCLASS(BlueprintType)
class ODIROSIM_API UExperimentResultSuggestionViewModel : public UOdiroListItemViewModel
{
	GENERATED_BODY()

public:
	// Dashboard suggestion 데이터를 row 표시 상태로 변환한다.
	void InitializeFromDashboardItem(const FProjectRunAiSuggestionDashboardItem& suggestionItem);

	// 표시 심각도를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentResult")
	EProjectRunAiSuggestionSeverity GetSeverity() const { return Severity; }

	// 표시 심각도를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentResult")
	void SetSeverity(EProjectRunAiSuggestionSeverity severity);

	// 표시 심각도 label을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentResult")
	FString GetSeverityLabel() const { return SeverityLabel; }

	// 표시 심각도 label을 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentResult")
	void SetSeverityLabel(const FString& severityLabel);

	// 제안 사유 표시 문자열을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentResult")
	FString GetReason() const { return Reason; }

	// 제안 사유 표시 문자열을 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentResult")
	void SetReason(const FString& reason);

	// 권장 조치 표시 문자열을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentResult")
	FString GetRecommendation() const { return Recommendation; }

	// 권장 조치 표시 문자열을 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentResult")
	void SetRecommendation(const FString& recommendation);

	// 설정 parameter 표시 문자열을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentResult")
	FString GetParameterName() const { return ParameterName; }

	// 설정 parameter 표시 문자열을 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentResult")
	void SetParameterName(const FString& parameterName);

	// 현재 parameter 값 표시 문자열을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentResult")
	FString GetCurrentValue() const { return CurrentValue; }

	// 현재 parameter 값 표시 문자열을 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentResult")
	void SetCurrentValue(const FString& currentValue);

	// 제안 parameter 값 표시 문자열을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ExperimentResult")
	FString GetSuggestedValue() const { return SuggestedValue; }

	// 제안 parameter 값 표시 문자열을 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentResult")
	void SetSuggestedValue(const FString& suggestedValue);

private:
	// AI suggestion 표시 심각도.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentResult", meta = (AllowPrivateAccess = "true"))
	EProjectRunAiSuggestionSeverity Severity = EProjectRunAiSuggestionSeverity::Info;

	// AI suggestion 심각도 label.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentResult", meta = (AllowPrivateAccess = "true"))
	FString SeverityLabel;

	// AI suggestion 사유.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentResult", meta = (AllowPrivateAccess = "true"))
	FString Reason;

	// AI suggestion 권장 조치.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentResult", meta = (AllowPrivateAccess = "true"))
	FString Recommendation;

	// AI suggestion 대상 parameter.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentResult", meta = (AllowPrivateAccess = "true"))
	FString ParameterName;

	// AI suggestion 현재 값.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentResult", meta = (AllowPrivateAccess = "true"))
	FString CurrentValue;

	// AI suggestion 제안 값.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ExperimentResult", meta = (AllowPrivateAccess = "true"))
	FString SuggestedValue;
};
