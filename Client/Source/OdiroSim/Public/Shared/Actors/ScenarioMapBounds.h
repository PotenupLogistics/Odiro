#pragma once

#include "CoreMinimal.h"
#include "Math/Box2D.h"

class AActor;
struct FScenarioPlaceableInstanceSpec;

// Grid와 Preview가 공유하는 최종 시나리오 맵 영역이다.
struct ODIROSIM_API FScenarioMapBounds
{
	// Padding까지 적용된 최종 XY 영역이다.
	FBox2D XYBounds = FBox2D(ForceInit);
	// 유효한 Surface actor 중심 높이의 평균값이다.
	double CenterZ = 0.0;
	// 최종 XY 영역이 유효한지 반환한다.
	bool IsValid() const
	{
		return XYBounds.bIsValid;
	}
};

// Surface actor와 DeliveryBot route에서 공용 맵 영역을 계산한다.
class ODIROSIM_API FScenarioMapBoundsResolver
{
public:
	// Surface actor와 DeliveryBot route를 합쳐 Padding이 적용된 최종 영역을 계산한다.
	static bool TryResolve(
		const TArray<AActor*>& surfaceActors,
		const TArray<FScenarioPlaceableInstanceSpec>& placeables,
		double paddingCm,
		FScenarioMapBounds& outBounds);

private:
	// 유효한 actor의 component bounds를 현재 Surface 영역에 누적한다.
	static void AccumulateSurfaceActor(
		const AActor* actor,
		FBox2D& inOutXYBounds,
		double& inOutCenterZSum,
		int32& inOutValidActorCount);

	// DeliveryBot의 최종 Start와 Goal이 영역에 포함되도록 확장한다.
	static void ExpandForDeliveryBotRoute(
		const FScenarioPlaceableInstanceSpec& placeableSpec,
		FBox2D& inOutXYBounds);
};
