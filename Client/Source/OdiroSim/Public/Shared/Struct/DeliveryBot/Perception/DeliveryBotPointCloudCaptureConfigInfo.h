#pragma once

#include "CoreMinimal.h"
#include "DeliveryBotPointCloudCaptureConfigInfo.generated.h"

// DeliveryBotSetup JSON에서 Python Point Cloud capture 동작을 제어하는 설정.
USTRUCT(BlueprintType)
struct FDeliveryBotPointCloudCaptureConfigInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasSetupPointCloudConfig{ false }; // setup JSON이 Point Cloud capture 설정을 명시했는지 여부

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ObservationProfile{ TEXT("point_cloud_capture") }; // Python lidarSpec.observationProfile 값

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCaptureEnabled{ true }; // Python Point Cloud 파일 저장 여부

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 CaptureEveryNSensorFrames{ 10 }; // Point Cloud를 저장할 sensor frame 간격

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RangeLimitM{ 0.f }; // 0보다 크면 Point Cloud 저장 거리 제한으로 사용

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIncludeGroundPoints{ true }; // tag 없는 ground point 저장 여부

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxPoints{ 4096 }; // frame당 최대 저장 point 수
};
