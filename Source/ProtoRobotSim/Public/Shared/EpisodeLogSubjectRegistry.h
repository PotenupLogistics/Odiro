#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodeMeasurementLogTypes.h"
#include "UObject/Object.h"
#include "EpisodeLogSubjectRegistry.generated.h"

class AActor;
class UEpisodePlaceableComponent;
class UWorld;

/// Builds the measurement log actor table from episode placeable actors.
UCLASS(BlueprintType)
class PROTOROBOTSIM_API UEpisodeLogSubjectRegistry : public UObject
{
	GENERATED_BODY()

public:
	/// Clears actor table, moving actors, lookup maps, and diagnostics.
	void Reset();

	/// Rebuilds the actor table from all placeable actors in the world.
	bool BuildFromWorld(UWorld* World, double WorldTimeSeconds = 0.0);

	/// Rebuilds the actor table from explicit placeable components.
	bool BuildFromComponents(
		const TArray<UEpisodePlaceableComponent*>& PlaceableComponents,
		double WorldTimeSeconds = 0.0);

	/// Reports placeable actors discovered after the table was built.
	bool DetectNewSubjectsInWorld(UWorld* World, double WorldTimeSeconds = 0.0);

	/// Reports explicit placeable components discovered after the table was built.
	bool DetectNewSubjects(
		const TArray<UEpisodePlaceableComponent*>& PlaceableComponents,
		double WorldTimeSeconds = 0.0);

	/// Returns the stable actor table written to the log header.
	const TArray<FEpisodeMeasurementLogActorInfo>& GetActorTable() const;

	/// Returns actors whose MobilityMode is Moving.
	const TArray<TWeakObjectPtr<AActor>>& GetMovingActors() const;

	/// Returns diagnostics generated while building or refreshing the registry.
	const TArray<FEpisodeMeasurementLogDiagnostic>& GetDiagnostics() const;

	/// Finds the actor table index for an instance id.
	int32 FindActorIndexById(const FString& InstanceId) const;

	/// Returns the actor pointer for an actor table index.
	AActor* GetActorByIndex(int32 ActorIndex) const;

	/// Returns true when any diagnostic is Error or Failure severity.
	bool HasBlockingDiagnostic() const;

private:
	/// Placeable candidate with its owning actor.
	struct FSubjectCandidate
	{
		TWeakObjectPtr<AActor> Actor;
		TWeakObjectPtr<UEpisodePlaceableComponent> PlaceableComponent;
	};

	TArray<FEpisodeMeasurementLogActorInfo> ActorTable;
	TArray<TWeakObjectPtr<AActor>> ActorsByIndex;
	TArray<TWeakObjectPtr<AActor>> MovingActors;
	TArray<FEpisodeMeasurementLogDiagnostic> Diagnostics;
	TMap<FString, int32> ActorIndexById;
	TSet<FString> ReportedDynamicInstanceIds;
	TSet<FString> ReportedDynamicActorNames;

	static void CollectWorldPlaceables(
		UWorld* World,
		TArray<UEpisodePlaceableComponent*>& OutPlaceableComponents);

	static bool MakeCandidate(
		UEpisodePlaceableComponent* PlaceableComponent,
		FSubjectCandidate& OutCandidate);

	void AddDiagnostic(
		EEpisodeMeasurementLogSeverity Severity,
		const FString& Code,
		const FString& Message,
		double WorldTimeSeconds);

	bool AddCandidateToTable(const FSubjectCandidate& Candidate, double WorldTimeSeconds);
	bool IsKnownSubject(const FSubjectCandidate& Candidate) const;
	void ReportNewSubject(const FSubjectCandidate& Candidate, double WorldTimeSeconds);
};
