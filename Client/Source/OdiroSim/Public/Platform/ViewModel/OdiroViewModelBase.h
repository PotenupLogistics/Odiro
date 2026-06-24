#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "OdiroViewModelBase.generated.h"

// Platform UI ViewModel의 공통 FieldNotify 상태와 진단 메시지 base.
UCLASS(Abstract, BlueprintType)
class ODIROSIM_API UOdiroViewModelBase : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	// 현재 진단/검증 메시지를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ViewModel")
	FString GetDiagnosticsText() const { return DiagnosticsText; }

	// UI에 표시할 진단/검증 메시지를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ViewModel")
	void SetDiagnosticsText(const FString& message);

	// 진단 메시지를 비운다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ViewModel")
	void ClearDiagnostics();

	// command가 비동기 작업 중인지 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ViewModel")
	bool IsBusy() const { return bBusy; }

	// command 진행 상태를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ViewModel")
	void SetBusy(bool bInBusy);

protected:
	// View가 그대로 표시할 진단/검증 메시지.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ViewModel", meta = (AllowPrivateAccess = "true"))
	FString DiagnosticsText;

	// View command control의 busy/disabled 상태.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ViewModel", meta = (AllowPrivateAccess = "true"))
	bool bBusy = false;
};
