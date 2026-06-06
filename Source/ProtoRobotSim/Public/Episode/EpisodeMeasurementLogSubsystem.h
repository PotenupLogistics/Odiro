#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodeConfigTypes.h"
#include "Shared/EpisodeJsonlMeasurementWriter.h"
#include "Shared/EpisodeMeasurementLogTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "EpisodeMeasurementLogSubsystem.generated.h"

class UEpisodeLogSubjectRegistry;
class UEpisodeEvaluationSubsystem;
class ADeliveryBot_ChaosActor;

/// Owns measurement log lifecycle for a PIE or game world.
UCLASS(BlueprintType)
class PROTOROBOTSIM_API UEpisodeMeasurementLogSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/// Runtime-configurable measurement log settings.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FEpisodeMeasurementLogSettings Settings;

	/// Applies simulator setup settings and optionally restarts an already-open log.
	void ApplySettings(const FEpisodeMeasurementLogSettings& settings, bool bRestartIfLogging = true);

	/// Starts a measurement log with currently known world metadata.
	UFUNCTION(BlueprintCallable, Category = "Episode|MeasurementLog")
	bool StartLogging();

	/// Stops the active log and writes a footer.
	UFUNCTION(BlueprintCallable, Category = "Episode|MeasurementLog")
	void StopLogging(const FString& CloseReason);

	/// Appends an event record immediately.
	UFUNCTION(BlueprintCallable, Category = "Episode|MeasurementLog")
	bool WriteEventRecord(const FEpisodeMeasurementLogEventRecord& EventRecord);

	/// Records a collision as an immediate event record.
	UFUNCTION(BlueprintCallable, Category = "Episode|MeasurementLog")
	bool RecordCollisionEvent(
		const FString& SubjectId,
		const FString& TargetId,
		FVector Location,
		double Value);

	/// Records an emergency stop as an immediate event record.
	UFUNCTION(BlueprintCallable, Category = "Episode|MeasurementLog")
	bool RecordEmergencyStopEvent(
		const FString& SubjectId,
		const FString& TargetId,
		FVector Location,
		double Value,
		const FString& Reason);

	/// Returns true while the subsystem owns an open writer.
	UFUNCTION(BlueprintPure, Category = "Episode|MeasurementLog")
	bool IsLogging() const;

	/// Returns the absolute path of the active log file.
	UFUNCTION(BlueprintPure, Category = "Episode|MeasurementLog")
	FString GetCurrentLogPath() const;

	/// Returns diagnostics collected during the active log.
	UFUNCTION(BlueprintPure, Category = "Episode|MeasurementLog")
	const TArray<FEpisodeMeasurementLogDiagnostic>& GetDiagnostics() const;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void OnWorldEndPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UEpisodeLogSubjectRegistry> SubjectRegistry;

	UPROPERTY(Transient)
	TObjectPtr<UEpisodeEvaluationSubsystem> BoundEvaluationSubsystem;

	TUniquePtr<FEpisodeJsonlMeasurementWriter> Writer;
	TArray<FEpisodeMeasurementLogDiagnostic> Diagnostics;
	FString CurrentLogPath;
	/// Actor table indexes are stable for the lifetime of one active log.
	TSet<int32> ReportedInvalidMovingActorIndexes;
	TSet<FString> ReportedUnknownEventKinds;
	int32 NextTickIndex = 0;
	int32 NextEventIndex = 0;
	int32 AppendedRegistryDiagnosticCount = 0;
	bool bStartAttempted = false;
	bool bStopping = false;
	/// True after the header record has been written for the active log.
	bool bHeaderWritten = false;
	bool bReportedMissingRobot = false;

	UFUNCTION()
	void HandleEvaluationEvent(FEpisodeEvaluationEvent Event);

	bool OpenWriter(double WorldTimeSeconds);
	bool EnsureHeaderWritten(double WorldTimeSeconds);
	bool WriteHeader(double WorldTimeSeconds);
	bool WriteTick(float DeltaTime);
	bool WriteFooter(const FString& CloseReason);
	FEpisodeMeasurementLogTickRecord BuildTickRecord(float DeltaTime);
	FEpisodeMeasurementLogHeaderRecord BuildHeaderRecord(double WorldTimeSeconds) const;
	ADeliveryBot_ChaosActor* FindRobotActor() const;
	void CaptureMovingActors(FEpisodeMeasurementLogTickRecord& TickRecord, AActor* RobotActor);
	FString BuildOutputPath() const;
	FString GetCleanMapName() const;
	void AddDiagnostic(
		EEpisodeMeasurementLogSeverity Severity,
		const FString& Code,
		const FString& Message,
		double WorldTimeSeconds = 0.0);
	void AppendRegistryDiagnostics();
	void BindEvaluationSubsystem();
	void UnbindEvaluationSubsystem();
};
