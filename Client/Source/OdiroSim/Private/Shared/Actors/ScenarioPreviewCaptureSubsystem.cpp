#include "Shared/Actors/ScenarioPreviewCaptureSubsystem.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

// Preview Capture의 요청, 자원 생성, 인코딩과 파일 교체 결과를 기록한다.
DEFINE_LOG_CATEGORY_STATIC(LogScenarioPreviewCapture, Log, All);

// Capture Service가 안전하게 처리할 수 있는 요청인지 확인한다.
bool FScenarioPreviewCaptureRequest::IsValid() const
{
	// 해상도와 출력 경로의 공통 경계를 검증한다.
	constexpr int32 maximumOutputDimension = 8192;
	if (OutputSize.X <= 0
		|| OutputSize.Y <= 0
		|| OutputSize.X > maximumOutputDimension
		|| OutputSize.Y > maximumOutputDimension)
	{
		return false;
	}

	// 파일 Service가 처리할 수 있는 절대 PNG 경로인지 검증한다.
	FString normalizedOutputPath = OutputPath;
	normalizedOutputPath.TrimStartAndEndInline();
	FPaths::NormalizeFilename(normalizedOutputPath);

	const bool bPathValid = !normalizedOutputPath.IsEmpty()
		&& !FPaths::IsRelative(normalizedOutputPath)
		&& FPaths::GetExtension(normalizedOutputPath).Equals(
			TEXT("png"),
			ESearchCase::IgnoreCase);
	if (!bPathValid)
	{
		return false;
	}

	// 탑뷰 프레임과 카메라 높이가 SceneCapture에 사용할 수 있는지 검증한다.
	return Frame.IsValid()
		&& Frame.OrthoWidth <= static_cast<double>(TNumericLimits<float>::Max())
		&& FMath::IsFinite(CaptureHeightCm)
		&& CaptureHeightCm > 0.0
		&& FMath::IsFinite(Frame.CenterZ + CaptureHeightCm);
}

// 전달받은 프레이밍으로 월드를 한 번 촬영하고 PNG를 원자적으로 저장한다.
FScenarioPreviewCaptureResult UScenarioPreviewCaptureSubsystem::CapturePreview(
	const FScenarioPreviewCaptureRequest& request)
{
	// Game Thread와 외부 요청 경계를 검증한다.
	if (!IsInGameThread())
	{
		return MakeFailure(
			request,
			TEXT("request_validation"),
			TEXT("CapturePreview must run on the Game Thread."));
	}

	if (!request.IsValid())
	{
		return MakeFailure(
			request,
			TEXT("request_validation"),
			TEXT("Capture request contains an invalid camera, size, or PNG path."));
	}

	if (!IsValid(GetWorld()))
	{
		return MakeFailure(
			request,
			TEXT("world_validation"),
			TEXT("Capture world is unavailable."));
	}

	// 임시 RenderTarget을 생성한다.
	UTextureRenderTarget2D* renderTarget =
		CreateRenderTarget(request.OutputSize);
	if (!IsValid(renderTarget))
	{
		return MakeFailure(
			request,
			TEXT("render_target_creation"),
			TEXT("Failed to create the transient render target."));
	}

	// 모든 반환 경로에서 Capture Actor 연결과 GPU 자원을 정리한다.
	ASceneCapture2D* captureActor = nullptr;
	ON_SCOPE_EXIT
	{
		if (IsValid(captureActor))
		{
			if (USceneCaptureComponent2D* captureComponent =
				captureActor->GetCaptureComponent2D())
			{
				captureComponent->TextureTarget = nullptr;
			}

			captureActor->Destroy();
		}

		if (IsValid(renderTarget))
		{
			renderTarget->ReleaseResource();
		}
	};

	// 정중앙 상공 탑뷰 Capture Actor를 생성한다.
	captureActor = SpawnCaptureActor(request, renderTarget);
	if (!IsValid(captureActor))
	{
		return MakeFailure(
			request,
			TEXT("capture_actor_creation"),
			TEXT("Failed to create or configure the transient SceneCapture2D actor."));
	}

	USceneCaptureComponent2D* captureComponent =
		captureActor->GetCaptureComponent2D();
	if (!IsValid(captureComponent))
	{
		return MakeFailure(
			request,
			TEXT("capture_component_validation"),
			TEXT("SceneCapture2D does not have a valid capture component."));
	}

	// 현재 월드 상태를 RenderTarget에 한 번 촬영한다.
	captureComponent->CaptureScene();

	// RenderTarget을 PNG 데이터로 읽고 압축한다.
	TArray64<uint8> pngData;
	FString failureReason;
	if (!TryCompressRenderTarget(
		renderTarget,
		pngData,
		failureReason))
	{
		return MakeFailure(
			request,
			TEXT("render_target_readback"),
			failureReason);
	}

	// 완성된 PNG를 임시 파일을 거쳐 최종 경로로 교체한다.
	if (!TryWritePngAtomically(
		request.OutputPath,
		pngData,
		failureReason))
	{
		return MakeFailure(
			request,
			TEXT("file_commit"),
			failureReason);
	}

	// 촬영 조건과 최종 출력 경로를 성공 로그에 기록한다.
	UE_LOG(
		LogScenarioPreviewCapture,
		Log,
		TEXT("Preview capture succeeded. output_path=\"%s\" mode=\"top_view_orthographic\" source=\"final_color_ldr\" center=(%.2f, %.2f, %.2f) ortho_width=%.2f resolution=%dx%d"),
		*request.OutputPath,
		request.Frame.CenterXY.X,
		request.Frame.CenterXY.Y,
		request.Frame.CenterZ,
		request.Frame.OrthoWidth,
		request.OutputSize.X,
		request.OutputSize.Y);

	FScenarioPreviewCaptureResult result;
	result.bSuccess = true;
	return result;
}

// 요청한 출력 크기의 임시 8비트 sRGB RenderTarget을 생성한다.
UTextureRenderTarget2D* UScenarioPreviewCaptureSubsystem::CreateRenderTarget(
	const FIntPoint& outputSize)
{
	// Subsystem을 Outer로 사용하는 Transient RenderTarget을 생성한다.
	UTextureRenderTarget2D* renderTarget =
		NewObject<UTextureRenderTarget2D>(
			this,
			NAME_None,
			RF_Transient);
	if (!IsValid(renderTarget))
	{
		return nullptr;
	}

	// UI Preview에 사용할 8비트 sRGB 형식과 크기를 적용한다.
	renderTarget->RenderTargetFormat = RTF_RGBA8_SRGB;
	renderTarget->ClearColor = FLinearColor(0.08f, 0.10f, 0.12f, 1.0f);
	renderTarget->bAutoGenerateMips = false;
	renderTarget->InitAutoFormat(
		static_cast<uint32>(outputSize.X),
		static_cast<uint32>(outputSize.Y));
	renderTarget->UpdateResourceImmediate(true);

	return renderTarget;
}

// 요청 프레임의 정중앙 상공을 내려다보는 임시 Capture Actor를 생성하고 설정한다.
ASceneCapture2D* UScenarioPreviewCaptureSubsystem::SpawnCaptureActor(
	const FScenarioPreviewCaptureRequest& request,
	UTextureRenderTarget2D* renderTarget)
{
	// Actor 생성에 필요한 월드와 RenderTarget을 검증한다.
	UWorld* world = GetWorld();
	if (!IsValid(world) || !IsValid(renderTarget))
	{
		return nullptr;
	}

	// 프레임 중심의 상공에서 수직 아래를 바라보는 카메라 Transform을 만든다.
	const FVector captureLocation(
		request.Frame.CenterXY.X,
		request.Frame.CenterXY.Y,
		request.Frame.CenterZ + request.CaptureHeightCm);
	const FRotator captureRotation(-90.0, 0.0, 0.0);

	FActorSpawnParameters spawnParameters;
	spawnParameters.ObjectFlags |= RF_Transient;
	spawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ASceneCapture2D* captureActor =
		world->SpawnActor<ASceneCapture2D>(
			captureLocation,
			captureRotation,
			spawnParameters);
	if (!IsValid(captureActor))
	{
		return nullptr;
	}

	// 요청한 범위를 표시하는 Orthographic 탑뷰를 설정한다.
	USceneCaptureComponent2D* captureComponent =
		captureActor->GetCaptureComponent2D();
	if (!IsValid(captureComponent))
	{
		captureActor->Destroy();
		return nullptr;
	}

	captureComponent->ProjectionType =
		ECameraProjectionMode::Orthographic;
	captureComponent->OrthoWidth =
		static_cast<float>(request.Frame.OrthoWidth);
	captureComponent->bAutoCalculateOrthoPlanes = true;
	captureComponent->bUpdateOrthoPlanes = true;
	captureComponent->bUseCameraHeightAsViewTarget = true;

	captureComponent->PrimitiveRenderMode =
		ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;

	// 실제 화면과 같은 조명 및 후처리 결과를 저장한다.
	captureComponent->CaptureSource = SCS_FinalColorLDR;
	captureComponent->TextureTarget = renderTarget;
	captureComponent->bCaptureEveryFrame = false;
	captureComponent->bCaptureOnMovement = false;
	captureComponent->bAlwaysPersistRenderingState = false;
	captureComponent->bExcludeFromSceneTextureExtents = true;
	captureComponent->bUseRayTracingIfEnabled = false;

	// 호출자가 지정한 유효한 Actor를 촬영 결과에서 제외한다.
	captureComponent->HiddenActors.Reset();
	for (const TWeakObjectPtr<AActor>& hiddenActor : request.HiddenActors)
	{
		AActor* actor = hiddenActor.Get();
		if (IsValid(actor) && actor != captureActor)
		{
			captureComponent->HiddenActors.AddUnique(actor);
		}
	}

	return captureActor;
}

// RenderTarget 픽셀을 읽고 PNG 바이트 배열로 압축한다.
bool UScenarioPreviewCaptureSubsystem::TryCompressRenderTarget(
	UTextureRenderTarget2D* renderTarget,
	TArray64<uint8>& outPngData,
	FString& outFailureReason)
{
	// 실패 시 이전 호출의 데이터가 사용되지 않도록 초기화한다.
	outPngData.Reset();
	outFailureReason.Reset();

	// readback 대상 RenderTarget을 검증한다.
	if (!IsValid(renderTarget))
	{
		outFailureReason = TEXT("Render target is invalid.");
		return false;
	}

	// GPU RenderTarget을 CPU Image로 읽는다.
	FImage capturedImage;
	if (!FImageUtils::GetRenderTargetImage(
		renderTarget,
		capturedImage))
	{
		outFailureReason = TEXT("Failed to read pixels from the render target.");
		return false;
	}

	if (capturedImage.SizeX <= 0 || capturedImage.SizeY <= 0)
	{
		outFailureReason = TEXT("Render target readback produced an empty image.");
		return false;
	}

	// Image 데이터를 PNG 파일 형식의 바이트 배열로 압축한다.
	if (!FImageUtils::CompressImage(
		outPngData,
		TEXT("png"),
		capturedImage))
	{
		outFailureReason = TEXT("Failed to compress the captured image as PNG.");
		return false;
	}

	// PNG signature와 최소 데이터 크기를 확인한다.
	constexpr uint8 pngSignature[] =
	{
		0x89, 0x50, 0x4E, 0x47,
		0x0D, 0x0A, 0x1A, 0x0A
	};

	if (outPngData.Num() < UE_ARRAY_COUNT(pngSignature))
	{
		outFailureReason = TEXT("PNG compression produced insufficient data.");
		return false;
	}

	for (int32 signatureIndex = 0;
		signatureIndex < UE_ARRAY_COUNT(pngSignature);
		++signatureIndex)
	{
		if (outPngData[signatureIndex] != pngSignature[signatureIndex])
		{
			outPngData.Reset();
			outFailureReason = TEXT("PNG compression produced an invalid signature.");
			return false;
		}
	}

	return true;
}

// PNG를 임시 파일에 기록한 뒤 최종 경로로 교체한다.
bool UScenarioPreviewCaptureSubsystem::TryWritePngAtomically(
	const FString& outputPath,
	const TArray64<uint8>& pngData,
	FString& outFailureReason)
{
	// 실패 시 이전 호출의 설명이 사용되지 않도록 초기화한다.
	outFailureReason.Reset();

	// 저장할 PNG 데이터와 절대 출력 경로를 검증한다.
	if (pngData.IsEmpty())
	{
		outFailureReason = TEXT("PNG data is empty.");
		return false;
	}

	FString normalizedOutputPath = outputPath;
	normalizedOutputPath.TrimStartAndEndInline();
	FPaths::NormalizeFilename(normalizedOutputPath);
	FPaths::CollapseRelativeDirectories(normalizedOutputPath);

	if (normalizedOutputPath.IsEmpty()
		|| FPaths::IsRelative(normalizedOutputPath)
		|| !FPaths::GetExtension(normalizedOutputPath).Equals(
			TEXT("png"),
			ESearchCase::IgnoreCase))
	{
		outFailureReason = TEXT("Output path must be an absolute PNG path.");
		return false;
	}

	IFileManager& fileManager = IFileManager::Get();
	const FString outputDirectory =
		FPaths::GetPath(normalizedOutputPath);
	if (outputDirectory.IsEmpty()
		|| !fileManager.DirectoryExists(*outputDirectory))
	{
		outFailureReason = TEXT("Output directory does not exist.");
		return false;
	}

	// 이전 실패에서 남은 동일 요청의 임시 파일만 정리한다.
	const FString temporaryPath =
		normalizedOutputPath + TEXT(".tmp");
	fileManager.Delete(
		*temporaryPath,
		false,
		true,
		true);

	// 완성된 PNG 바이트를 임시 파일에 기록한다.
	if (!FFileHelper::SaveArrayToFile(
		pngData,
		*temporaryPath))
	{
		fileManager.Delete(*temporaryPath, false, true, true);
		outFailureReason = TEXT("Failed to write the temporary PNG file.");
		return false;
	}

	const int64 temporaryFileSize =
		fileManager.FileSize(*temporaryPath);
	if (temporaryFileSize != pngData.Num()
		|| temporaryFileSize <= 0)
	{
		fileManager.Delete(*temporaryPath, false, true, true);
		outFailureReason = TEXT("Temporary PNG file size does not match the encoded data.");
		return false;
	}

	// 기존 최종 파일을 교체하도록 임시 파일을 동일 디렉터리에서 이동한다.
	if (!fileManager.Move(
		*normalizedOutputPath,
		*temporaryPath,
		true,
		true,
		false,
		true))
	{
		fileManager.Delete(*temporaryPath, false, true, true);
		outFailureReason = TEXT("Failed to replace the final PNG file.");
		return false;
	}

	return true;
}

// 실패 단계와 원인을 로그에 남기고 표준 실패 결과를 구성한다.
FScenarioPreviewCaptureResult UScenarioPreviewCaptureSubsystem::MakeFailure(
	const FScenarioPreviewCaptureRequest& request,
	const FString& failureStage,
	const FString& failureReason)
{
	// 호출자가 결과를 처리하기 전에도 진단할 수 있도록 경고를 기록한다.
	UE_LOG(
		LogScenarioPreviewCapture,
		Warning,
		TEXT("Preview capture failed. output_path=\"%s\" mode=\"top_view_orthographic\" source=\"final_color_ldr\" stage=\"%s\" reason=\"%s\""),
		*request.OutputPath,
		*failureStage,
		*failureReason);

	FScenarioPreviewCaptureResult result;
	result.FailureStage = failureStage;
	result.FailureReason = failureReason;
	return result;
}
