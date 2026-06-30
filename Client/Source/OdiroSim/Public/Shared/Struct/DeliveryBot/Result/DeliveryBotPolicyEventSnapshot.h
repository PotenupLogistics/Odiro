#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodeResultTypes.h"

// Python policy/server event를 EvaluationSubsystem에 전달하기 위한 정규화된 snapshot.
struct ODIROSIM_API FDeliveryBotPolicyEventSnapshot
{
	EEpisodeEvaluationEventType EventType{ EEpisodeEvaluationEventType::None }; // EvaluationSubsystem에 기록할 episode event 타입
	EEpisodeEvaluationEventSeverity Severity{ EEpisodeEvaluationEventSeverity::Info }; // 이벤트 심각도

	bool bTerminalFailure{ false }; // true이면 이벤트 기록 후 episode를 실패로 종료한다

	int32 Sequence{ INDEX_NONE }; // Python decide 요청 sequence
	float RunTimeSeconds{ 0.f }; // 이벤트가 발생한 episode runtime

	FString Endpoint{}; // 이벤트가 나온 Python HTTP endpoint
	FString EventCode{}; // 이벤트 종류를 식별하는 machine-readable code
	FString Message{}; // episode event에 표시할 요약 메시지

	FString SelectedPolicy{}; // Python이 선택했거나 실패한 policy 이름
	FString Reason{}; // Python policy/debug reason code

	int32 HttpStatusCode{ 0 }; // 서버 실패 시 HTTP status code, 응답이 없으면 0
	FString ErrorCode{}; // 서버/정책 실패의 machine-readable error code
	FString ErrorMessage{}; // 서버/정책 실패의 human-readable message
	bool bRetryable{ false }; // 같은 요청을 재시도할 수 있는 실패인지 여부
	FString PythonProcessStatus{}; // 서버 실패 시 Python process subsystem의 진단 상태
	FString ResponseBodySnippet{}; // 서버 실패 시 응답 본문의 짧은 진단 문자열

	FString PathStatus{}; // RePath 시 Python이 판단한 path 상태
	int32 PathIndex{ INDEX_NONE }; // 현재 path index
	int32 PathLength{ 0 }; // 현재 path point 개수
	int32 TargetPathIndex{ INDEX_NONE }; // 추종 대상 path index

	bool bHasTargetWorldPoint{ false }; // TargetWorldPointCm 값이 유효한지 여부
	FVector TargetWorldPointCm{ FVector::ZeroVector }; // RePath 시 목표 world point, cm 단위

	float ClosestPathDistanceCm{ 0.f }; // 로봇과 path 사이의 최근접 거리
	float MaxPathErrorCm{ 0.f }; // 허용 가능한 최대 path error

	int32 ObstacleWarningCount{ 0 }; // RePath 판단 당시 obstacle warning 개수
	FString LastObstacleWarningSource{}; // 마지막 obstacle warning의 source
	int32 BlockedCorridorCellCount{ 0 }; // corridor에서 막힌 cell 개수
	int32 DynamicBlockedCellCount{ 0 }; // 동적 장애물로 막힌 cell 개수
	float PathfindTotalMs{ 0.f }; // Python A* 전체 처리 시간(ms)
	float PathfindCellLookupMs{ 0.f }; // Python A* cell lookup 생성 시간(ms)
	float PathfindSoftCostMs{ 0.f }; // Python A* obstacle soft cost 계산 시간(ms)
	float PathfindSearchMs{ 0.f }; // Python A* open-set 탐색 시간(ms)
	float PathfindSmoothMs{ 0.f }; // Python A* line-of-sight smoothing 시간(ms)
	int32 PathfindGridCellCount{ 0 }; // Python A* 입력 grid cell 수
	int32 PathfindBlockedCellCount{ 0 }; // Python A* 입력 blocked cell 수
	int32 PathfindSoftCostCellCount{ 0 }; // Python A* soft cost가 적용된 cell 수
	int32 PathfindVisitedNodeCount{ 0 }; // Python A* pop 처리한 node 수
	int32 PathfindNeighborCheckCount{ 0 }; // Python A* neighbor 후보 확인 수
	int32 PathfindOpenPushCount{ 0 }; // Python A* open set push 수
};
