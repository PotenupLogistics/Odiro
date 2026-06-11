#pragma once

#include "CoreMinimal.h"
#include "ScenarioSpecTypes.h"
#include "ScenarioCompileTypes.generated.h"

// 에피소드 컴파일 결과와 검증 메시지를 정의하는 파일.

UENUM(BlueprintType)
enum class EScenarioCompileDiagnosticSeverity : uint8
{
	Info,
	Warning,
	Error
};

// 에피소드 컴파일 중 발견한 경고나 오류 한 건을 담는 타입.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FScenarioCompileDiagnostic
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	EScenarioCompileDiagnosticSeverity Severity = EScenarioCompileDiagnosticSeverity::Info;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString Code;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString Message;
};

// 컴파일 성공 여부와 생성된 WorldSpec을 함께 돌려주기 위한 타입.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FScenarioCompileResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	bool bSuccess = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FScenarioWorldSpec WorldSpec;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TArray<FScenarioCompileDiagnostic> Diagnostics;
};
