#if WITH_DEV_AUTOMATION_TESTS

#include "Shared/Actors/ScenarioPreviewFraming.h"

#include "Misc/AutomationTest.h"

// 4:3 화면에서 세로형 Map Bounds가 잘리지 않는지 검증한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioPreviewScenarioFramingTest,
	"OdiroSim.Shared.PreviewFraming.Scenario",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

// 4:3 화면에서 세로형 Map Bounds가 잘리지 않는지 검증한다.
bool FScenarioPreviewScenarioFramingTest::RunTest(
	const FString& parameters)
{
	// 사용하지 않는 자동화 테스트 인자를 명시적으로 처리한다.
	(void)parameters;

	// 4:3 기본 설정과 세로형 맵 영역을 구성한다.
	FScenarioPreviewFramingSettings settings;
	settings.ScenarioPreviewFitScale = 1.0;

	FScenarioMapBounds mapBounds;
	mapBounds.XYBounds = FBox2D(
		FVector2D(100.0, 200.0),
		FVector2D(300.0, 600.0));
	mapBounds.CenterZ = 50.0;

	// 시나리오 프레이밍을 계산한다.
	FScenarioPreviewFrame frame;
	const bool bResolved =
		FScenarioPreviewFramingResolver::TryResolveScenario(
			mapBounds,
			settings,
			frame);

	// 기본 해상도와 계산 성공 여부를 검증한다.
	TestEqual(TEXT("output width"), settings.OutputWidth, 512);
	TestEqual(TEXT("output height"), settings.OutputHeight, 384);
	TestTrue(TEXT("scenario framing resolves"), bResolved);

	if (!bResolved)
	{
		return false;
	}

	// 중심 좌표와 세로형 Bounds를 포함하는 가로 범위를 검증한다.
	const double expectedOrthoWidth =
		400.0 * settings.GetAspectRatio();

	TestTrue(
		TEXT("scenario center"),
		frame.CenterXY.Equals(FVector2D(200.0, 400.0)));
	TestTrue(
		TEXT("scenario ortho width"),
		FMath::IsNearlyEqual(
			frame.OrthoWidth,
			expectedOrthoWidth));

	return true;
}

#endif
