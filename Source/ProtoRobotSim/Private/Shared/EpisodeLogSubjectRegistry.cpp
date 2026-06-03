#include "Shared/EpisodeLogSubjectRegistry.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Episode/Components/EpisodePlaceableComponent.h"
#include "GameFramework/Actor.h"

namespace
{
	FString GetSubjectActorName(const AActor* Actor)
	{
		return IsValid(Actor) ? Actor->GetName() : FString(TEXT("<invalid>"));
	}

	bool IsValidActorCategory(EEpisodeActorCategory Category)
	{
		const UEnum* Enum = StaticEnum<EEpisodeActorCategory>();
		return Enum && Enum->IsValidEnumValue(static_cast<int64>(Category));
	}

	bool IsValidMobilityMode(EEpisodeMobilityMode MobilityMode)
	{
		const UEnum* Enum = StaticEnum<EEpisodeMobilityMode>();
		return Enum && Enum->IsValidEnumValue(static_cast<int64>(MobilityMode));
	}
}

void UEpisodeLogSubjectRegistry::Reset()
{
	ActorTable.Reset();
	ActorsByIndex.Reset();
	MovingActors.Reset();
	Diagnostics.Reset();
	ActorIndexById.Reset();
	ReportedDynamicInstanceIds.Reset();
	ReportedDynamicActorNames.Reset();
}

bool UEpisodeLogSubjectRegistry::BuildFromWorld(UWorld* World, double WorldTimeSeconds)
{
	Reset();

	if (!IsValid(World))
	{
		AddDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("missing_world"),
			TEXT("A valid world is required to build the measurement log subject registry."),
			WorldTimeSeconds);
		return false;
	}

	TArray<UEpisodePlaceableComponent*> PlaceableComponents;
	CollectWorldPlaceables(World, PlaceableComponents);
	return BuildFromComponents(PlaceableComponents, WorldTimeSeconds);
}

bool UEpisodeLogSubjectRegistry::BuildFromComponents(
	const TArray<UEpisodePlaceableComponent*>& PlaceableComponents,
	double WorldTimeSeconds)
{
	Reset();

	TArray<FSubjectCandidate> Candidates;
	Candidates.Reserve(PlaceableComponents.Num());

	for (UEpisodePlaceableComponent* PlaceableComponent : PlaceableComponents)
	{
		FSubjectCandidate Candidate;
		if (!MakeCandidate(PlaceableComponent, Candidate))
		{
			AddDiagnostic(
				EEpisodeMeasurementLogSeverity::Warning,
				TEXT("invalid_placeable_component"),
				TEXT("A null or ownerless placeable component was skipped."),
				WorldTimeSeconds);
			continue;
		}

		Candidates.Add(Candidate);
	}

	Candidates.Sort(
		[](const FSubjectCandidate& Left, const FSubjectCandidate& Right)
		{
			const UEpisodePlaceableComponent* LeftComponent = Left.PlaceableComponent.Get();
			const UEpisodePlaceableComponent* RightComponent = Right.PlaceableComponent.Get();
			const AActor* LeftActor = Left.Actor.Get();
			const AActor* RightActor = Right.Actor.Get();

			const FString LeftId = LeftComponent ? LeftComponent->InstanceId : FString();
			const FString RightId = RightComponent ? RightComponent->InstanceId : FString();
			const int32 IdCompare = LeftId.Compare(RightId, ESearchCase::CaseSensitive);
			if (IdCompare != 0)
			{
				return IdCompare < 0;
			}

			return GetSubjectActorName(LeftActor) < GetSubjectActorName(RightActor);
		});

	for (const FSubjectCandidate& Candidate : Candidates)
	{
		AddCandidateToTable(Candidate, WorldTimeSeconds);
	}

	return !HasBlockingDiagnostic();
}

bool UEpisodeLogSubjectRegistry::DetectNewSubjectsInWorld(UWorld* World, double WorldTimeSeconds)
{
	if (!IsValid(World))
	{
		AddDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("missing_world"),
			TEXT("A valid world is required to detect new measurement log subjects."),
			WorldTimeSeconds);
		return false;
	}

	TArray<UEpisodePlaceableComponent*> PlaceableComponents;
	CollectWorldPlaceables(World, PlaceableComponents);
	return DetectNewSubjects(PlaceableComponents, WorldTimeSeconds);
}

bool UEpisodeLogSubjectRegistry::DetectNewSubjects(
	const TArray<UEpisodePlaceableComponent*>& PlaceableComponents,
	double WorldTimeSeconds)
{
	for (UEpisodePlaceableComponent* PlaceableComponent : PlaceableComponents)
	{
		FSubjectCandidate Candidate;
		if (!MakeCandidate(PlaceableComponent, Candidate))
		{
			continue;
		}

		if (!IsKnownSubject(Candidate))
		{
			ReportNewSubject(Candidate, WorldTimeSeconds);
		}
	}

	return !HasBlockingDiagnostic();
}

const TArray<FEpisodeMeasurementLogActorInfo>& UEpisodeLogSubjectRegistry::GetActorTable() const
{
	return ActorTable;
}

const TArray<TWeakObjectPtr<AActor>>& UEpisodeLogSubjectRegistry::GetMovingActors() const
{
	return MovingActors;
}

const TArray<FEpisodeMeasurementLogDiagnostic>& UEpisodeLogSubjectRegistry::GetDiagnostics() const
{
	return Diagnostics;
}

int32 UEpisodeLogSubjectRegistry::FindActorIndexById(const FString& InstanceId) const
{
	if (const int32* FoundIndex = ActorIndexById.Find(InstanceId))
	{
		return *FoundIndex;
	}

	return INDEX_NONE;
}

AActor* UEpisodeLogSubjectRegistry::GetActorByIndex(int32 ActorIndex) const
{
	return ActorsByIndex.IsValidIndex(ActorIndex)
		? ActorsByIndex[ActorIndex].Get()
		: nullptr;
}

bool UEpisodeLogSubjectRegistry::HasBlockingDiagnostic() const
{
	return FEpisodeMeasurementLogJson::HasError(Diagnostics);
}

void UEpisodeLogSubjectRegistry::CollectWorldPlaceables(
	UWorld* World,
	TArray<UEpisodePlaceableComponent*>& OutPlaceableComponents)
{
	OutPlaceableComponents.Reset();

	if (!IsValid(World))
	{
		return;
	}

	for (TActorIterator<AActor> ActorIterator(World); ActorIterator; ++ActorIterator)
	{
		AActor* Actor = *ActorIterator;
		if (!IsValid(Actor))
		{
			continue;
		}

		if (UEpisodePlaceableComponent* PlaceableComponent = Actor->FindComponentByClass<UEpisodePlaceableComponent>())
		{
			OutPlaceableComponents.Add(PlaceableComponent);
		}
	}
}

bool UEpisodeLogSubjectRegistry::MakeCandidate(
	UEpisodePlaceableComponent* PlaceableComponent,
	FSubjectCandidate& OutCandidate)
{
	if (!IsValid(PlaceableComponent))
	{
		return false;
	}

	AActor* Owner = PlaceableComponent->GetOwner();
	if (!IsValid(Owner))
	{
		return false;
	}

	OutCandidate.Actor = Owner;
	OutCandidate.PlaceableComponent = PlaceableComponent;
	return true;
}

void UEpisodeLogSubjectRegistry::AddDiagnostic(
	EEpisodeMeasurementLogSeverity Severity,
	const FString& Code,
	const FString& Message,
	double WorldTimeSeconds)
{
	Diagnostics.Add(FEpisodeMeasurementLogJson::MakeDiagnostic(
		Severity,
		Code,
		Message,
		WorldTimeSeconds));
}

bool UEpisodeLogSubjectRegistry::AddCandidateToTable(
	const FSubjectCandidate& Candidate,
	double WorldTimeSeconds)
{
	AActor* Actor = Candidate.Actor.Get();
	UEpisodePlaceableComponent* PlaceableComponent = Candidate.PlaceableComponent.Get();
	if (!IsValid(Actor) || !IsValid(PlaceableComponent))
	{
		AddDiagnostic(
			EEpisodeMeasurementLogSeverity::Warning,
			TEXT("invalid_subject"),
			TEXT("An invalid placeable actor was skipped."),
			WorldTimeSeconds);
		return false;
	}

	const FString ActorName = GetSubjectActorName(Actor);
	if (PlaceableComponent->InstanceId.IsEmpty())
	{
		AddDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("missing_instance_id"),
			FString::Printf(TEXT("Placeable actor '%s' is missing InstanceId."), *ActorName),
			WorldTimeSeconds);
		return false;
	}

	if (ActorIndexById.Contains(PlaceableComponent->InstanceId))
	{
		AddDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("duplicate_instance_id"),
			FString::Printf(
				TEXT("Duplicate placeable InstanceId '%s' found on actor '%s'."),
				*PlaceableComponent->InstanceId,
				*ActorName),
			WorldTimeSeconds);
		return false;
	}

	if (PlaceableComponent->AssetId.IsEmpty())
	{
		AddDiagnostic(
			EEpisodeMeasurementLogSeverity::Warning,
			TEXT("missing_asset_id"),
			FString::Printf(
				TEXT("Placeable actor '%s' with InstanceId '%s' is missing AssetId."),
				*ActorName,
				*PlaceableComponent->InstanceId),
			WorldTimeSeconds);
	}

	if (!IsValidActorCategory(PlaceableComponent->Category))
	{
		AddDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("invalid_actor_category"),
			FString::Printf(
				TEXT("Placeable actor '%s' with InstanceId '%s' has an invalid Category."),
				*ActorName,
				*PlaceableComponent->InstanceId),
			WorldTimeSeconds);
		return false;
	}

	if (!IsValidMobilityMode(PlaceableComponent->MobilityMode))
	{
		AddDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("invalid_mobility_mode"),
			FString::Printf(
				TEXT("Placeable actor '%s' with InstanceId '%s' has an invalid MobilityMode."),
				*ActorName,
				*PlaceableComponent->InstanceId),
			WorldTimeSeconds);
		return false;
	}

	const int32 ActorIndex = ActorTable.Num();

	FEpisodeMeasurementLogActorInfo ActorInfo;
	ActorInfo.Index = ActorIndex;
	ActorInfo.Id = PlaceableComponent->InstanceId;
	ActorInfo.AssetId = PlaceableComponent->AssetId;
	ActorInfo.ActorCategory = PlaceableComponent->Category;
	ActorInfo.Mobility = PlaceableComponent->MobilityMode;

	ActorTable.Add(ActorInfo);
	ActorsByIndex.Add(Actor);
	ActorIndexById.Add(ActorInfo.Id, ActorIndex);

	if (ActorInfo.Mobility == EEpisodeMobilityMode::Moving)
	{
		MovingActors.Add(Actor);
	}

	return true;
}

bool UEpisodeLogSubjectRegistry::IsKnownSubject(const FSubjectCandidate& Candidate) const
{
	const UEpisodePlaceableComponent* PlaceableComponent = Candidate.PlaceableComponent.Get();
	if (!PlaceableComponent || PlaceableComponent->InstanceId.IsEmpty())
	{
		return false;
	}

	return ActorIndexById.Contains(PlaceableComponent->InstanceId);
}

void UEpisodeLogSubjectRegistry::ReportNewSubject(
	const FSubjectCandidate& Candidate,
	double WorldTimeSeconds)
{
	const AActor* Actor = Candidate.Actor.Get();
	const UEpisodePlaceableComponent* PlaceableComponent = Candidate.PlaceableComponent.Get();
	const FString ActorName = GetSubjectActorName(Actor);

	if (!PlaceableComponent || PlaceableComponent->InstanceId.IsEmpty())
	{
		if (ReportedDynamicActorNames.Contains(ActorName))
		{
			return;
		}

		ReportedDynamicActorNames.Add(ActorName);
		AddDiagnostic(
			EEpisodeMeasurementLogSeverity::Warning,
			TEXT("dynamic_subject_missing_instance_id"),
			FString::Printf(
				TEXT("Runtime placeable actor '%s' was discovered after the actor table build but has no InstanceId."),
				*ActorName),
			WorldTimeSeconds);
		return;
	}

	if (ReportedDynamicInstanceIds.Contains(PlaceableComponent->InstanceId))
	{
		return;
	}

	ReportedDynamicInstanceIds.Add(PlaceableComponent->InstanceId);
	AddDiagnostic(
		EEpisodeMeasurementLogSeverity::Warning,
		TEXT("dynamic_subject_discovered"),
		FString::Printf(
			TEXT("Runtime placeable actor '%s' with InstanceId '%s' was discovered after the actor table build and was not added."),
			*ActorName,
			*PlaceableComponent->InstanceId),
		WorldTimeSeconds);
}
