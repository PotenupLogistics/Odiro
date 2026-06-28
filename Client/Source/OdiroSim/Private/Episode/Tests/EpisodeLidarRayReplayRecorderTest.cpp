#if WITH_DEV_AUTOMATION_TESTS

#include "Episode/EpisodeLidarRayReplayRecorder.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Shared/EpisodeLidarRayReplayDataTypes.h"

namespace
{
	// Builds one source LiDAR ray for recorder tests.
	FDeliveryBotLidarRayInfo MakeRecorderTestRay(
		const int32 RayIndex,
		const bool bHit,
		const FName Tag,
		const FVector& HitLocationCm)
	{
		FDeliveryBotLidarRayInfo RayInfo;
		RayInfo.RayDimensionType = EDeliveryBotLidarRayDimensionType::ThreeD;
		RayInfo.bHit = bHit;
		RayInfo.RayIndex = RayIndex;
		RayInfo.RayYawDegree = 10.0f + static_cast<float>(RayIndex);
		RayInfo.RayPitchDegree = -2.0f;
		RayInfo.DistanceM = bHit ? 1.25f : 5.0f;
		RayInfo.StartLocationCm = FVector(10.0, 20.0, 30.0);
		RayInfo.EndLocationCm = FVector(110.0, 120.0, 35.0);
		RayInfo.HitLocationCm = HitLocationCm;
		if (!Tag.IsNone())
		{
			RayInfo.TargetTags.Add(Tag);
		}
		return RayInfo;
	}

	// Builds one source LiDAR scan for recorder tests.
	FDeliveryBotLidarScanInfo MakeRecorderTestScan()
	{
		FDeliveryBotLidarScanInfo ScanInfo;
		ScanInfo.RayInfos.Add(MakeRecorderTestRay(0, true, TEXT("obstacle"), FVector(100.0, 100.0, 25.0)));
		ScanInfo.RayInfos.Add(MakeRecorderTestRay(1, true, TEXT("corridor"), FVector(50.0, 50.0, 5.0)));
		ScanInfo.RayInfos.Add(MakeRecorderTestRay(2, false, NAME_None, FVector::ZeroVector));
		return ScanInfo;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEpisodeLidarRayReplayRecorderWriteTest,
	"OdiroSim.Replay.LidarRay.Recorder.WriteArtifacts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEpisodeLidarRayReplayRecorderWriteTest::RunTest(const FString& Parameters)
{
	const FString EpisodeDirectory = FPaths::Combine(
		FPaths::AutomationTransientDir(),
		TEXT("LidarRayReplayRecorder"),
		FGuid::NewGuid().ToString(EGuidFormats::Digits));
	IFileManager::Get().MakeDirectory(*EpisodeDirectory, true);

	FEpisodeLidarRayReplayRecorder Recorder;
	TArray<FString> Diagnostics;
	TestTrue(TEXT("recorder opens"), Recorder.Open(EpisodeDirectory, Diagnostics));
	TestEqual(TEXT("open diagnostics empty"), Diagnostics.Num(), 0);

	const FDeliveryBotLidarScanInfo ScanInfo = MakeRecorderTestScan();
	TestTrue(TEXT("first scan records"), Recorder.RecordSensorSnapshot(100.0, 7, ScanInfo, Diagnostics));
	TestTrue(TEXT("duplicate sequence is skipped"), Recorder.RecordSensorSnapshot(101.0, 7, ScanInfo, Diagnostics));
	TestTrue(TEXT("second scan records"), Recorder.RecordSensorSnapshot(102.0, 8, ScanInfo, Diagnostics));
	TestEqual(TEXT("unique frame count"), Recorder.GetFrameCount(), 2);
	TestTrue(TEXT("recorder closes"), Recorder.Close(Diagnostics));
	TestEqual(TEXT("close diagnostics empty"), Diagnostics.Num(), 0);

	const FString LidarRayDirectory = FPaths::Combine(EpisodeDirectory, TEXT("replay"), TEXT("lidar_rays"));
	const FString ManifestPath = FPaths::Combine(LidarRayDirectory, TEXT("rays.meta.json"));
	const FString FramePath = FPaths::Combine(LidarRayDirectory, TEXT("rays.frames.bin"));
	TestTrue(TEXT("manifest exists"), IFileManager::Get().FileExists(*ManifestPath));
	TestTrue(TEXT("binary exists"), IFileManager::Get().FileExists(*FramePath));

	FEpisodeLidarRayReplayManifest Manifest;
	TestTrue(
		TEXT("manifest loads"),
		FEpisodeLidarRayReplayManifestJson::LoadFromFile(ManifestPath, Manifest, Diagnostics));
	TestEqual(TEXT("manifest frame count"), Manifest.FrameCount, 2);
	TestEqual(TEXT("manifest ray count"), Manifest.TotalRayCount, 6);
	TestEqual(TEXT("manifest first sequence"), Manifest.FirstSensorSequence, 7);
	TestEqual(TEXT("manifest last sequence"), Manifest.LastSensorSequence, 8);

	TArray<FEpisodeLidarRayFrame> Frames;
	TestTrue(
		TEXT("binary loads"),
		FEpisodeLidarRayReplayBinary::LoadFramesFromFile(FramePath, Frames, Diagnostics));
	TestEqual(TEXT("loaded frame count"), Frames.Num(), 2);
	if (Frames.Num() == 2 && Frames[0].Rays.Num() == 3)
	{
		TestEqual(
			TEXT("obstacle classification"),
			Frames[0].Rays[0].Classification,
			ELidarRayReplayClassification::Obstacle);
		TestEqual(
			TEXT("corridor ground classification"),
			Frames[0].Rays[1].Classification,
			ELidarRayReplayClassification::Ground);
		TestEqual(
			TEXT("miss classification"),
			Frames[0].Rays[2].Classification,
			ELidarRayReplayClassification::Miss);
		TestEqual(TEXT("second relative time"), Frames[1].TimeSeconds, 2.0f);
	}

	return true;
}

#endif
