#pragma once

#include "CoreMinimal.h"
#include "Shared/Struct/DeliveryBot/Setup/DeliveryBotPythonSettings.h"
#include "HAL/PlatformProcess.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "DeliveryBotPythonProcessSubsystem.generated.h"

UENUM(BlueprintType)
enum class EDeliveryBotPythonProcessState : uint8
{
	NotStarted,
	CheckingExistingServer,
	Launching,
	WaitingForHealth,
	Ready,
	Failed,
	Stopped
};

UCLASS()
class ODIROSIM_API UDeliveryBotPythonProcessSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& collection) override; // Python 서버 생명주기 시작
	virtual void Deinitialize() override; // Python 서버 생명주기 종료

public:
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Python")
	bool IsReady() const { return PythonState == EDeliveryBotPythonProcessState::Ready; } // Python 서버 준비 여부 반환

	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Python")
	FString GetBaseUrl() const; // Python 서버 기본 URL 반환

private:  // health 는 서버가 동작중인지 혹은 어떤 상태인지 확인
	void StartPythonServerLifecycle();				// Python 서버 실행 흐름 시작
	void CheckExistingServerHealth();				// 기존 Python 서버 health 확인
	void StartHealthPolling();						// health check 반복 시작
	void SendHealthCheckRequest();					// health check 요청 1회 전송
	void RecordReady(bool bAlreadyRunningServer);	// Python 서버 준비 완료 처리
	void RecordFailed(const FString& errorMessage);	// Python 서버 실패 처리
	bool LaunchPythonProcess();						// Python 서버 프로세스 실행
	void StopPythonProcess();						// 직접 실행한 Python 서버만 종료

	FString BuildHealthUrl() const;					// health check URL 생성
	FString ResolveServerScriptPath() const;		// server.py 절대 경로 생성

private:
	FDeliveryBotPythonSettings Settings;
	FProcHandle PythonProcessHandle;
	FTimerHandle HealthPollingTimerHandle;

	EDeliveryBotPythonProcessState PythonState{ EDeliveryBotPythonProcessState::NotStarted };

private:
	FString LastErrorMessage;

	bool bLaunchedByThisProcess{ false };
	bool bHealthRequestInFlight{ false };

	double HealthCheckStartTimeSeconds{ 0.0 };


};