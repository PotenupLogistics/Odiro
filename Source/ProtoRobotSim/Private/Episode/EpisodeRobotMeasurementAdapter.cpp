#include "Episode/EpisodeRobotMeasurementAdapter.h"

#include "DeliveryBot/Actor/DeliveryBot.h"
#include "Episode/Components/EpisodePlaceableComponent.h"
#include "Shared/EpisodeLogSubjectRegistry.h"

bool FEpisodeRobotMeasurementAdapter::BuildRobotTick(
	ADeliveryBot* RobotActor,
	const UEpisodeLogSubjectRegistry* SubjectRegistry,
	FEpisodeMeasurementLogRobotTick& OutRobotTick,
	TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics,
	double WorldTimeSeconds)
{
	if (!IsValid(RobotActor))
	{
		OutDiagnostics.Add(FEpisodeMeasurementLogJson::MakeDiagnostic(
			EEpisodeMeasurementLogSeverity::Warning,
			TEXT("missing_robot_actor"),
			TEXT("Delivery Bot measurement snapshot skipped because the robot actor is invalid."),
			WorldTimeSeconds));
		return false;
	}

	const FString RobotId = ResolveActorId(RobotActor, SubjectRegistry);
	if (RobotId.IsEmpty())
	{
		OutDiagnostics.Add(FEpisodeMeasurementLogJson::MakeDiagnostic(
			EEpisodeMeasurementLogSeverity::Warning,
			TEXT("missing_robot_identity"),
			FString::Printf(TEXT("Delivery Bot actor '%s' has no stable InstanceId."), *RobotActor->GetName()),
			WorldTimeSeconds));
	}

	FDeliveryBotSensorSnapshot Snapshot;
	RobotActor->GetSensorSnapshot(Snapshot);

	OutRobotTick.Id = RobotId;
	OutRobotTick.Truth = BuildActorState(RobotActor);
	for (const FDeliveryBotLidarRayInfo& RayInfo : Snapshot.LidarScanInfo.RayInfos)
	{
		if (RayInfo.bHit)
		{
			++OutRobotTick.Lidar.HitCount;
		}
	}
	OutRobotTick.Lidar.bHasFrontObject = Snapshot.bHasFrontObject;

	if (Snapshot.bHasFrontObject)
	{
		const AActor* FrontActor = Snapshot.FrontObjectInfo.DetectedActor.Get();
		OutRobotTick.Lidar.FrontObjectId = ResolveActorId(FrontActor, SubjectRegistry);
		OutRobotTick.Lidar.FrontDistanceM = Snapshot.FrontObjectInfo.ClosestFrontDistanceM;
		OutRobotTick.Lidar.FrontYawDegree = Snapshot.FrontObjectInfo.ClosestFrontRayYawDegree;
	}

	FDeliveryBotMoveCommandInfo MoveCommandInfo;
	FString ActionReason;
	if (RobotActor->GetLastMoveCommandInfo(MoveCommandInfo, ActionReason))
	{
		OutRobotTick.Action.TargetSpeedKmh = MoveCommandInfo.TargetSpeedKmh;
		OutRobotTick.Action.Steering = MoveCommandInfo.Steering;
		OutRobotTick.Action.Brake = MoveCommandInfo.Brake;
		OutRobotTick.Action.bBrakeApplied = MoveCommandInfo.bBrake;
		OutRobotTick.Action.Reason = ActionReason;
	}

	return true;
}

FString FEpisodeRobotMeasurementAdapter::ResolveActorId(
	const AActor* Actor,
	const UEpisodeLogSubjectRegistry* SubjectRegistry)
{
	if (!IsValid(Actor))
	{
		return FString();
	}

	if (const UEpisodePlaceableComponent* PlaceableComponent = Actor->FindComponentByClass<UEpisodePlaceableComponent>())
	{
		if (!PlaceableComponent->InstanceId.IsEmpty())
		{
			return PlaceableComponent->InstanceId;
		}
	}

	if (SubjectRegistry)
	{
		const TArray<FEpisodeMeasurementLogActorInfo>& ActorTable = SubjectRegistry->GetActorTable();
		for (const FEpisodeMeasurementLogActorInfo& ActorInfo : ActorTable)
		{
			if (SubjectRegistry->GetActorByIndex(ActorInfo.Index) == Actor)
			{
				return ActorInfo.Id;
			}
		}
	}

	return Actor->GetName();
}

FEpisodeMeasurementLogActorState FEpisodeRobotMeasurementAdapter::BuildActorState(const AActor* Actor)
{
	FEpisodeMeasurementLogActorState State;
	if (!IsValid(Actor))
	{
		return State;
	}

	const FQuat Rotation = Actor->GetActorQuat();
	State.PositionCm = Actor->GetActorLocation();
	State.RotationQuatXyzw = { Rotation.X, Rotation.Y, Rotation.Z, Rotation.W };
	State.VelocityCmPerSecond = Actor->GetVelocity();
	return State;
}
