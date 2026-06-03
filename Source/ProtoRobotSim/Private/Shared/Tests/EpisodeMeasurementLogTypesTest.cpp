#if WITH_DEV_AUTOMATION_TESTS

#include "Shared/EpisodeMeasurementLogTypes.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	bool ParseMeasurementLogJsonLine(const FString& JsonLine, TSharedPtr<FJsonObject>& OutObject)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonLine);
		return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
	}

	bool HasMeasurementLogDiagnosticCode(
		const TArray<FEpisodeMeasurementLogDiagnostic>& Diagnostics,
		const FString& Code)
	{
		return Diagnostics.ContainsByPredicate(
			[&Code](const FEpisodeMeasurementLogDiagnostic& Diagnostic)
			{
				return Diagnostic.Code == Code;
			});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEpisodeMeasurementLogContractDefaultsTest,
	"ProtoRobotSim.MeasurementLog.ContractDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEpisodeMeasurementLogContractDefaultsTest::RunTest(const FString& Parameters)
{
	const FEpisodeMeasurementLogSettings Settings;
	TestTrue(TEXT("logging is enabled by default"), Settings.bEnabled);
	TestEqual(TEXT("default output directory"), Settings.OutputDirectory, FString(TEXT("Saved/AnalysisLogs")));
	TestEqual(TEXT("default file prefix"), Settings.FilePrefix, FString(TEXT("MeasurementLog")));
	TestEqual(TEXT("default contract version"), Settings.Version, 1);

	const FEpisodeMeasurementLogHeaderRecord Header;
	TestEqual(TEXT("header version default"), Header.Version, 1);
	TestTrue(TEXT("header categories include perception"), Header.Categories.Contains(TEXT("perception")));
	TestTrue(TEXT("header categories include action"), Header.Categories.Contains(TEXT("action")));
	TestTrue(TEXT("header categories include truth"), Header.Categories.Contains(TEXT("truth")));
	TestTrue(TEXT("header categories include event"), Header.Categories.Contains(TEXT("event")));
	TestTrue(TEXT("header categories include diagnostic"), Header.Categories.Contains(TEXT("diagnostic")));

	const FString FileName = FEpisodeMeasurementLogJson::MakeDefaultFileName(
		Settings,
		TEXT("../Bad\\Map/Name"),
		FDateTime(2026, 6, 2, 12, 34, 56));
	TestFalse(TEXT("sanitized file name has slash"), FileName.Contains(TEXT("/")));
	TestFalse(TEXT("sanitized file name has backslash"), FileName.Contains(TEXT("\\")));
	TestFalse(TEXT("sanitized file name has traversal token"), FileName.Contains(TEXT("..")));
	TestTrue(TEXT("file name extension"), FileName.EndsWith(TEXT(".jsonl")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEpisodeMeasurementLogJsonSerializationTest,
	"ProtoRobotSim.MeasurementLog.JsonSerialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEpisodeMeasurementLogJsonSerializationTest::RunTest(const FString& Parameters)
{
	FEpisodeMeasurementLogHeaderRecord Header;
	Header.LogId = TEXT("log_01");
	Header.MapName = TEXT("EpisodeSandbox");
	Header.SourceJsonPath = TEXT("Json/Input/EpisodeSetupSample.json");
	Header.SpecHash = TEXT("abc123");

	FEpisodeMeasurementLogActorInfo RobotActor;
	RobotActor.Index = 0;
	RobotActor.Id = TEXT("robot_01");
	RobotActor.AssetId = TEXT("delivery_bot");
	RobotActor.ActorCategory = EEpisodeActorCategory::DeliveryBot;
	RobotActor.Mobility = EEpisodeMobilityMode::Moving;
	Header.Actors.Add(RobotActor);

	FString JsonLine;
	TArray<FEpisodeMeasurementLogDiagnostic> Diagnostics;
	TestTrue(TEXT("header serializes"), FEpisodeMeasurementLogJson::TryWriteHeaderLine(Header, JsonLine, Diagnostics));
	TestFalse(TEXT("header diagnostics has no error"), FEpisodeMeasurementLogJson::HasError(Diagnostics));
	TestFalse(TEXT("header line has no fixed simStep"), JsonLine.Contains(TEXT("simStep")));
	TestFalse(TEXT("header line has no fixedDeltaSeconds"), JsonLine.Contains(TEXT("fixedDeltaSeconds")));
	TestFalse(TEXT("header line has no captureHz"), JsonLine.Contains(TEXT("captureHz")));
	TestFalse(TEXT("header line has no startTimeSeconds"), JsonLine.Contains(TEXT("startTimeSeconds")));
	TestFalse(TEXT("header line has no endTimeSeconds"), JsonLine.Contains(TEXT("endTimeSeconds")));

	TSharedPtr<FJsonObject> HeaderObject;
	TestTrue(TEXT("header parses"), ParseMeasurementLogJsonLine(JsonLine, HeaderObject));
	if (HeaderObject.IsValid())
	{
		TestEqual(TEXT("header type"), HeaderObject->GetStringField(TEXT("type")), FString(TEXT("header")));
		TestEqual(TEXT("header actor count"), HeaderObject->GetArrayField(TEXT("actors")).Num(), 1);
	}

	FEpisodeMeasurementLogTickRecord Tick;
	Tick.TickIndex = 0;
	Tick.WorldTimeSeconds = 12.345;
	Tick.DeltaSeconds = 0.016;
	Tick.Robot.Id = TEXT("robot_01");
	Tick.Robot.Truth.RotationQuatXyzw = { 0.0, 0.0, 0.0, 1.0 };
	Tick.Robot.Action.TargetSpeedKmh = 4.0;
	Tick.Robot.Action.Reason = TEXT("path_follow");

	FEpisodeMeasurementLogActorState MovingActor;
	MovingActor.ActorIndex = 1;
	MovingActor.PositionCm = FVector(120.0, 40.0, 0.0);
	MovingActor.RotationQuatXyzw = { 0.0, 0.0, 0.0, 1.0 };
	MovingActor.VelocityCmPerSecond = FVector(0.0, 80.0, 0.0);
	Tick.MovingActors.Add(MovingActor);

	JsonLine.Reset();
	Diagnostics.Reset();
	TestTrue(TEXT("tick serializes"), FEpisodeMeasurementLogJson::TryWriteTickLine(Tick, JsonLine, Diagnostics));
	TestFalse(TEXT("tick diagnostics has no error"), FEpisodeMeasurementLogJson::HasError(Diagnostics));

	TSharedPtr<FJsonObject> TickObject;
	TestTrue(TEXT("tick parses"), ParseMeasurementLogJsonLine(JsonLine, TickObject));
	if (TickObject.IsValid())
	{
		TestEqual(TEXT("tick type"), TickObject->GetStringField(TEXT("type")), FString(TEXT("tick")));
		TestEqual(TEXT("moving actor count"), TickObject->GetArrayField(TEXT("movingActors")).Num(), 1);

		const TSharedPtr<FJsonObject> RobotObject = TickObject->GetObjectField(TEXT("robot"));
		const TSharedPtr<FJsonObject> PerceptionObject = RobotObject->GetObjectField(TEXT("perception"));
		const TSharedPtr<FJsonObject> LidarObject = PerceptionObject->GetObjectField(TEXT("lidar"));
		TestFalse(TEXT("lidar has no front object"), LidarObject->GetBoolField(TEXT("hasFrontObject")));
		TestTrue(TEXT("lidar keeps distance key"), LidarObject->HasField(TEXT("frontDistanceM")));
		TestTrue(TEXT("lidar keeps yaw key"), LidarObject->HasField(TEXT("frontYawDegree")));

		const TSharedPtr<FJsonObject> ActionObject = RobotObject->GetObjectField(TEXT("action"));
		TestTrue(TEXT("action uses brakeApplied key"), ActionObject->HasField(TEXT("brakeApplied")));
		TestFalse(TEXT("action omits bBrake wire key"), ActionObject->HasField(TEXT("bBrake")));
	}

	FEpisodeMeasurementLogEventRecord Event;
	Event.EventIndex = 0;
	Event.WorldTimeSeconds = 12.5;
	Event.Kind = TEXT("near_miss");
	Event.Severity = EEpisodeMeasurementLogSeverity::Warning;
	Event.SubjectId = TEXT("robot_01");
	Event.TargetId = TEXT("obstacle_01");
	Event.Location = FVector(100.0, 200.0, 0.0);
	Event.Value = 1.5;

	JsonLine.Reset();
	Diagnostics.Reset();
	TestTrue(TEXT("event serializes"), FEpisodeMeasurementLogJson::TryWriteEventLine(Event, JsonLine, Diagnostics));
	TestFalse(TEXT("event diagnostics has no error"), FEpisodeMeasurementLogJson::HasError(Diagnostics));

	TSharedPtr<FJsonObject> EventObject;
	TestTrue(TEXT("event parses"), ParseMeasurementLogJsonLine(JsonLine, EventObject));
	if (EventObject.IsValid())
	{
		TestEqual(TEXT("event type"), EventObject->GetStringField(TEXT("type")), FString(TEXT("event")));
		TestEqual(TEXT("event kind"), EventObject->GetStringField(TEXT("kind")), FString(TEXT("near_miss")));
		TestEqual(TEXT("event severity"), EventObject->GetStringField(TEXT("severity")), FString(TEXT("warning")));
	}

	FEpisodeMeasurementLogFooterRecord Footer;
	Footer.Ticks = 1;
	Footer.Events = 1;
	Footer.CloseReason = TEXT("pie_end");
	Footer.Diagnostics.Add(FEpisodeMeasurementLogJson::MakeDiagnostic(
		EEpisodeMeasurementLogSeverity::Warning,
		TEXT("sample_warning"),
		TEXT("sample warning")));

	JsonLine.Reset();
	Diagnostics.Reset();
	TestTrue(TEXT("footer serializes"), FEpisodeMeasurementLogJson::TryWriteFooterLine(Footer, JsonLine, Diagnostics));
	TestFalse(TEXT("footer diagnostics has no error"), FEpisodeMeasurementLogJson::HasError(Diagnostics));

	TSharedPtr<FJsonObject> FooterObject;
	TestTrue(TEXT("footer parses"), ParseMeasurementLogJsonLine(JsonLine, FooterObject));
	if (FooterObject.IsValid())
	{
		TestEqual(TEXT("footer type"), FooterObject->GetStringField(TEXT("type")), FString(TEXT("footer")));
		TestEqual(TEXT("footer ticks"), static_cast<int32>(FooterObject->GetNumberField(TEXT("ticks"))), 1);
		TestEqual(TEXT("footer diagnostics count"), FooterObject->GetArrayField(TEXT("diagnostics")).Num(), 1);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEpisodeMeasurementLogValidationTest,
	"ProtoRobotSim.MeasurementLog.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEpisodeMeasurementLogValidationTest::RunTest(const FString& Parameters)
{
	FEpisodeMeasurementLogHeaderRecord Header;
	Header.LogId = TEXT("log_bad");
	Header.MapName = TEXT("EpisodeSandbox");

	FEpisodeMeasurementLogActorInfo BadActor;
	BadActor.Index = 3;
	Header.Actors.Add(BadActor);

	FString JsonLine;
	TArray<FEpisodeMeasurementLogDiagnostic> Diagnostics;
	TestFalse(TEXT("invalid header does not serialize"), FEpisodeMeasurementLogJson::TryWriteHeaderLine(Header, JsonLine, Diagnostics));
	TestTrue(TEXT("invalid header reports error"), FEpisodeMeasurementLogJson::HasError(Diagnostics));
	TestTrue(TEXT("invalid header reports actor index code"), HasMeasurementLogDiagnosticCode(Diagnostics, TEXT("non_contiguous_actor_index")));
	TestTrue(TEXT("invalid header reports actor id code"), HasMeasurementLogDiagnosticCode(Diagnostics, TEXT("missing_actor_id")));

	FEpisodeMeasurementLogTickRecord BadTick;
	BadTick.TickIndex = -1;
	BadTick.WorldTimeSeconds = 1.0;
	BadTick.Robot.Truth.RotationQuatXyzw = { 0.0, 0.0, 0.0 };
	Diagnostics.Reset();
	TestFalse(TEXT("invalid tick does not serialize"), FEpisodeMeasurementLogJson::TryWriteTickLine(BadTick, JsonLine, Diagnostics));
	TestTrue(TEXT("invalid tick reports error"), FEpisodeMeasurementLogJson::HasError(Diagnostics));
	TestTrue(TEXT("invalid tick reports index code"), HasMeasurementLogDiagnosticCode(Diagnostics, TEXT("invalid_tick_index")));
	TestTrue(TEXT("invalid tick reports rotation code"), HasMeasurementLogDiagnosticCode(Diagnostics, TEXT("invalid_robot_rotation")));

	FEpisodeMeasurementLogEventRecord BadEvent;
	BadEvent.EventIndex = 0;
	BadEvent.WorldTimeSeconds = 1.0;
	Diagnostics.Reset();
	TestFalse(TEXT("invalid event does not serialize"), FEpisodeMeasurementLogJson::TryWriteEventLine(BadEvent, JsonLine, Diagnostics));
	TestTrue(TEXT("invalid event reports error"), FEpisodeMeasurementLogJson::HasError(Diagnostics));
	TestTrue(TEXT("invalid event reports kind code"), HasMeasurementLogDiagnosticCode(Diagnostics, TEXT("missing_event_kind")));

	TArray<FEpisodeMeasurementLogDiagnostic> FailureDiagnostics;
	FailureDiagnostics.Add(FEpisodeMeasurementLogJson::MakeDiagnostic(
		EEpisodeMeasurementLogSeverity::Failure,
		TEXT("fatal_contract_error"),
		TEXT("fatal contract error")));
	TestTrue(TEXT("failure severity counts as error"), FEpisodeMeasurementLogJson::HasError(FailureDiagnostics));

	return true;
}

#endif
