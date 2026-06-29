#include "UI/DisplayDpiScalingRule.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericWindow.h"
#include "Widgets/SWindow.h"

float UDisplayDpiScalingRule::GetDPIScaleBasedOnSize(const FIntPoint size) const
{
	(void)size;
	return ResolveGameWindowDpiScale();
}

float UDisplayDpiScalingRule::ResolveGameWindowDpiScale()
{
	TSharedPtr<SWindow> gameWindow;
	if (GEngine && GEngine->GameViewport)
	{
		gameWindow = GEngine->GameViewport->GetWindow();
	}

	if (!gameWindow.IsValid() && FSlateApplication::IsInitialized())
	{
		gameWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
	}

	TSharedPtr<FGenericWindow> nativeWindow;
	if (gameWindow.IsValid())
	{
		nativeWindow = gameWindow->GetNativeWindow();
	}
	if (!nativeWindow.IsValid())
	{
		return 1.0f;
	}

	return FMath::Max(nativeWindow->GetDPIScaleFactor(), 0.01f);
}
