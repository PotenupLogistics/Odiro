#include "Scenario/Editor/ScenarioEditorController.h"

#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Scenario/Actors/ScenarioGroundRegion.h"
#include "Scenario/Editor/ScenarioAuthoringSubsystem.h"
#include "Scenario/Editor/ScenarioPlacementPreviewActor.h"
#include "Scenario/Editor/ScenarioTransformGizmoActor.h"
#include "Shared/Actors/ScenarioPreviewCaptureSubsystem.h"
#include "Shared/Actors/ScenarioPreviewFraming.h"

// Scenario 저장 후 Preview 생성 흐름의 단계와 결과를 기록한다.
DEFINE_LOG_CATEGORY_STATIC(LogScenarioEditorPreview, Log, All);

// 저장된 scenario.json과 현재 Editor Preview로 프로젝트 대표 PNG를 생성한다.
void AScenarioEditorController::CaptureScenarioPreviewAfterSave(
	const FString& resolvedJsonFilePath,
	UScenarioAuthoringSubsystem* authoringSubsystem)
{
	// 저장된 JSON 경로에서 사용자 프로젝트 root와 Preview 경로를 계산한다.
	FString normalizedJsonFilePath = resolvedJsonFilePath;
	normalizedJsonFilePath.TrimStartAndEndInline();
	FPaths::NormalizeFilename(normalizedJsonFilePath);
	FPaths::CollapseRelativeDirectories(normalizedJsonFilePath);

	if (normalizedJsonFilePath.IsEmpty()
		|| FPaths::IsRelative(normalizedJsonFilePath))
	{
		UE_LOG(
			LogScenarioEditorPreview,
			Warning,
			TEXT("Scenario preview failed. kind=scenario stage=path_resolution reason=\"Saved scenario path is not absolute.\""));
		return;
	}

	const FString projectRoot =
		FPaths::GetPath(normalizedJsonFilePath);
	const FString previewPath =
		FPaths::Combine(projectRoot, TEXT("preview.png"));

	// 실패한 촬영이 이전 Scenario 이미지로 오인되지 않도록 stale 파일 제거 동작을 준비한다.
	auto removeStalePreview = [&previewPath]()
	{
		IFileManager& fileManager = IFileManager::Get();
		if (!fileManager.FileExists(*previewPath))
		{
			return;
		}

		if (!fileManager.Delete(*previewPath, false, true, true))
		{
			UE_LOG(
				LogScenarioEditorPreview,
				Warning,
				TEXT("Scenario preview stale file could not be removed. path=\"%s\""),
				*previewPath);
		}
	};

	// 실패 단계와 이유를 기록하고 기존 Preview를 제거하는 공통 처리를 준비한다.
	auto handleCaptureFailure =
		[&previewPath, &removeStalePreview](
			const FString& failureStage,
			const FString& failureReason)
	{
		removeStalePreview();

		UE_LOG(
			LogScenarioEditorPreview,
			Warning,
			TEXT("Scenario preview failed. kind=scenario output_path=\"%s\" stage=\"%s\" reason=\"%s\""),
			*previewPath,
			*failureStage,
			*failureReason);
	};

	// 저장된 Draft와 현재 Editor Preview를 소유한 Subsystem을 검증한다.
	if (!IsValid(authoringSubsystem))
	{
		handleCaptureFailure(
			TEXT("authoring_validation"),
			TEXT("Scenario authoring subsystem is unavailable."));
		return;
	}

	// Saved preview framing follows the same authored scenario bounds used by editor viewport fit.
	FScenarioMapBounds mapBounds;
	if (!authoringSubsystem->TryResolveScenarioEditorViewportMapBounds(mapBounds))
	{
		handleCaptureFailure(
			TEXT("map_bounds"),
			TEXT("Current editor viewport does not provide valid map bounds."));
		return;
	}

	// The preview image keeps its own output aspect while sharing the editor fit margin.
	FScenarioPreviewFramingSettings framingSettings;
	framingSettings.ScenarioPreviewFitScale = FMath::Max(EditorViewportFitScale, 1.0);
	FScenarioPreviewFrame frame;
	if (!FScenarioPreviewFramingResolver::TryResolveScenario(
		mapBounds,
		framingSettings,
		frame))
	{
		handleCaptureFailure(
			TEXT("framing"),
			TEXT("Failed to resolve the scenario preview frame."));
		return;
	}

	// 현재 월드의 단발성 Capture Service를 조회한다.
	UWorld* world = GetWorld();
	UScenarioPreviewCaptureSubsystem* captureSubsystem =
		IsValid(world)
			? world->GetSubsystem<UScenarioPreviewCaptureSubsystem>()
			: nullptr;
	if (!IsValid(captureSubsystem))
	{
		handleCaptureFailure(
			TEXT("capture_service"),
			TEXT("Scenario preview capture subsystem is unavailable."));
		return;
	}

	// 프레임, 출력 경로와 Authoring 전용 제외 Actor를 요청에 구성한다.
	FScenarioPreviewCaptureRequest request;
	request.Frame = frame;
	request.OutputPath = previewPath;
	request.OutputSize = FIntPoint(
		framingSettings.OutputWidth,
		framingSettings.OutputHeight);

	TArray<AActor*> authoringHiddenActors;
	authoringSubsystem->GetScenarioPreviewHiddenActors(authoringHiddenActors);
	for (AActor* hiddenActor : authoringHiddenActors)
	{
		if (IsValid(hiddenActor))
		{
			request.HiddenActors.AddUnique(
				TWeakObjectPtr<AActor>(hiddenActor));
		}
	}

	// Controller가 소유하는 임시 편집 Actor도 촬영에서 제외한다.
	const auto addControllerHiddenActor =
		[&request](AActor* hiddenActor)
	{
		if (IsValid(hiddenActor))
		{
			request.HiddenActors.AddUnique(
				TWeakObjectPtr<AActor>(hiddenActor));
		}
	};

	addControllerHiddenActor(PlacementPreviewActor.Get());
	addControllerHiddenActor(TransformGizmoActor.Get());
	addControllerHiddenActor(RegionDrawPreviewActor.Get());

	// 현재 월드 상태를 한 번 촬영하고 최종 PNG로 교체한다.
	const FScenarioPreviewCaptureResult captureResult =
		captureSubsystem->CapturePreview(request);
	if (!captureResult.bSuccess)
	{
		handleCaptureFailure(
			captureResult.FailureStage,
			captureResult.FailureReason);
		return;
	}

	// 시나리오 의미를 포함한 성공 로그를 기록한다.
	UE_LOG(
		LogScenarioEditorPreview,
		Log,
		TEXT("Scenario preview succeeded. kind=scenario output_path=\"%s\" center=(%.2f, %.2f, %.2f) ortho_width=%.2f resolution=%dx%d"),
		*previewPath,
		frame.CenterXY.X,
		frame.CenterXY.Y,
		frame.CenterZ,
		frame.OrthoWidth,
		framingSettings.OutputWidth,
		framingSettings.OutputHeight);
}
