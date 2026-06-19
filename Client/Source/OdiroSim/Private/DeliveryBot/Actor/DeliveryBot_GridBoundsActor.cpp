
#include "DeliveryBot/Actor/DeliveryBot_GridBoundsActor.h"
#include "Components/BoxComponent.h"
#include "DeliveryBot/Subsystem/DeliveryBot_GridSubsystem.h"

namespace
{
	FDeliveryBotGridCollisionRuleInfo MakeGridCollisionRule(
		FName collisionProfileName,
		EDeliveryBotGridAreaType areaType,
		float cost,
		bool bBlocksMovement)
	{
		FDeliveryBotGridCollisionRuleInfo rule;
		rule.CollisionProfileName = collisionProfileName;
		rule.AreaType = areaType;
		rule.Cost = cost;
		rule.bBlocksMovement = bBlocksMovement;
		return rule;
	}
}

ADeliveryBot_GridBoundsActor::ADeliveryBot_GridBoundsActor()
{
	PrimaryActorTick.bCanEverTick = false;
	
	BoundsBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundsBox"));
	SetRootComponent(BoundsBox);
	BoundsBox->SetMobility(EComponentMobility::Movable);
	BoundsBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	CollisionProfileRules =
	{
		MakeGridCollisionRule(FName(TEXT("Walkable")), EDeliveryBotGridAreaType::Walkable, 1.0f, false),
		MakeGridCollisionRule(FName(TEXT("Penalty")), EDeliveryBotGridAreaType::Penalty, 3.0f, false),
		MakeGridCollisionRule(FName(TEXT("Blocked")), EDeliveryBotGridAreaType::Blocked, BIG_NUMBER, true)
	};
	
}

void ADeliveryBot_GridBoundsActor::BeginPlay()
{
	Super::BeginPlay();
	if (!bBuildGridOnBeginPlay)
		return;

	UWorld* world = GetWorld();
	if (!IsValid(world))
		return;

	UDeliveryBot_GridSubsystem* gridSubsystem = world->GetSubsystem<UDeliveryBot_GridSubsystem>();
	if (!IsValid(gridSubsystem))
		return;

	gridSubsystem->BuildGridFromBounds(this);
}

void ADeliveryBot_GridBoundsActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
}

void ADeliveryBot_GridBoundsActor::SetBuildGridOnBeginPlay(bool bEnabled)
{
	bBuildGridOnBeginPlay = bEnabled;
}
