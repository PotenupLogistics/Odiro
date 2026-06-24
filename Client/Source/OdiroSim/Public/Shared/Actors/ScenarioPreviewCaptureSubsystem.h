#pragma once

#include "CoreMinimal.h"
#include "Shared/Actors/ScenarioPreviewFraming.h"
#include "Subsystems/WorldSubsystem.h"
#include "ScenarioPreviewCaptureSubsystem.generated.h"

class AActor;
class ASceneCapture2D;
class UTextureRenderTarget2D;

// 단발성 Preview PNG 촬영에 필요한 카메라, 출력 크기와 경로를 전달한다.
struct ODIROSIM_API FScenarioPreviewCaptureRequest
{
	// TopViewOrthographic 모드에서 사용할 맵 중심 기준 카메라 프레임이다.
	FScenarioPreviewFrame Frame;

	// 호출자가 허용된 사용자 프로젝트 영역 안에서 검증한 절대 PNG 경로다.
	FString OutputPath;

	// RenderTarget과 최종 PNG가 사용할 픽셀 크기다.
	FIntPoint OutputSize = FIntPoint(512, 384);

	// TopViewOrthographic 모드에서 맵 중심 높이 위로 띄울 임시 카메라 높이다.
	double CaptureHeightCm = 100000.0;

	// 편집 Handle이나 Preview Actor처럼 결과 이미지에서 제외할 외부 소유 Actor 목록이다.
	TArray<TWeakObjectPtr<AActor>> HiddenActors;

	// Capture Service가 안전하게 처리할 수 있는 요청인지 확인한다.
	bool IsValid() const;
};

// 단발성 Capture의 성공 여부 또는 실패 단계와 원인을 반환한다.
struct ODIROSIM_API FScenarioPreviewCaptureResult
{
	// PNG가 최종 출력 경로에 완전히 교체되었는지 나타낸다.
	bool bSuccess = false;

	// 실패한 처리 경계를 식별하며 성공 시 비어 있다.
	FString FailureStage;

	// 호출자가 경고 로그에 사용할 수 있는 실패 설명이며 성공 시 비어 있다.
	FString FailureReason;
};

// 월드별 단발성 SceneCapture2D와 RenderTarget 수명주기를 관리하고 PNG를 저장한다.
UCLASS()
class ODIROSIM_API UScenarioPreviewCaptureSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// 전달받은 프레이밍으로 월드를 한 번 촬영하고 PNG를 원자적으로 저장한다.
	FScenarioPreviewCaptureResult CapturePreview(
		const FScenarioPreviewCaptureRequest& request);

private:
	// 요청한 출력 크기의 임시 8비트 sRGB RenderTarget을 생성한다.
	UTextureRenderTarget2D* CreateRenderTarget(
		const FIntPoint& outputSize);

	// 요청 프레임의 정중앙 상공을 내려다보는 임시 Capture Actor를 생성하고 설정한다.
	ASceneCapture2D* SpawnCaptureActor(
		const FScenarioPreviewCaptureRequest& request,
		UTextureRenderTarget2D* renderTarget);

	// RenderTarget 픽셀을 읽고 PNG 바이트 배열로 압축한다.
	static bool TryCompressRenderTarget(
		UTextureRenderTarget2D* renderTarget,
		TArray64<uint8>& outPngData,
		FString& outFailureReason);

	// PNG를 임시 파일에 기록한 뒤 최종 경로로 교체한다.
	static bool TryWritePngAtomically(
		const FString& outputPath,
		const TArray64<uint8>& pngData,
		FString& outFailureReason);

	// 실패 단계와 원인을 로그에 남기고 표준 실패 결과를 구성한다.
	static FScenarioPreviewCaptureResult MakeFailure(
		const FScenarioPreviewCaptureRequest& request,
		const FString& failureStage,
		const FString& failureReason);
};
