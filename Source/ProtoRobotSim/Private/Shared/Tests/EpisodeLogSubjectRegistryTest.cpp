#if WITH_DEV_AUTOMATION_TESTS

#include "Shared/EpisodeLogSubjectRegistry.h"

#include "Episode/Components/EpisodePlaceableComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

namespace
{
	UEpisodePlaceableComponent* MakePlaceableComponent(
		const FString& InstanceId,
		const FString& AssetId,
		EEpisodeActorCategory Category,
		EEpisodeMobilityMode MobilityMode)
	{
		AActor* Actor = NewObject<AActor>(GetTransientPackage());
		UEpisodePlaceableComponent* PlaceableComponent = NewObject<UEpisodePlaceableComponent>(Actor);
		Actor->AddInstanceComponent(PlaceableComponent);

		PlaceableComponent->InstanceId = InstanceId;
		PlaceableComponent->AssetId = AssetId;
		PlaceableComponent->Category = Category;
		PlaceableComponent->MobilityMode = MobilityMode;
		return PlaceableComponent;
	}

	bool HasSubjectRegistryDiagnosticCode(
		const TArray<FEpisodeMeasurementLogDiagnostic>& Diagnostics,
		const FString& Code)
	{
		return Diagnostics.ContainsByPredicate(
			[&Code](const FEpisodeMeasurementLogDiagnostic& Diagnostic)
			{
				return Diagnostic.Code == Code;
			});
	}

	const FEpisodeMeasurementLogActorInfo* FindActorInfo(
		const TArray<FEpisodeMeasurementLogActorInfo>& ActorTable,
		const FString& InstanceId)
	{
		return ActorTable.FindByPredicate(
			[&InstanceId](const FEpisodeMeasurementLogActorInfo& ActorInfo)
			{
				return ActorInfo.Id == InstanceId;
			});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEpisodeLogSubjectRegistryTableTest,
	"ProtoRobotSim.MeasurementLog.SubjectRegistry.ActorTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEpisodeLogSubjectRegistryTableTest::RunTest(const FString& Parameters)
{
	TArray<UEpisodePlaceableComponent*> PlaceableComponents;
	PlaceableComponents.Add(MakePlaceableComponent(
		TEXT("static_01"),
		TEXT("trash_bag"),
		EEpisodeActorCategory::StaticObstacle,
		EEpisodeMobilityMode::Static));
	PlaceableComponents.Add(MakePlaceableComponent(
		TEXT("robot_01"),
		TEXT("delivery_bot"),
		EEpisodeActorCategory::DeliveryBot,
		EEpisodeMobilityMode::Moving));
	PlaceableComponents.Add(MakePlaceableComponent(
		TEXT("ped_01"),
		TEXT("adult_pedestrian"),
		EEpisodeActorCategory::Pedestrian,
		EEpisodeMobilityMode::Moving));

	UEpisodeLogSubjectRegistry* Registry = NewObject<UEpisodeLogSubjectRegistry>();
	TestTrue(TEXT("registry builds from placeables"), Registry->BuildFromComponents(PlaceableComponents));
	TestFalse(TEXT("registry has no blocking diagnostics"), Registry->HasBlockingDiagnostic());

	const TArray<FEpisodeMeasurementLogActorInfo>& ActorTable = Registry->GetActorTable();
	TestEqual(TEXT("actor table count"), ActorTable.Num(), 3);

	for (int32 Index = 0; Index < ActorTable.Num(); ++Index)
	{
		TestEqual(TEXT("actor index is contiguous"), ActorTable[Index].Index, Index);
		TestNotNull(TEXT("actor pointer exists by index"), Registry->GetActorByIndex(Index));
	}

	const FEpisodeMeasurementLogActorInfo* RobotInfo = FindActorInfo(ActorTable, TEXT("robot_01"));
	const FEpisodeMeasurementLogActorInfo* PedestrianInfo = FindActorInfo(ActorTable, TEXT("ped_01"));
	const FEpisodeMeasurementLogActorInfo* StaticInfo = FindActorInfo(ActorTable, TEXT("static_01"));

	TestNotNull(TEXT("robot is in actor table"), RobotInfo);
	TestNotNull(TEXT("pedestrian is in actor table"), PedestrianInfo);
	TestNotNull(TEXT("static obstacle is in actor table"), StaticInfo);

	if (RobotInfo)
	{
		TestEqual(TEXT("robot category"), RobotInfo->ActorCategory, EEpisodeActorCategory::DeliveryBot);
		TestEqual(TEXT("robot actor index lookup"), Registry->FindActorIndexById(TEXT("robot_01")), RobotInfo->Index);
	}

	if (PedestrianInfo)
	{
		TestEqual(TEXT("pedestrian mobility"), PedestrianInfo->Mobility, EEpisodeMobilityMode::Moving);
	}

	if (StaticInfo)
	{
		TestEqual(TEXT("static obstacle mobility"), StaticInfo->Mobility, EEpisodeMobilityMode::Static);
	}

	TestEqual(TEXT("moving actor count"), Registry->GetMovingActors().Num(), 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEpisodeLogSubjectRegistryValidationTest,
	"ProtoRobotSim.MeasurementLog.SubjectRegistry.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEpisodeLogSubjectRegistryValidationTest::RunTest(const FString& Parameters)
{
	TArray<UEpisodePlaceableComponent*> PlaceableComponents;
	PlaceableComponents.Add(MakePlaceableComponent(
		TEXT("dup_01"),
		TEXT("delivery_bot"),
		EEpisodeActorCategory::DeliveryBot,
		EEpisodeMobilityMode::Moving));
	PlaceableComponents.Add(MakePlaceableComponent(
		TEXT("dup_01"),
		TEXT("adult_pedestrian"),
		EEpisodeActorCategory::Pedestrian,
		EEpisodeMobilityMode::Moving));
	PlaceableComponents.Add(MakePlaceableComponent(
		FString(),
		TEXT("trash_bag"),
		EEpisodeActorCategory::StaticObstacle,
		EEpisodeMobilityMode::Static));

	UEpisodeLogSubjectRegistry* Registry = NewObject<UEpisodeLogSubjectRegistry>();
	TestFalse(TEXT("duplicate or missing identity blocks registry"), Registry->BuildFromComponents(PlaceableComponents));
	TestTrue(TEXT("registry has blocking diagnostics"), Registry->HasBlockingDiagnostic());
	TestTrue(
		TEXT("duplicate instance id diagnostic"),
		HasSubjectRegistryDiagnosticCode(Registry->GetDiagnostics(), TEXT("duplicate_instance_id")));
	TestTrue(
		TEXT("missing instance id diagnostic"),
		HasSubjectRegistryDiagnosticCode(Registry->GetDiagnostics(), TEXT("missing_instance_id")));

	const TArray<FEpisodeMeasurementLogActorInfo>& ActorTable = Registry->GetActorTable();
	for (int32 Index = 0; Index < ActorTable.Num(); ++Index)
	{
		TestEqual(TEXT("valid actor indexes stay contiguous"), ActorTable[Index].Index, Index);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEpisodeLogSubjectRegistryDynamicTest,
	"ProtoRobotSim.MeasurementLog.SubjectRegistry.DynamicDiscovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEpisodeLogSubjectRegistryDynamicTest::RunTest(const FString& Parameters)
{
	UEpisodePlaceableComponent* RobotComponent = MakePlaceableComponent(
		TEXT("robot_01"),
		TEXT("delivery_bot"),
		EEpisodeActorCategory::DeliveryBot,
		EEpisodeMobilityMode::Moving);
	UEpisodePlaceableComponent* PedestrianComponent = MakePlaceableComponent(
		TEXT("ped_late_01"),
		TEXT("adult_pedestrian"),
		EEpisodeActorCategory::Pedestrian,
		EEpisodeMobilityMode::Moving);

	UEpisodeLogSubjectRegistry* Registry = NewObject<UEpisodeLogSubjectRegistry>();
	TestTrue(TEXT("initial registry builds"), Registry->BuildFromComponents({ RobotComponent }));
	TestEqual(TEXT("initial actor table count"), Registry->GetActorTable().Num(), 1);

	TestTrue(
		TEXT("dynamic subject warning does not block"),
		Registry->DetectNewSubjects({ RobotComponent, PedestrianComponent }, 3.5));
	TestEqual(TEXT("dynamic subject is not added to actor table"), Registry->GetActorTable().Num(), 1);
	TestTrue(
		TEXT("dynamic subject diagnostic"),
		HasSubjectRegistryDiagnosticCode(Registry->GetDiagnostics(), TEXT("dynamic_subject_discovered")));

	return true;
}

#endif
