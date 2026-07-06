#pragma once

#include "CoreMinimal.h"

namespace ProjectRunStatusNames
{
	// Launcher와 simulator process가 공유하는 status.json state 문자열 계약.
	constexpr const TCHAR* Starting = TEXT("starting");
	constexpr const TCHAR* Running = TEXT("running");
	constexpr const TCHAR* Stopping = TEXT("stopping");
	constexpr const TCHAR* Canceled = TEXT("canceled");
	constexpr const TCHAR* Cancelled = TEXT("cancelled");
	constexpr const TCHAR* Exited = TEXT("exited");
	constexpr const TCHAR* Completed = TEXT("completed");
	constexpr const TCHAR* Failed = TEXT("failed");

	// Cancellation 계열 상태가 terminal write를 덮어쓰지 않게 보존한다.
	FORCEINLINE bool IsCancellationState(const FString& statusState)
	{
		const FString normalizedState = statusState.TrimStartAndEnd().ToLower();
		return normalizedState == Stopping
			|| normalizedState == Canceled
			|| normalizedState == Cancelled;
	}
}
