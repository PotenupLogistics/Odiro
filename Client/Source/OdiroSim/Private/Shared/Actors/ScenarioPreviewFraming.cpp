#include "Shared/Actors/ScenarioPreviewFraming.h"

// 해상도와 프레이밍 설정이 계산에 사용할 수 있는지 확인한다.
bool FScenarioPreviewFramingSettings::IsValid() const
{
	// 해상도와 배율의 허용 범위를 검증한다.
	return OutputWidth > 0
		&& OutputHeight > 0
		&& FMath::IsFinite(ScenarioPreviewFitScale)
		&& ScenarioPreviewFitScale > 0.0;
}

// 출력 이미지의 가로세로 비율을 반환한다.
double FScenarioPreviewFramingSettings::GetAspectRatio() const
{
	// 0으로 나누는 잘못된 해상도를 거부한다.
	if (OutputWidth <= 0 || OutputHeight <= 0)
	{
		return 0.0;
	}

	// 픽셀 해상도를 실수 비율로 변환한다.
	return static_cast<double>(OutputWidth)
		/ static_cast<double>(OutputHeight);
}

// 프레이밍 결과가 Capture에 사용 가능한지 확인한다.
bool FScenarioPreviewFrame::IsValid() const
{
	// 중심 좌표와 Orthographic 범위를 검증한다.
	return FMath::IsFinite(CenterXY.X)
		&& FMath::IsFinite(CenterXY.Y)
		&& FMath::IsFinite(CenterZ)
		&& FMath::IsFinite(OrthoWidth)
		&& OrthoWidth > 0.0;
}

// Map Bounds와 시나리오 대표 Preview 배율로 프레임을 계산한다.
bool FScenarioPreviewFramingResolver::TryResolveScenario(
	const FScenarioMapBounds& mapBounds,
	const FScenarioPreviewFramingSettings& settings,
	FScenarioPreviewFrame& outFrame)
{
	// 실패 시 이전 프레이밍 결과가 사용되지 않도록 초기화한다.
	outFrame = FScenarioPreviewFrame{};

	// 외부에서 전달된 Bounds와 설정을 검증한다.
	if (!settings.IsValid() || !IsFiniteMapBounds(mapBounds))
	{
		return false;
	}

	// 전체 Bounds를 담는 기준 가로 범위를 계산한다.
	const FVector2D mapSize = mapBounds.XYBounds.GetSize();
	const double aspectRatio = settings.GetAspectRatio();
	const double requiredOrthoWidth = FMath::Max(
		mapSize.X,
		mapSize.Y * aspectRatio);

	// 시나리오 대표 Preview 배율을 적용한 최종 프레임을 구성한다.
	outFrame.CenterXY = mapBounds.XYBounds.GetCenter();
	outFrame.CenterZ = mapBounds.CenterZ;
	outFrame.OrthoWidth =
		requiredOrthoWidth * settings.ScenarioPreviewFitScale;

	// 계산 과정에서 잘못된 결과가 발생했는지 최종 확인한다.
	return outFrame.IsValid();
}

// Map Bounds의 좌표와 중심 높이가 프레이밍에 사용할 수 있는지 확인한다.
bool FScenarioPreviewFramingResolver::IsFiniteMapBounds(
	const FScenarioMapBounds& mapBounds)
{
	// Bounds 상태와 중심 높이를 먼저 검증한다.
	if (!mapBounds.IsValid() || !FMath::IsFinite(mapBounds.CenterZ))
	{
		return false;
	}

	// Bounds를 구성하는 모든 XY 좌표를 검증한다.
	return FMath::IsFinite(mapBounds.XYBounds.Min.X)
		&& FMath::IsFinite(mapBounds.XYBounds.Min.Y)
		&& FMath::IsFinite(mapBounds.XYBounds.Max.X)
		&& FMath::IsFinite(mapBounds.XYBounds.Max.Y);
}
