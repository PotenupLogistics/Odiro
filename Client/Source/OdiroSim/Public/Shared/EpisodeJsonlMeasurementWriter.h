#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodeMeasurementLogTypes.h"

/// Streams validated measurement log records as UTF-8 JSONL.
class ODIROSIM_API FEpisodeJsonlMeasurementWriter
{
public:
	/// Opens a JSONL file for streaming writes.
	bool Open(
		const FString& AbsoluteFilePath,
		TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics,
		double WorldTimeSeconds = 0.0);

	/// Returns true while a file handle is open.
	bool IsOpen() const;

	/// Returns the absolute path currently owned by the writer.
	const FString& GetFilePath() const;

	/// Writes a header record and appends validation diagnostics.
	bool WriteHeader(
		const FEpisodeMeasurementLogHeaderRecord& Record,
		TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics);

	/// Writes a tick record and appends validation diagnostics.
	bool WriteTick(
		const FEpisodeMeasurementLogTickRecord& Record,
		TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics);

	/// Writes an event record and appends validation diagnostics.
	bool WriteEvent(
		const FEpisodeMeasurementLogEventRecord& Record,
		TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics);

	/// Writes a footer record and appends validation diagnostics.
	bool WriteFooter(
		const FEpisodeMeasurementLogFooterRecord& Record,
		TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics);

	/// Flushes the file handle when open.
	void Flush();

	/// Closes the file handle.
	void Close();

private:
	TUniquePtr<FArchive> FileArchive;
	FString FilePath;

	bool WriteLine(
		const FString& JsonLine,
		TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics,
		double WorldTimeSeconds);
};
