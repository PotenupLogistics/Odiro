#pragma once

#include "CoreMinimal.h"
#include "EpisodeSpecTypes.h"
#include "EpisodeCompileTypes.generated.h"

// 에피소드 컴파일 결과와 검증 메시지를 정의하는 파일임.

UENUM(BlueprintType)
enum class EEpisodeCompileDiagnosticSeverity : uint8
{
	Info,
	Warning,
	Error
};

// 에피소드 컴파일 중 발견한 경고나 오류 한 건을 담는 타입임.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeCompileDiagnostic
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	EEpisodeCompileDiagnosticSeverity Severity = EEpisodeCompileDiagnosticSeverity::Info;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString Code;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString Message;
};

// 컴파일 성공 여부와 생성된 WorldSpec을 함께 돌려주기 위한 타입임.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeCompileResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	bool bSuccess = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FEpisodeWorldSpec WorldSpec;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	TArray<FEpisodeCompileDiagnostic> Diagnostics;
};
