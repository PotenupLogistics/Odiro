#pragma once

#include "CoreMinimal.h"
#include "Shared/Actors/ScenarioMapBounds.h"

// Preview 프레이밍 계산에 사용하는 해상도와 여백 설정이다.
struct ODIROSIM_API FScenarioPreviewFramingSettings
{
	// 출력 이미지의 가로 픽셀 수다.
	int32 OutputWidth = 512;

	// 출력 이미지의 세로 픽셀 수다.
	int32 OutputHeight = 384;

	// 시나리오 대표 Preview의 촬영 범위를 조절하는 단일 배율이다.
	double ScenarioPreviewFitScale = 0.75;

	// 해상도와 프레이밍 설정이 계산에 사용할 수 있는지 확인한다.
	bool IsValid() const;

	// 출력 이미지의 가로세로 비율을 반환한다.
	double GetAspectRatio() const;
};

// Top View Capture에 전달할 카메라 프레이밍 결과다.
struct ODIROSIM_API FScenarioPreviewFrame
{
	// 촬영할 월드 XY 중심 좌표다.
	FVector2D CenterXY = FVector2D::ZeroVector;

	// 촬영 카메라가 기준으로 사용할 맵 중심 높이다.
	double CenterZ = 0.0;

	// Orthographic 카메라가 표시할 월드 가로 범위다.
	double OrthoWidth = 0.0;

	// 프레이밍 결과가 Capture에 사용 가능한지 확인한다.
	bool IsValid() const;
};

// 시나리오 Bounds로 대표 Preview 프레임을 계산한다.
class ODIROSIM_API FScenarioPreviewFramingResolver
{
public:
	// Map Bounds와 시나리오 대표 Preview 배율로 프레임을 계산한다.
	static bool TryResolveScenario(
		const FScenarioMapBounds& mapBounds,
		const FScenarioPreviewFramingSettings& settings,
		FScenarioPreviewFrame& outFrame);

private:
	// Map Bounds의 좌표와 중심 높이가 프레이밍에 사용할 수 있는지 확인한다.
	static bool IsFiniteMapBounds(const FScenarioMapBounds& mapBounds);
};
