#include "Shared/EpisodeJsonlMeasurementWriter.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"

bool FEpisodeJsonlMeasurementWriter::Open(
	const FString& AbsoluteFilePath,
	TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics,
	double WorldTimeSeconds)
{
	Close();

	if (AbsoluteFilePath.IsEmpty())
	{
		OutDiagnostics.Add(FEpisodeMeasurementLogJson::MakeDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("missing_log_file_path"),
			TEXT("Measurement log file path is empty."),
			WorldTimeSeconds));
		return false;
	}

	const FString Directory = FPaths::GetPath(AbsoluteFilePath);
	if (!Directory.IsEmpty() && !IFileManager::Get().MakeDirectory(*Directory, true))
	{
		OutDiagnostics.Add(FEpisodeMeasurementLogJson::MakeDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("log_directory_create_failed"),
			FString::Printf(TEXT("Failed to create measurement log directory '%s'."), *Directory),
			WorldTimeSeconds));
		return false;
	}

	FileArchive.Reset(IFileManager::Get().CreateFileWriter(*AbsoluteFilePath, FILEWRITE_AllowRead));
	if (!FileArchive)
	{
		OutDiagnostics.Add(FEpisodeMeasurementLogJson::MakeDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("log_file_open_failed"),
			FString::Printf(TEXT("Failed to open measurement log file '%s'."), *AbsoluteFilePath),
			WorldTimeSeconds));
		return false;
	}

	FilePath = AbsoluteFilePath;
	return true;
}

bool FEpisodeJsonlMeasurementWriter::IsOpen() const
{
	return FileArchive.IsValid();
}

const FString& FEpisodeJsonlMeasurementWriter::GetFilePath() const
{
	return FilePath;
}

bool FEpisodeJsonlMeasurementWriter::WriteHeader(
	const FEpisodeMeasurementLogHeaderRecord& Record,
	TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics)
{
	FString JsonLine;
	if (!FEpisodeMeasurementLogJson::TryWriteHeaderLine(Record, JsonLine, OutDiagnostics))
	{
		return false;
	}

	return WriteLine(JsonLine, OutDiagnostics, 0.0);
}

bool FEpisodeJsonlMeasurementWriter::WriteTick(
	const FEpisodeMeasurementLogTickRecord& Record,
	TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics)
{
	FString JsonLine;
	if (!FEpisodeMeasurementLogJson::TryWriteTickLine(Record, JsonLine, OutDiagnostics))
	{
		return false;
	}

	return WriteLine(JsonLine, OutDiagnostics, Record.WorldTimeSeconds);
}

bool FEpisodeJsonlMeasurementWriter::WriteEvent(
	const FEpisodeMeasurementLogEventRecord& Record,
	TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics)
{
	FString JsonLine;
	if (!FEpisodeMeasurementLogJson::TryWriteEventLine(Record, JsonLine, OutDiagnostics))
	{
		return false;
	}

	return WriteLine(JsonLine, OutDiagnostics, Record.WorldTimeSeconds);
}

bool FEpisodeJsonlMeasurementWriter::WriteFooter(
	const FEpisodeMeasurementLogFooterRecord& Record,
	TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics)
{
	FString JsonLine;
	if (!FEpisodeMeasurementLogJson::TryWriteFooterLine(Record, JsonLine, OutDiagnostics))
	{
		return false;
	}

	return WriteLine(JsonLine, OutDiagnostics, 0.0);
}

void FEpisodeJsonlMeasurementWriter::Flush()
{
	if (FileArchive)
	{
		FileArchive->Flush();
	}
}

void FEpisodeJsonlMeasurementWriter::Close()
{
	if (FileArchive)
	{
		FileArchive->Flush();
		FileArchive.Reset();
	}

	FilePath.Reset();
}

bool FEpisodeJsonlMeasurementWriter::WriteLine(
	const FString& JsonLine,
	TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics,
	double WorldTimeSeconds)
{
	if (!FileArchive)
	{
		OutDiagnostics.Add(FEpisodeMeasurementLogJson::MakeDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("log_file_not_open"),
			TEXT("Measurement log file is not open."),
			WorldTimeSeconds));
		return false;
	}

	const FString LineWithTerminator = JsonLine + TEXT("\n");
	const FTCHARToUTF8 Utf8Line(*LineWithTerminator);
	FileArchive->Serialize(const_cast<ANSICHAR*>(Utf8Line.Get()), Utf8Line.Length());

	if (FileArchive->IsError())
	{
		OutDiagnostics.Add(FEpisodeMeasurementLogJson::MakeDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("log_file_write_failed"),
			FString::Printf(TEXT("Failed to write measurement log file '%s'."), *FilePath),
			WorldTimeSeconds));
		return false;
	}

	return true;
}
