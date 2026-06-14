#if WITH_DEV_AUTOMATION_TESTS

#include "Shared/EpisodeJsonlMeasurementWriter.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	bool ParseWriterJsonLine(const FString& JsonLine, TSharedPtr<FJsonObject>& OutObject)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonLine);
		return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEpisodeJsonlMeasurementWriterStreamingTest,
	"OdiroSim.MeasurementLog.JsonlWriter.Streaming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEpisodeJsonlMeasurementWriterStreamingTest::RunTest(const FString& Parameters)
{
	const FString OutputDirectory = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("Automation"),
		TEXT("MeasurementLogWriter"));
	const FString OutputPath = FPaths::Combine(
		OutputDirectory,
		FString::Printf(TEXT("Writer_%s.jsonl"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));

	FEpisodeJsonlMeasurementWriter Writer;
	TArray<FEpisodeMeasurementLogDiagnostic> Diagnostics;
	TestTrue(TEXT("writer opens"), Writer.Open(OutputPath, Diagnostics));
	TestTrue(TEXT("writer reports open"), Writer.IsOpen());

	FEpisodeMeasurementLogHeaderRecord Header;
	Header.LogId = TEXT("writer_test");
	Header.MapName = TEXT("AutomationMap");

	FEpisodeMeasurementLogTickRecord Tick;
	Tick.TickIndex = 0;
	Tick.WorldTimeSeconds = 1.0;
	Tick.DeltaSeconds = 0.016;
	Tick.Robot.Id = TEXT("robot_01");
	Tick.Robot.Truth.RotationQuatXyzw = { 0.0, 0.0, 0.0, 1.0 };

	FEpisodeMeasurementLogEventRecord Event;
	Event.EventIndex = 0;
	Event.WorldTimeSeconds = 1.25;
	Event.Kind = TEXT("writer_event");

	FEpisodeMeasurementLogFooterRecord Footer;
	Footer.Ticks = 1;
	Footer.Events = 1;
	Footer.CloseReason = TEXT("test_close");

	TestTrue(TEXT("header writes"), Writer.WriteHeader(Header, Diagnostics));
	TestTrue(TEXT("tick writes"), Writer.WriteTick(Tick, Diagnostics));
	TestTrue(TEXT("event writes"), Writer.WriteEvent(Event, Diagnostics));
	TestTrue(TEXT("footer writes"), Writer.WriteFooter(Footer, Diagnostics));
	Writer.Close();
	TestFalse(TEXT("writer closes"), Writer.IsOpen());
	TestFalse(TEXT("writer diagnostics has no error"), FEpisodeMeasurementLogJson::HasError(Diagnostics));

	FString FileContent;
	TestTrue(TEXT("log file loads"), FFileHelper::LoadFileToString(FileContent, *OutputPath));
	TestFalse(TEXT("jsonl uses LF without CR"), FileContent.Contains(TEXT("\r")));

	TArray<FString> Lines;
	FileContent.ParseIntoArrayLines(Lines, true);
	TestEqual(TEXT("jsonl line count"), Lines.Num(), 4);

	const TCHAR* ExpectedTypes[] = { TEXT("header"), TEXT("tick"), TEXT("event"), TEXT("footer") };
	for (int32 Index = 0; Index < Lines.Num() && Index < 4; ++Index)
	{
		TestFalse(FString::Printf(TEXT("line %d has no CR"), Index), Lines[Index].Contains(TEXT("\r")));

		TSharedPtr<FJsonObject> Object;
		TestTrue(FString::Printf(TEXT("line %d parses"), Index), ParseWriterJsonLine(Lines[Index], Object));
		if (Object.IsValid())
		{
			TestEqual(
				FString::Printf(TEXT("line %d type"), Index),
				Object->GetStringField(TEXT("type")),
				FString(ExpectedTypes[Index]));
		}
	}

	TUniquePtr<FArchive> ReopenedFile(IFileManager::Get().CreateFileReader(*OutputPath));
	TestTrue(TEXT("output file can be reopened after close"), ReopenedFile.IsValid());
	return true;
}

#endif
