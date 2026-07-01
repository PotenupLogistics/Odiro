#include "Platform/Widget/WindowsControlWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "Widgets/SWidget.h"
#include "Widgets/SWindow.h"

#if WITH_EDITOR
#include "Editor.h"
#include "LevelEditorViewport.h"
#endif

namespace
{
#if WITH_EDITOR
	constexpr float EditorImmersiveStateRefreshIntervalSeconds = 0.1f;
#endif

	const FName CaptionTypefaceName(TEXT("Regular"));
	const TCHAR MinimizeGlyph[] = TEXT("\uE921");
	const TCHAR MaximizeGlyph[] = TEXT("\uE922");
	const TCHAR RestoreGlyph[] = TEXT("\uE923");
	const TCHAR CloseGlyph[] = TEXT("\uE8BB");

	FString ResolveWindowsFontPath(const TCHAR* fontFileName)
	{
		const FString windowsDir = FPlatformMisc::GetEnvironmentVariable(TEXT("WINDIR"));
		if (!windowsDir.IsEmpty())
		{
			const FString fontPath = FPaths::Combine(windowsDir, TEXT("Fonts"), fontFileName);
			if (FPaths::FileExists(fontPath))
			{
				return fontPath;
			}
		}

		const FString fallbackPath = FPaths::Combine(TEXT("C:/Windows/Fonts"), fontFileName);
		return FPaths::FileExists(fallbackPath) ? fallbackPath : FString();
	}

	TSharedPtr<const FCompositeFont> ResolveCaptionCompositeFont()
	{
		static TSharedPtr<const FCompositeFont> CachedFont;
		if (CachedFont.IsValid())
		{
			return CachedFont;
		}

		FString fontPath = ResolveWindowsFontPath(TEXT("SegoeIcons.ttf"));
		if (fontPath.IsEmpty())
		{
			fontPath = ResolveWindowsFontPath(TEXT("segmdl2.ttf"));
		}
		if (fontPath.IsEmpty())
		{
			return nullptr;
		}

		CachedFont = MakeShared<FStandaloneCompositeFont>(
			CaptionTypefaceName,
			MoveTemp(fontPath),
			EFontHinting::Default,
			EFontLoadingPolicy::LazyLoad);
		return CachedFont;
	}

	const FText& ResolveGlyphText(const EPlatformWindowCaptionGlyph glyph)
	{
		static const FText MinimizeGlyphText = FText::FromString(MinimizeGlyph);
		static const FText MaximizeGlyphText = FText::FromString(MaximizeGlyph);
		static const FText RestoreGlyphText = FText::FromString(RestoreGlyph);
		static const FText CloseGlyphText = FText::FromString(CloseGlyph);

		switch (glyph)
		{
		case EPlatformWindowCaptionGlyph::Minimize:
			return MinimizeGlyphText;
		case EPlatformWindowCaptionGlyph::Maximize:
			return MaximizeGlyphText;
		case EPlatformWindowCaptionGlyph::Restore:
			return RestoreGlyphText;
		case EPlatformWindowCaptionGlyph::Close:
			return CloseGlyphText;
		default:
			return CloseGlyphText;
		}
	}
}

bool UWindowsControlWidget::ApplyWindowsCaptionGlyph(UTextBlock* textBlock, const EPlatformWindowCaptionGlyph glyph)
{
	if (!textBlock)
	{
		return false;
	}

	const TSharedPtr<const FCompositeFont> captionFont = ResolveCaptionCompositeFont();
	if (!captionFont.IsValid())
	{
		return false;
	}

	FSlateFontInfo fontInfo = textBlock->GetFont();
	fontInfo.FontObject = nullptr;
	fontInfo.CompositeFont = captionFont;
	fontInfo.TypefaceFontName = CaptionTypefaceName;

	textBlock->SetFont(fontInfo);
	textBlock->SetText(ResolveGlyphText(glyph));
	return true;
}

void UWindowsControlWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyCaptionGlyphs();
}

void UWindowsControlWidget::NativeConstruct()
{
	Super::NativeConstruct();
	bIsEditorImmersiveMaximizeActive = false;
#if WITH_EDITOR
	if (ShouldToggleEditorImmersiveForWindowCommand())
	{
		bIsEditorImmersiveMaximizeActive = IsEditorImmersiveViewportActive();
		StartEditorImmersiveStateRefreshTimer();
	}
#endif
	BindControls();
	RefreshWindowControlButtons();
}

void UWindowsControlWidget::NativeDestruct()
{
#if WITH_EDITOR
	StopEditorImmersiveStateRefreshTimer();
#endif
	UnbindControls();
	Super::NativeDestruct();
}

void UWindowsControlWidget::RefreshWindowControlButtons()
{
	const bool bIsMaximized = IsWindowMaximized();

	if (!bHasCachedMaximizedState || bLastKnownMaximized != bIsMaximized)
	{
		bHasCachedMaximizedState = true;
		bLastKnownMaximized = bIsMaximized;
		BP_OnMaximizeRestoreStateChanged(bIsMaximized);
	}

	ApplyConfiguredWindowsCaptionGlyph(MinimizeText.Get(), EPlatformWindowCaptionGlyph::Minimize);
	ApplyConfiguredWindowsCaptionGlyph(CloseText.Get(), EPlatformWindowCaptionGlyph::Close);

	ApplyConfiguredWindowsCaptionGlyph(
		MaximizeRestoreText.Get(),
		bIsMaximized ? EPlatformWindowCaptionGlyph::Restore : EPlatformWindowCaptionGlyph::Maximize);
}

void UWindowsControlWidget::MinimizeWindow()
{
	const TSharedPtr<SWindow> window = ResolveGameWindow();
	if (window.IsValid())
	{
		window->Minimize();
	}
}

void UWindowsControlWidget::ToggleMaximizeRestoreWindow()
{
#if WITH_EDITOR
	if (ShouldToggleEditorImmersiveForWindowCommand())
	{
		const bool bWasEditorImmersiveActive = IsEditorImmersiveViewportActive();
		if (ToggleEditorImmersiveViewport())
		{
			bIsEditorImmersiveMaximizeActive = !bWasEditorImmersiveActive;
			RefreshWindowControlButtons();
			return;
		}
	}
#endif

	const TSharedPtr<SWindow> window = ResolveGameWindow();
	if (!window.IsValid())
	{
		return;
	}

	if (window->IsWindowMaximized())
	{
		window->Restore();
	}
	else
	{
		window->Maximize();
	}

	RefreshWindowControlButtons();
}

void UWindowsControlWidget::CloseWindow()
{
#if WITH_EDITOR
	if (ShouldEndPlayForClose())
	{
		if (GEditor)
		{
			GEditor->RequestEndPlayMap();
		}
		return;
	}
#endif

	const TSharedPtr<SWindow> window = ResolveGameWindow();
	if (window.IsValid())
	{
		window->RequestDestroyWindow();
	}
}

bool UWindowsControlWidget::IsWindowMaximized() const
{
#if WITH_EDITOR
	if (ShouldToggleEditorImmersiveForWindowCommand())
	{
		return IsEditorImmersiveViewportActive();
	}
#endif

	const TSharedPtr<SWindow> window = ResolveGameWindow();
	return window.IsValid() && window->IsWindowMaximized();
}

void UWindowsControlWidget::BindControls()
{
	UButton* minimizeButton = MinimizeButton.Get();
	UButton* maximizeRestoreButton = MaximizeRestoreButton.Get();
	UButton* closeButton = CloseButton.Get();

	if (minimizeButton)
	{
		minimizeButton->OnClicked.RemoveDynamic(this, &UWindowsControlWidget::HandleMinimizeClicked);
		minimizeButton->OnClicked.AddDynamic(this, &UWindowsControlWidget::HandleMinimizeClicked);
	}
	if (maximizeRestoreButton)
	{
		maximizeRestoreButton->OnClicked.RemoveDynamic(this, &UWindowsControlWidget::HandleMaximizeRestoreClicked);
		maximizeRestoreButton->OnClicked.AddDynamic(this, &UWindowsControlWidget::HandleMaximizeRestoreClicked);
	}
	if (closeButton)
	{
		closeButton->OnClicked.RemoveDynamic(this, &UWindowsControlWidget::HandleCloseClicked);
		closeButton->OnClicked.AddDynamic(this, &UWindowsControlWidget::HandleCloseClicked);
	}
}

void UWindowsControlWidget::UnbindControls()
{
	UButton* minimizeButton = MinimizeButton.Get();
	UButton* maximizeRestoreButton = MaximizeRestoreButton.Get();
	UButton* closeButton = CloseButton.Get();

	if (minimizeButton)
	{
		minimizeButton->OnClicked.RemoveDynamic(this, &UWindowsControlWidget::HandleMinimizeClicked);
	}
	if (maximizeRestoreButton)
	{
		maximizeRestoreButton->OnClicked.RemoveDynamic(this, &UWindowsControlWidget::HandleMaximizeRestoreClicked);
	}
	if (closeButton)
	{
		closeButton->OnClicked.RemoveDynamic(this, &UWindowsControlWidget::HandleCloseClicked);
	}
}

bool UWindowsControlWidget::ApplyConfiguredWindowsCaptionGlyph(
	UTextBlock* textBlock,
	const EPlatformWindowCaptionGlyph glyph) const
{
	if (!textBlock)
	{
		return false;
	}

	const TSharedPtr<const FCompositeFont> captionFont = ResolveCaptionCompositeFont();
	if (!captionFont.IsValid())
	{
		return false;
	}

	FSlateFontInfo fontInfo = textBlock->GetFont();
	fontInfo.FontObject = nullptr;
	fontInfo.CompositeFont = captionFont;
	fontInfo.TypefaceFontName = CaptionTypefaceName;
	if (CaptionGlyphFontSize > 0.0f)
	{
		fontInfo.Size = CaptionGlyphFontSize;
	}

	textBlock->SetFont(fontInfo);
	textBlock->SetText(ResolveGlyphText(glyph));
	return true;
}

void UWindowsControlWidget::ApplyCaptionGlyphs()
{
	ApplyConfiguredWindowsCaptionGlyph(MinimizeText.Get(), EPlatformWindowCaptionGlyph::Minimize);
	ApplyConfiguredWindowsCaptionGlyph(CloseText.Get(), EPlatformWindowCaptionGlyph::Close);

	ApplyConfiguredWindowsCaptionGlyph(MaximizeRestoreText.Get(), EPlatformWindowCaptionGlyph::Maximize);
}

TSharedPtr<SWindow> UWindowsControlWidget::ResolveGameWindow() const
{
	if (FSlateApplication::IsInitialized())
	{
		const TSharedPtr<SWidget> cachedWidget = GetCachedWidget();
		if (cachedWidget.IsValid())
		{
			const TSharedPtr<SWindow> widgetWindow = FSlateApplication::Get().FindWidgetWindow(
				cachedWidget.ToSharedRef());
			if (widgetWindow.IsValid())
			{
				return widgetWindow;
			}
		}
	}

	if (!GEngine || !GEngine->GameViewport)
	{
		return nullptr;
	}

	return GEngine->GameViewport->GetWindow();
}

bool UWindowsControlWidget::ShouldEndPlayForClose() const
{
	const UWorld* world = GetWorld();
	return world && world->WorldType == EWorldType::PIE;
}

bool UWindowsControlWidget::ShouldToggleEditorImmersiveForWindowCommand() const
{
	const UWorld* world = GetWorld();
	return world && world->WorldType == EWorldType::PIE;
}

bool UWindowsControlWidget::ToggleEditorImmersiveViewport() const
{
#if WITH_EDITOR
	IConsoleObject* consoleObject = IConsoleManager::Get().FindConsoleObject(TEXT("LevelEditor.ToggleImmersive"));
	IConsoleCommand* consoleCommand = consoleObject ? consoleObject->AsCommand() : nullptr;
	if (!consoleCommand)
	{
		return false;
	}

	consoleCommand->Execute(TArray<FString>(), GetWorld(), *GLog);
	return true;
#else
	return false;
#endif
}

#if WITH_EDITOR
void UWindowsControlWidget::StartEditorImmersiveStateRefreshTimer()
{
	UWorld* world = GetWorld();
	if (!world || !ShouldToggleEditorImmersiveForWindowCommand())
	{
		return;
	}

	world->GetTimerManager().SetTimer(
		EditorImmersiveStateRefreshTimerHandle,
		this,
		&UWindowsControlWidget::HandleEditorImmersiveStateRefresh,
		EditorImmersiveStateRefreshIntervalSeconds,
		true);
}

void UWindowsControlWidget::StopEditorImmersiveStateRefreshTimer()
{
	UWorld* world = GetWorld();
	if (!world)
	{
		return;
	}

	world->GetTimerManager().ClearTimer(EditorImmersiveStateRefreshTimerHandle);
}

void UWindowsControlWidget::HandleEditorImmersiveStateRefresh()
{
	if (!ShouldToggleEditorImmersiveForWindowCommand())
	{
		StopEditorImmersiveStateRefreshTimer();
		return;
	}

	const bool bIsEditorImmersiveActive = IsEditorImmersiveViewportActive();
	if (bIsEditorImmersiveMaximizeActive == bIsEditorImmersiveActive &&
		bHasCachedMaximizedState &&
		bLastKnownMaximized == bIsEditorImmersiveActive)
	{
		return;
	}

	bIsEditorImmersiveMaximizeActive = bIsEditorImmersiveActive;
	RefreshWindowControlButtons();
}

bool UWindowsControlWidget::IsEditorImmersiveViewportActive() const
{
	bool bFoundViewport = false;
	if (GCurrentLevelEditingViewportClient)
	{
		bFoundViewport = true;
		if (GCurrentLevelEditingViewportClient->IsInImmersiveViewport())
		{
			return true;
		}
	}

	if (GEditor)
	{
		for (const FEditorViewportClient* viewportClient : GEditor->GetAllViewportClients())
		{
			bFoundViewport = true;
			if (viewportClient && viewportClient->IsInImmersiveViewport())
			{
				return true;
			}
		}
	}

	return bFoundViewport ? false : bIsEditorImmersiveMaximizeActive;
}
#endif

void UWindowsControlWidget::HandleMinimizeClicked()
{
	MinimizeWindow();
}

void UWindowsControlWidget::HandleMaximizeRestoreClicked()
{
	ToggleMaximizeRestoreWindow();
}

void UWindowsControlWidget::HandleCloseClicked()
{
	CloseWindow();
}
