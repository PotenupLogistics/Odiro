#include "Misc/AutomationTest.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Shared/EpisodeReplayDataTypes.h"

namespace
{
	// Builds a deterministic frame for replay binary round-trip tests.
	FEpisodeReplayRobotFrame MakeReplayTestFrame(
		float TimeSeconds,
		const FVector& Position,
		float TargetSpeedKmh)
	{
		FEpisodeReplayRobotFrame Frame;
		Frame.TimeSeconds = TimeSeconds;
		Frame.PositionCm = Position;
		Frame.Rotation = FQuat(FVector::UpVector, FMath::DegreesToRadians(15.0f)).GetNormalized();
		Frame.VelocityCmPerSecond = FVector(100.0, 0.0, 0.0);
		Frame.SpeedKmh = 3.6f;
		Frame.Steering = 0.25f;
		Frame.Throttle = 0.5f;
		Frame.Brake = 0.0f;
		Frame.TargetSpeedKmh = TargetSpeedKmh;
		Frame.Direction = EEpisodeReplayDirection::Forward;
		return Frame;
	}

	// Returns an isolated automation temp directory for replay tests.
	FString MakeReplayTestDirectory()
	{
		return FPaths::Combine(
			FPaths::AutomationTransientDir(),
			TEXT("OdiroReplayBinary"));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioReplayBinaryRoundTripTest,
	"Odiro.Scenario.Replay.Binary.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioReplayBinaryRoundTripTest::RunTest(const FString& Parameters)
{
	const FString TestDirectory = MakeReplayTestDirectory();
	IFileManager::Get().MakeDirectory(*TestDirectory, true);
	const FString ReplayDirectory = FPaths::Combine(TestDirectory, TEXT("replay"));
	const FString FramePath = FPaths::Combine(ReplayDirectory, TEXT("replay.frames.bin"));
	const FString ManifestPath = FPaths::Combine(ReplayDirectory, TEXT("replay.meta.json"));

	TArray<FEpisodeReplayRobotFrame> Frames;
	Frames.Add(MakeReplayTestFrame(0.0f, FVector(1.0, 2.0, 3.0), 4.0f));
	Frames.Add(MakeReplayTestFrame(1.0f / 30.0f, FVector(10.0, 20.0, 30.0), 5.0f));
	Frames.Add(MakeReplayTestFrame(2.0f / 30.0f, FVector(100.0, 200.0, 300.0), 6.0f));

	TArray<FString> Diagnostics;
	TestTrue(
		TEXT("save replay frames"),
		FEpisodeReplayBinary::SaveFramesToFile(FramePath, Frames, Diagnostics));
	if (!Diagnostics.IsEmpty())
	{
		AddInfo(FString::Join(Diagnostics, TEXT("\n")));
	}

	FEpisodeReplayManifest Manifest;
	Manifest.FrameCount = Frames.Num();
	Manifest.DurationSeconds = Frames.Last().TimeSeconds;
	Manifest.ScenarioSample = TEXT("scenario-sample.json");
	Manifest.ScenarioHash = TEXT("test");
	TestTrue(
		TEXT("save replay manifest"),
		FEpisodeReplayManifestJson::SaveToFile(ManifestPath, Manifest, Diagnostics));

	FEpisodeReplayBinaryHeader LoadedHeader;
	TArray<FEpisodeReplayRobotFrame> LoadedFrames;
	TestTrue(
		TEXT("load replay frames"),
		FEpisodeReplayBinary::LoadFramesFromFile(
			FramePath,
			LoadedFrames,
			LoadedHeader,
			Diagnostics));
	TestEqual(TEXT("loaded frame count"), LoadedFrames.Num(), Frames.Num());
	TestEqual(TEXT("header frame size"), LoadedHeader.FrameSizeBytes, EpisodeReplayV1::FixedFrameSizeBytes);
	TestEqual(TEXT("second frame target speed"), LoadedFrames[1].TargetSpeedKmh, 5.0f);
	TestEqual(TEXT("third frame x"), LoadedFrames[2].PositionCm.X, 100.0);

	FEpisodeReplayManifest LoadedManifest;
	TestTrue(
		TEXT("load replay manifest"),
		FEpisodeReplayManifestJson::LoadFromFile(
			ManifestPath,
			LoadedManifest,
			Diagnostics));
	TestEqual(TEXT("manifest frame count"), LoadedManifest.FrameCount, Frames.Num());
	TestEqual(
		TEXT("resolve frame index"),
		FEpisodeReplayBinary::ResolveFrameIndex(2.0 / 30.0, LoadedManifest),
		2);
	return true;
}
