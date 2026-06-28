#if WITH_DEV_AUTOMATION_TESTS

#include "Shared/EpisodeLidarRayReplayDataTypes.h"

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace
{
	// Builds a representative hit ray for binary round-trip coverage.
	FEpisodeLidarRaySample MakeTestHitRay(const int32 RayIndex)
	{
		FEpisodeLidarRaySample Ray;
		Ray.DimensionType = 2;
		Ray.Classification = ELidarRayReplayClassification::Obstacle;
		Ray.bHit = true;
		Ray.RayIndex = RayIndex;
		Ray.RayYawDegree = 12.5f;
		Ray.RayPitchDegree = -3.0f;
		Ray.DistanceM = 2.25f;
		Ray.StartLocationCm = FVector(10.0, 20.0, 30.0);
		Ray.EndLocationCm = FVector(200.0, 220.0, 40.0);
		Ray.HitLocationCm = FVector(190.0, 210.0, 38.0);
		return Ray;
	}

	// Builds a representative miss ray for binary round-trip coverage.
	FEpisodeLidarRaySample MakeTestMissRay(const int32 RayIndex)
	{
		FEpisodeLidarRaySample Ray;
		Ray.DimensionType = 1;
		Ray.Classification = ELidarRayReplayClassification::Miss;
		Ray.bHit = false;
		Ray.RayIndex = RayIndex;
		Ray.RayYawDegree = -20.0f;
		Ray.RayPitchDegree = 0.0f;
		Ray.DistanceM = 5.0f;
		Ray.StartLocationCm = FVector(10.0, 20.0, 30.0);
		Ray.EndLocationCm = FVector(-300.0, 120.0, 30.0);
		Ray.HitLocationCm = FVector::ZeroVector;
		return Ray;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEpisodeLidarRayReplayDataTypesRoundTripTest,
	"OdiroSim.Replay.LidarRay.DataTypes.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEpisodeLidarRayReplayDataTypesRoundTripTest::RunTest(const FString& Parameters)
{
	const FString OutputDirectory = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("Automation"),
		TEXT("LidarRayReplay"),
		FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const FString ManifestPath = FPaths::Combine(OutputDirectory, TEXT("rays.meta.json"));
	const FString FramePath = FPaths::Combine(OutputDirectory, TEXT("rays.frames.bin"));

	FEpisodeLidarRayFrame FirstFrame;
	FirstFrame.TimeSeconds = 0.1f;
	FirstFrame.SensorSequence = 10;
	FirstFrame.Rays.Add(MakeTestHitRay(0));
	FirstFrame.Rays.Add(MakeTestMissRay(1));

	FEpisodeLidarRayFrame SecondFrame;
	SecondFrame.TimeSeconds = 0.2f;
	SecondFrame.SensorSequence = 11;
	SecondFrame.Rays.Add(MakeTestHitRay(2));

	TArray<FEpisodeLidarRayFrame> Frames;
	Frames.Add(FirstFrame);
	Frames.Add(SecondFrame);

	TArray<FString> Diagnostics;
	TestTrue(
		TEXT("ray frames save"),
		FEpisodeLidarRayReplayBinary::SaveFramesToFile(FramePath, Frames, Diagnostics));
	TestEqual(TEXT("save diagnostics empty"), Diagnostics.Num(), 0);

	TArray<FEpisodeLidarRayFrame> LoadedFrames;
	TestTrue(
		TEXT("ray frames load"),
		FEpisodeLidarRayReplayBinary::LoadFramesFromFile(FramePath, LoadedFrames, Diagnostics));
	TestEqual(TEXT("load diagnostics empty"), Diagnostics.Num(), 0);
	TestEqual(TEXT("loaded frame count"), LoadedFrames.Num(), 2);
	if (LoadedFrames.Num() == 2)
	{
		TestEqual(TEXT("first sequence"), LoadedFrames[0].SensorSequence, 10);
		TestEqual(TEXT("second sequence"), LoadedFrames[1].SensorSequence, 11);
		TestEqual(TEXT("first ray count"), LoadedFrames[0].Rays.Num(), 2);
		TestEqual(TEXT("second ray count"), LoadedFrames[1].Rays.Num(), 1);
		TestEqual(TEXT("hit classification"),
			LoadedFrames[0].Rays[0].Classification,
			ELidarRayReplayClassification::Obstacle);
		TestTrue(TEXT("hit ray flag"), LoadedFrames[0].Rays[0].bHit);
		TestFalse(TEXT("miss ray flag"), LoadedFrames[0].Rays[1].bHit);
		TestEqual(TEXT("hit location x"), LoadedFrames[0].Rays[0].HitLocationCm.X, 190.0);
	}
	TestEqual(
		TEXT("resolve before first frame"),
		FEpisodeLidarRayReplayBinary::ResolveFrameIndex(0.0, LoadedFrames),
		0);
	TestEqual(
		TEXT("resolve nearest lower frame"),
		FEpisodeLidarRayReplayBinary::ResolveFrameIndex(0.149, LoadedFrames),
		0);
	TestEqual(
		TEXT("resolve nearest upper frame"),
		FEpisodeLidarRayReplayBinary::ResolveFrameIndex(0.151, LoadedFrames),
		1);
	TestEqual(
		TEXT("resolve after last frame"),
		FEpisodeLidarRayReplayBinary::ResolveFrameIndex(1.0, LoadedFrames),
		1);

	FEpisodeLidarRayReplayManifest Manifest;
	Manifest.FrameCount = Frames.Num();
	Manifest.TotalRayCount = 3;
	Manifest.FirstSensorSequence = 10;
	Manifest.LastSensorSequence = 11;
	Manifest.FirstTimeSeconds = 0.1;
	Manifest.LastTimeSeconds = 0.2;

	TestTrue(
		TEXT("manifest saves"),
		FEpisodeLidarRayReplayManifestJson::SaveToFile(ManifestPath, Manifest, Diagnostics));
	TestEqual(TEXT("manifest save diagnostics empty"), Diagnostics.Num(), 0);

	FEpisodeLidarRayReplayManifest LoadedManifest;
	TestTrue(
		TEXT("manifest loads"),
		FEpisodeLidarRayReplayManifestJson::LoadFromFile(ManifestPath, LoadedManifest, Diagnostics));
	TestEqual(TEXT("manifest load diagnostics empty"), Diagnostics.Num(), 0);
	TestEqual(TEXT("manifest schema"), LoadedManifest.Schema, FString(TEXT("episode_lidar_rays")));
	TestEqual(TEXT("manifest frame file"), LoadedManifest.FrameFile, FString(TEXT("rays.frames.bin")));
	TestEqual(TEXT("manifest frame count"), LoadedManifest.FrameCount, 2);
	TestEqual(TEXT("manifest total ray count"), LoadedManifest.TotalRayCount, 3);
	TestEqual(TEXT("obstacle color red"), LoadedManifest.ObstacleColor.R, static_cast<uint8>(255));
	TestEqual(TEXT("wall color blue"), LoadedManifest.WallColor.B, static_cast<uint8>(255));

	return true;
}

#endif
