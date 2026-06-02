
#include "Episode/Actors/EpisodePedestrianAnimInstance.h"
#include "Episode/Actors/EpisodePedestrian.h"

void UEpisodePedestrianAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	CachedPedestrian = Cast<AEpisodePedestrian>(TryGetPawnOwner());
	RootMotionMode = ERootMotionMode::NoRootMotionExtraction;
}

void UEpisodePedestrianAnimInstance::NativeUpdateAnimation(float deltaSeconds)
{
	Super::NativeUpdateAnimation(deltaSeconds);

	if (!CachedPedestrian)
	{
		CachedPedestrian = Cast<AEpisodePedestrian>(TryGetPawnOwner());
	}

	if (!CachedPedestrian)
	{
		VisualSpeedCmPerSecond = 0.0f;
		VisualDirectionDegrees = 0.0f;
		bMoving = false;
		return;
	}

	VisualSpeedCmPerSecond = CachedPedestrian->VisualSpeedCmPerSecond;
	VisualDirectionDegrees = CachedPedestrian->VisualDirectionDegrees;
	bMoving = CachedPedestrian->bMoving;
}
