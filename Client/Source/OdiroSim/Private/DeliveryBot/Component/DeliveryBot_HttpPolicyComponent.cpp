#include "DeliveryBot/Component/DeliveryBot_HttpPolicyComponent.h"
#include "DrawDebugHelpers.h"
#include "DeliveryBot/Actor/DeliveryBot.h"
#include "DeliveryBot/DeliveryBotLidarRayPattern.h"
#include "DeliveryBot/Subsystem/DeliveryBotPythonProcessSubsystem.h"
#include "DeliveryBot/Subsystem/DeliveryBot_GridSubsystem.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "Scenario/ScenarioEvaluationSubsystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Interfaces/IHttpResponse.h"
#include "Shared/SimulationSetupTypes.h"
#include "Shared/Struct/DeliveryBot/Result/DeliveryBotPolicyEventSnapshot.h"
#include "Shared/UserProjectDataTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeliveryBotHttpPolicy, Log, All);

namespace
{
	// Point Cloud 저장 간격이 잘못 들어왔을 때 사용할 기본 sensor frame 간격.
	constexpr int32 DefaultCaptureEveryNSensorFrames = 10;

	// Point Cloud frame당 최대 point 수가 잘못 들어왔을 때 사용할 기본값.
	constexpr int32 DefaultMaxPoints = 4096;

	// JSON object field를 안전하게 가져온다.
	bool TryGetJsonObjectField(const FJsonObject& jsonObject, const FString& fieldName, TSharedPtr<FJsonObject>& outObject)
	{
		const TSharedPtr<FJsonValue> jsonValue = jsonObject.TryGetField(fieldName);
		if (!jsonValue.IsValid() || jsonValue->Type != EJson::Object)
			return false;

		outObject = jsonValue->AsObject();
		return outObject.IsValid();
	}

	// JSON array field를 안전하게 가져온다.
	bool TryGetJsonArrayField(const FJsonObject& jsonObject, const FString& fieldName, TArray<TSharedPtr<FJsonValue>>& outArray)
	{
		const TSharedPtr<FJsonValue> jsonValue = jsonObject.TryGetField(fieldName);
		if (!jsonValue.IsValid() || jsonValue->Type != EJson::Array)
			return false;

		outArray = jsonValue->AsArray();
		return true;
	}

	// FName 태그 배열을 JSON string 배열로 변환한다.
	TArray<TSharedPtr<FJsonValue>> MakeJsonStringArrayFromNames(const TArray<FName>& names)
	{
		TArray<TSharedPtr<FJsonValue>> jsonValues;
		jsonValues.Reserve(names.Num());

		for (const FName& name : names)
		{
			jsonValues.Add(MakeShared<FJsonValueString>(name.ToString()));
		}

		return jsonValues;
	}

	TSharedRef<FJsonObject> MakeJsonVectorObject(const FVector& vector)
	{
		TSharedRef<FJsonObject> jsonObject = MakeShared<FJsonObject>();
		jsonObject->SetNumberField(TEXT("x"), vector.X);
		jsonObject->SetNumberField(TEXT("y"), vector.Y);
		jsonObject->SetNumberField(TEXT("z"), vector.Z);
		return jsonObject;
	}
	
	// HTTP 응답 본문은 평가 이벤트 payload에 짧은 진단 문자열로만 보관한다.
	FString MakeResponseBodySnippet(const FHttpResponsePtr& response, int32 maxLength = 500)
	{
		if (!response.IsValid())
			return FString();

		FString body = response->GetContentAsString();
		body.ReplaceInline(TEXT("\r"), TEXT(" "));
		body.ReplaceInline(TEXT("\n"), TEXT(" "));

		if (body.Len() > maxLength)
		{
			body = body.Left(maxLength) + TEXT("...");
		}

		return body;
	}

	// Python JSON point object를 Unreal world cm 좌표로 변환한다.
	bool TryGetJsonVectorCm(const TSharedPtr<FJsonObject>& object, FVector& outVector)
	{
		outVector = FVector::ZeroVector;

		if (!object.IsValid())
			return false;

		double x = 0.0;
		double y = 0.0;
		double z = 0.0;

		if (!object->TryGetNumberField(TEXT("x"), x) || !object->TryGetNumberField(TEXT("y"), y))
			return false;

		object->TryGetNumberField(TEXT("z"), z);
		outVector = FVector(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
		return true;
	}

	// 단순 path follow가 아니라 실제 RePath 판단으로 기록할 reason만 허용한다.
	bool IsActualRepathReason(const FString& reason)
	{
		return reason.Equals(TEXT("dynamic_repath_ready"), ESearchCase::IgnoreCase)
			|| reason.Equals(TEXT("collision_repath_ready"), ESearchCase::IgnoreCase);
	}

	// 우선순위가 높은 값이 비어 있을 때만 fallback string field를 복사한다.
	void TryCopyStringField(const TSharedPtr<FJsonObject>& sourceObject, const FString& fieldName, FString& outValue)
	{
		if (sourceObject.IsValid() && outValue.IsEmpty())
		{
			sourceObject->TryGetStringField(fieldName, outValue);
		}
	}

	// 우선순위가 높은 값이 없을 때만 fallback number field를 복사한다.
	void TryCopyNumberField(const TSharedPtr<FJsonObject>& sourceObject, const FString& fieldName, double& outValue, bool& bOutHasValue)
	{
		if (sourceObject.IsValid() && !bOutHasValue)
		{
			bOutHasValue = sourceObject->TryGetNumberField(fieldName, outValue);
		}
	}

	// LiDAR mode enum을 Python debug용 문자열로 변환한다.
	FString ToJsonLidarModeString(EDeliveryBotLidarModeType lidarModeType)
	{
		switch (lidarModeType)
		{
		case EDeliveryBotLidarModeType::OneD:
			return TEXT("OneD");
		case EDeliveryBotLidarModeType::TwoD:
			return TEXT("TwoD");
		case EDeliveryBotLidarModeType::ThreeD:
			return TEXT("ThreeD");
		case EDeliveryBotLidarModeType::OneDAndTwoD:
			return TEXT("OneDAndTwoD");
		case EDeliveryBotLidarModeType::TwoDAndThreeD:
			return TEXT("TwoDAndThreeD");
		case EDeliveryBotLidarModeType::All:
			return TEXT("All");
		case EDeliveryBotLidarModeType::OusterOS1:
			return TEXT("OusterOS1");
		default:
			return TEXT("TwoD");
		}
	}

	void SetJsonTargetFields(const TSharedPtr<FJsonObject>& targetObject, const FString& targetId, const TArray<FName>& targetTags)
	{
		if (!targetObject.IsValid())
		{
			return;
		}

		targetObject->SetStringField(TEXT("targetId"), targetId);
		targetObject->SetArrayField(TEXT("targetTags"), MakeJsonStringArrayFromNames(targetTags));
	}

	// Python LiDAR family 선택 규칙과 맞춰 action log에 기록할 policy 입력을 계산한다.
	FString ResolvePolicyRaySelectionMode(
		EDeliveryBotLidarModeType lidarModeType,
		int32 ray1DCount,
		int32 ray2DCount,
		int32 ray3DCount,
		int32 legacyRayCount)
	{
		switch (lidarModeType)
		{
		case EDeliveryBotLidarModeType::OneD:
			return TEXT("1d");
		case EDeliveryBotLidarModeType::TwoD:
		case EDeliveryBotLidarModeType::OneDAndTwoD:
		case EDeliveryBotLidarModeType::TwoDAndThreeD:
			return TEXT("2d");
		case EDeliveryBotLidarModeType::ThreeD:
		case EDeliveryBotLidarModeType::OusterOS1:
			return TEXT("3d");
		case EDeliveryBotLidarModeType::All:
			if (ray2DCount > 0)
			{
				return TEXT("2d");
			}
			if (ray3DCount > 0)
			{
				return TEXT("3d");
			}
			if (ray1DCount > 0)
			{
				return TEXT("1d");
			}
			return legacyRayCount > 0 ? TEXT("legacy2d") : TEXT("2d");
		default:
			break;
		}

		if (ray2DCount > 0)
		{
			return TEXT("2d");
		}
		if (ray3DCount > 0)
		{
			return TEXT("3d");
		}
		if (ray1DCount > 0)
		{
			return TEXT("1d");
		}
		if (legacyRayCount > 0)
		{
			return TEXT("legacy2d");
		}
		return TEXT("none");
	}

	// actions.jsonl policy_ray_selection 계약에 기록할 source path를 반환한다.
	FString ResolvePolicyRaySelectionSource(const FString& mode)
	{
		if (mode == TEXT("1d"))
		{
			return TEXT("lidar.rays_1d");
		}
		if (mode == TEXT("2d"))
		{
			return TEXT("lidar.rays_2d");
		}
		if (mode == TEXT("3d"))
		{
			return TEXT("lidar.rays_3d.nearest_vertical_by_yaw");
		}
		if (mode == TEXT("legacy2d"))
		{
			return TEXT("legacy.lidarRays");
		}
		return TEXT("none");
	}

	// Python 3D projection이 2D policy에 노출할 yaw bucket 수를 센다.
	int32 CountProjected3DPolicyRays(const TArray<FDeliveryBotLidarRayInfo>& rayInfos)
	{
		TSet<int32> yawKeys;
		for (const FDeliveryBotLidarRayInfo& rayInfo : rayInfos)
		{
			if (rayInfo.RayDimensionType != EDeliveryBotLidarRayDimensionType::ThreeD)
			{
				continue;
			}

			yawKeys.Add(FMath::RoundToInt(rayInfo.RayYawDegree * 100.f));
		}
		return yawKeys.Num();
	}

	// Python이 horizontal 3D projection 기준으로 사용하는 pitch 값을 찾는다.
	bool TryCalculateHorizontalPitchDegree(const TArray<FDeliveryBotLidarRayInfo>& rayInfos, float& outPitchDegree)
	{
		outPitchDegree = 0.f;
		bool bFoundRay = false;
		float bestAbsPitchDegree = TNumericLimits<float>::Max();

		for (const FDeliveryBotLidarRayInfo& rayInfo : rayInfos)
		{
			if (rayInfo.RayDimensionType != EDeliveryBotLidarRayDimensionType::ThreeD)
			{
				continue;
			}

			const float absPitchDegree = FMath::Abs(rayInfo.RayPitchDegree);
			if (!bFoundRay || absPitchDegree < bestAbsPitchDegree)
			{
				bFoundRay = true;
				bestAbsPitchDegree = absPitchDegree;
			}
		}

		if (bFoundRay)
		{
			outPitchDegree = bestAbsPitchDegree;
		}
		return bFoundRay;
	}

	// 3D ray hit 거리를 수평 정책 ray 기준 거리로 변환한다.
	float CalculateProjectedHorizontalDistanceM(const FDeliveryBotLidarRayInfo& rayInfo)
	{
		return rayInfo.DistanceM * FMath::Max(0.0f, FMath::Cos(FMath::DegreesToRadians(rayInfo.RayPitchDegree)));
	}

	// 같은 yaw/column에서 Python 3D projection과 같은 우선순위로 대표 ray를 고른다.
	bool IsBetterProjected3DRay(
		const FDeliveryBotLidarRayInfo& candidateRayInfo,
		const FDeliveryBotLidarRayInfo& currentRayInfo,
		float horizontalPitchAbsDegree)
	{
		if (candidateRayInfo.bHit != currentRayInfo.bHit)
		{
			return candidateRayInfo.bHit;
		}

		if (candidateRayInfo.bHit && candidateRayInfo.bBlocksPolicy != currentRayInfo.bBlocksPolicy)
		{
			return candidateRayInfo.bBlocksPolicy;
		}

		if (candidateRayInfo.bHit)
		{
			const float candidateDistanceM = CalculateProjectedHorizontalDistanceM(candidateRayInfo);
			const float currentDistanceM = CalculateProjectedHorizontalDistanceM(currentRayInfo);
			if (!FMath::IsNearlyEqual(candidateDistanceM, currentDistanceM))
			{
				return candidateDistanceM < currentDistanceM;
			}

			return FMath::Abs(candidateRayInfo.RayPitchDegree) < FMath::Abs(currentRayInfo.RayPitchDegree);
		}

		const float candidatePitchGap = FMath::Abs(FMath::Abs(candidateRayInfo.RayPitchDegree) - horizontalPitchAbsDegree);
		const float currentPitchGap = FMath::Abs(FMath::Abs(currentRayInfo.RayPitchDegree) - horizontalPitchAbsDegree);
		if (!FMath::IsNearlyEqual(candidatePitchGap, currentPitchGap))
		{
			return candidatePitchGap < currentPitchGap;
		}

		return candidateRayInfo.DistanceM < currentRayInfo.DistanceM;
	}

	// 특정 LiDAR dimension의 raw ray 개수를 센다.
	int32 CountLidarRaysForDimension(
		const TArray<FDeliveryBotLidarRayInfo>& rayInfos,
		EDeliveryBotLidarRayDimensionType dimensionType)
	{
		int32 rayCount = 0;
		for (const FDeliveryBotLidarRayInfo& rayInfo : rayInfos)
		{
			if (rayInfo.RayDimensionType == dimensionType)
			{
				++rayCount;
			}
		}
		return rayCount;
	}

	// Python 정책 payload에 넣을 3D ray를 full 또는 yaw/column별 대표 ray로 선택한다.
	void Build3DRayPayloadInfos(
		const TArray<FDeliveryBotLidarRayInfo>& sourceRayInfos,
		bool bSendFull3DRays,
		TArray<const FDeliveryBotLidarRayInfo*>& outRayInfos)
	{
		outRayInfos.Reset();

		if (bSendFull3DRays)
		{
			for (const FDeliveryBotLidarRayInfo& rayInfo : sourceRayInfos)
			{
				if (rayInfo.RayDimensionType == EDeliveryBotLidarRayDimensionType::ThreeD)
				{
					outRayInfos.Add(&rayInfo);
				}
			}
			return;
		}

		float horizontalPitchAbsDegree = 0.f;
		if (!TryCalculateHorizontalPitchDegree(sourceRayInfos, horizontalPitchAbsDegree))
		{
			return;
		}

		TMap<int32, const FDeliveryBotLidarRayInfo*> selectedRayByColumn;
		for (const FDeliveryBotLidarRayInfo& rayInfo : sourceRayInfos)
		{
			if (rayInfo.RayDimensionType != EDeliveryBotLidarRayDimensionType::ThreeD)
			{
				continue;
			}

			const int32 columnKey = rayInfo.ColumnIndex != INDEX_NONE
				? rayInfo.ColumnIndex
				: FMath::RoundToInt(rayInfo.RayYawDegree * 100.f);
			const FDeliveryBotLidarRayInfo* const* currentRayInfo = selectedRayByColumn.Find(columnKey);
			if (currentRayInfo == nullptr ||
				*currentRayInfo == nullptr ||
				IsBetterProjected3DRay(rayInfo, **currentRayInfo, horizontalPitchAbsDegree))
			{
				selectedRayByColumn.Add(columnKey, &rayInfo);
			}
		}

		selectedRayByColumn.GenerateValueArray(outRayInfos);
		outRayInfos.Sort(
			[](const FDeliveryBotLidarRayInfo& leftRayInfo, const FDeliveryBotLidarRayInfo& rightRayInfo)
			{
				if (!FMath::IsNearlyEqual(leftRayInfo.RayYawDegree, rightRayInfo.RayYawDegree))
				{
					return leftRayInfo.RayYawDegree < rightRayInfo.RayYawDegree;
				}
				return leftRayInfo.ColumnIndex < rightRayInfo.ColumnIndex;
			});
	}

	// action log writer가 소비할 selection summary를 만든다.
	TSharedRef<FJsonObject> MakeJsonPolicyRaySelectionObject(
		EDeliveryBotLidarModeType lidarModeType,
		const TArray<FDeliveryBotLidarRayInfo>& rayInfos,
		int32 ray1DCount,
		int32 ray2DCount,
		int32 ray3DCount,
		int32 legacyRayCount)
	{
		const FString mode = ResolvePolicyRaySelectionMode(
			lidarModeType,
			ray1DCount,
			ray2DCount,
			ray3DCount,
			legacyRayCount);

		int32 rayCount = 0;
		if (mode == TEXT("1d"))
		{
			rayCount = ray1DCount;
		}
		else if (mode == TEXT("2d"))
		{
			rayCount = ray2DCount;
		}
		else if (mode == TEXT("3d"))
		{
			rayCount = CountProjected3DPolicyRays(rayInfos);
		}
		else if (mode == TEXT("legacy2d"))
		{
			rayCount = legacyRayCount;
		}

		TSharedRef<FJsonObject> selectionObject = MakeShared<FJsonObject>();
		selectionObject->SetStringField(TEXT("mode"), mode);
		selectionObject->SetStringField(TEXT("source"), ResolvePolicyRaySelectionSource(mode));
		selectionObject->SetNumberField(TEXT("rayCount"), rayCount);

		float horizontalPitchDegree = 0.f;
		if (mode == TEXT("3d") && TryCalculateHorizontalPitchDegree(rayInfos, horizontalPitchDegree))
		{
			selectionObject->SetNumberField(TEXT("horizontalPitchDegree"), horizontalPitchDegree);
		}
		else
		{
			selectionObject->SetField(TEXT("horizontalPitchDegree"), MakeShared<FJsonValueNull>());
		}
		return selectionObject;
	}

	// 기존 Python 정책 호환용 2D LiDAR ray JSON을 만든다.
	TSharedRef<FJsonObject> MakeJsonLegacyLidarRayObject(const FDeliveryBotLidarRayInfo& rayInfo)
	{
		TSharedRef<FJsonObject> rayObject = MakeShared<FJsonObject>();
		rayObject->SetBoolField(TEXT("hit"), rayInfo.bHit);
		rayObject->SetNumberField(TEXT("distanceM"), rayInfo.DistanceM);
		rayObject->SetNumberField(TEXT("rayIndex"), rayInfo.RayIndex);
		rayObject->SetNumberField(TEXT("rayYawDegree"), rayInfo.RayYawDegree);
		rayObject->SetStringField(TEXT("actorName"), rayInfo.ActorName);
		rayObject->SetArrayField(TEXT("actorTags"), MakeJsonStringArrayFromNames(rayInfo.ActorTags));
		SetJsonTargetFields(rayObject, rayInfo.TargetId, rayInfo.TargetTags);
		rayObject->SetBoolField(TEXT("blocksPolicy"), rayInfo.bBlocksPolicy);
		return rayObject;
	}

	// 1D LiDAR ray JSON을 만든다.
	TSharedRef<FJsonObject> MakeJsonLidarRay1DObject(const FDeliveryBotLidarRayInfo& rayInfo)
	{
		TSharedRef<FJsonObject> rayObject = MakeShared<FJsonObject>();
		rayObject->SetBoolField(TEXT("hit"), rayInfo.bHit);
		rayObject->SetNumberField(TEXT("distanceM"), rayInfo.DistanceM);
		rayObject->SetNumberField(TEXT("rayIndex"), rayInfo.RayIndex);
		rayObject->SetStringField(TEXT("actorName"), rayInfo.ActorName);
		rayObject->SetArrayField(TEXT("actorTags"), MakeJsonStringArrayFromNames(rayInfo.ActorTags));
		SetJsonTargetFields(rayObject, rayInfo.TargetId, rayInfo.TargetTags);
		rayObject->SetBoolField(TEXT("blocksPolicy"), rayInfo.bBlocksPolicy);
		return rayObject;
	}

	// 2D LiDAR ray JSON을 만든다.
	TSharedRef<FJsonObject> MakeJsonLidarRay2DObject(const FDeliveryBotLidarRayInfo& rayInfo)
	{
		TSharedRef<FJsonObject> rayObject = MakeJsonLidarRay1DObject(rayInfo);
		rayObject->SetNumberField(TEXT("yawDegree"), rayInfo.RayYawDegree);
		return rayObject;
	}

	// 3D LiDAR ray JSON을 만든다.
	TSharedRef<FJsonObject> MakeJsonLidarRay3DObject(const FDeliveryBotLidarRayInfo& rayInfo)
	{
		TSharedRef<FJsonObject> rayObject = MakeJsonLidarRay2DObject(rayInfo);
		rayObject->SetNumberField(TEXT("pitchDegree"), rayInfo.RayPitchDegree);
		rayObject->SetObjectField(TEXT("hitLocationCm"), MakeJsonVectorObject(rayInfo.HitLocationCm));
		if (rayInfo.ChannelIndex != INDEX_NONE)
		{
			rayObject->SetNumberField(TEXT("channelIndex"), rayInfo.ChannelIndex);
		}
		if (rayInfo.ColumnIndex != INDEX_NONE)
		{
			rayObject->SetNumberField(TEXT("columnIndex"), rayInfo.ColumnIndex);
		}
		if (rayInfo.ChannelIndex != INDEX_NONE || rayInfo.ColumnIndex != INDEX_NONE || !rayInfo.SensorModel.IsNone())
		{
			rayObject->SetNumberField(TEXT("relativeTimeSeconds"), rayInfo.RelativeTimeSeconds);
		}
		if (!rayInfo.SensorModel.IsNone())
		{
			rayObject->SetStringField(TEXT("sensorModel"), rayInfo.SensorModel.ToString());
		}
		return rayObject;
	}
}

// 컴포넌트 Tick은 끄고 DeliveryBot Tick에서 명시적으로 갱신한다.
UDeliveryBot_HttpPolicyComponent::UDeliveryBot_HttpPolicyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// BeginPlay에서 호출되어 scenario start를 예약한다.
void UDeliveryBot_HttpPolicyComponent::RequestStartScenario()
{
	if (bScenarioStarted || bStartRequestInFlight)
		return;

	ResetScenarioState();

	bStartRequested = true;
	UE_LOG(
		LogDeliveryBotHttpPolicy,
		Log,
		TEXT("Python scenario start requested. Owner=%s"),
		*GetNameSafe(GetOwner()));
	TryStartScenario();
}

// Runner가 지정한 project output episode id를 이후 decide 기록에 사용한다.
void UDeliveryBot_HttpPolicyComponent::ConfigureProjectActionLogging(const FString& projectOutputEpisodeId)
{
	ProjectActionEpisodeId = projectOutputEpisodeId.TrimStartAndEnd();
}

// Runner가 확정한 project episode 경로를 actions와 Python artifact의 공통 출력 기준으로 저장한다.
bool UDeliveryBot_HttpPolicyComponent::ConfigureProjectEpisodeOutput(
	const FString& projectOutputEpisodeId,
	const FString& projectEpisodeOutputDirectory,
	const FString& projectEpisodeOutputRelativeDirectory)
{
	ConfigureProjectActionLogging(projectOutputEpisodeId);
	bProjectEpisodeOutputRequired = true;
	ProjectEpisodeOutputDirectory.Reset();
	ProjectEpisodeOutputRelativeDirectory.Reset();
	ProjectEpisodeOutputErrorCode.Reset();
	ProjectEpisodeOutputErrorMessage.Reset();

	if (!FUserProjectEpisodeScenarioJson::IsValidEpisodeId(ProjectActionEpisodeId))
	{
		ProjectEpisodeOutputErrorCode = TEXT("INVALID_EPISODE_OUTPUT_ID");
		ProjectEpisodeOutputErrorMessage = FString::Printf(
			TEXT("Invalid project episode output id: %s"),
			*ProjectActionEpisodeId);
		UE_LOG(
			LogDeliveryBotHttpPolicy,
			Warning,
			TEXT("Project episode output configuration deferred to /scenario/start failure: %s"),
			*ProjectEpisodeOutputErrorMessage);
		return false;
	}

	FString normalizedDirectory = projectEpisodeOutputDirectory.TrimStartAndEnd();
	if (normalizedDirectory.IsEmpty() || FPaths::IsRelative(normalizedDirectory))
	{
		ProjectEpisodeOutputErrorCode = TEXT("INVALID_EPISODE_OUTPUT_PATH");
		ProjectEpisodeOutputErrorMessage = FString::Printf(
			TEXT("Absolute project episode output directory required: %s"),
			*normalizedDirectory);
		UE_LOG(
			LogDeliveryBotHttpPolicy,
			Warning,
			TEXT("Project episode output configuration deferred to /scenario/start failure: %s"),
			*ProjectEpisodeOutputErrorMessage);
		return false;
	}
	FPaths::NormalizeDirectoryName(normalizedDirectory);

	FString normalizedRelativeDirectory = projectEpisodeOutputRelativeDirectory.TrimStartAndEnd();
	FPaths::NormalizeFilename(normalizedRelativeDirectory);
	if (normalizedRelativeDirectory.IsEmpty()
		|| !FPaths::IsRelative(normalizedRelativeDirectory)
		|| normalizedRelativeDirectory.Equals(TEXT(".."))
		|| normalizedRelativeDirectory.StartsWith(TEXT("../")))
	{
		ProjectEpisodeOutputErrorCode = TEXT("INVALID_EPISODE_OUTPUT_RELATIVE_PATH");
		ProjectEpisodeOutputErrorMessage = FString::Printf(
			TEXT("Safe run-relative project episode directory required: %s"),
			*normalizedRelativeDirectory);
		UE_LOG(
			LogDeliveryBotHttpPolicy,
			Warning,
			TEXT("Project episode output configuration deferred to /scenario/start failure: %s"),
			*ProjectEpisodeOutputErrorMessage);
		return false;
	}

	ProjectEpisodeOutputDirectory = MoveTemp(normalizedDirectory);
	ProjectEpisodeOutputRelativeDirectory = MoveTemp(normalizedRelativeDirectory);
	UE_LOG(
		LogDeliveryBotHttpPolicy,
		Log,
		TEXT("Project episode output configured: episode=%s root=%s relative=%s"),
		*ProjectActionEpisodeId,
		*ProjectEpisodeOutputDirectory,
		*ProjectEpisodeOutputRelativeDirectory);
	return true;
}

// start 전에는 재시도하고 start 후에는 decide를 반복 호출한다.
void UDeliveryBot_HttpPolicyComponent::UpdatePolicy(float deltaTime)
{
	if (bEndRequestInFlight)
		return;

	if (bStartRequested && !bScenarioStarted)
	{
		StartRetryElapsedSeconds += deltaTime;
		if (StartRetryElapsedSeconds >= StartRetryIntervalSeconds)
		{
			StartRetryElapsedSeconds = 0.f;
			TryStartScenario();
		}
		return;
	}

	if (!bScenarioStarted)
		return;

	DecideElapsedSeconds += deltaTime;
	if (DecideElapsedSeconds >= DecideIntervalSeconds)
	{
		const float decisionDeltaTime = DecideElapsedSeconds;
		DecideElapsedSeconds = 0.f;
		RequestDecision(decisionDeltaTime);
	}
}

// Python /scenario/end를 제한 시간 안에 요청하고 결과를 정확히 한 번 반환한다.
void UDeliveryBot_HttpPolicyComponent::EndScenario(
	const FString& status,
	TFunction<void(bool, const FString&)> onComplete)
{
	if (!bScenarioStarted || bEndRequestInFlight)
	{
		if (onComplete)
		{
			onComplete(
				false,
				TEXT("Scenario is not started or end request is already in flight."));
		}
		return;
	}

	FString payload;
	if (!BuildEndPayload(status, payload))
	{
		if (onComplete)
		{
			onComplete(false, TEXT("Failed to build /scenario/end payload."));
		}
		return;
	}

	bEndRequestInFlight = true;
	TWeakObjectPtr<UDeliveryBot_HttpPolicyComponent> weakThis(this);
	const TSharedRef<bool, ESPMode::ThreadSafe> bCompletionReported =
		MakeShared<bool, ESPMode::ThreadSafe>(false);

	const auto completeOnce =
		[bCompletionReported, onComplete](
			bool bSucceeded,
			const FString& errorMessage)
		{
			if (*bCompletionReported)
			{
				return;
			}

			*bCompletionReported = true;
			if (onComplete)
			{
				onComplete(bSucceeded, errorMessage);
			}
		};

	const bool bRequestStarted = SendPostRequest(
		TEXT("/scenario/end"),
		payload,
		[weakThis, completeOnce](
			FHttpResponsePtr response,
			bool bHttpSucceeded) mutable
		{
			UDeliveryBot_HttpPolicyComponent* component = weakThis.Get();
			if (!IsValid(component))
			{
				return;
			}

			component->bEndRequestInFlight = false;

			TSharedPtr<FJsonObject> responseObject;
			bool bAccepted = false;
			const bool bEndSucceeded =
				bHttpSucceeded
				&& component->IsPythonResponseOk(response)
				&& component->TryGetPythonResponseObject(response, responseObject)
				&& responseObject.IsValid()
				&& responseObject->TryGetBoolField(TEXT("accepted"), bAccepted)
				&& bAccepted;

			component->ResetScenarioState();

			completeOnce(
				bEndSucceeded,
				bEndSucceeded
					? FString()
					: TEXT("Python /scenario/end timed out, failed, or was rejected."));
		},
		EndRequestTimeoutSeconds);

	if (!bRequestStarted)
	{
		bEndRequestInFlight = false;

		completeOnce(
			false,
			TEXT("Python /scenario/end HTTP request could not be started."));
	}
}

// Python 서버에 /scenario/start 요청을 보낸다.
bool UDeliveryBot_HttpPolicyComponent::TryStartScenario()
{
	if (bScenarioStarted || bStartRequestInFlight)
		return true;

	FString payload;
	if (!BuildStartPayload(payload))
	{
		UE_LOG(
			LogDeliveryBotHttpPolicy,
			Warning,
			TEXT("Python scenario start payload build failed. Owner=%s"),
			*GetNameSafe(GetOwner()));
		return false;
	}

	bStartRequestInFlight = true;

	const bool bRequestStarted = SendPostRequest(
		TEXT("/scenario/start"),
		payload,
		[this](FHttpResponsePtr response, bool bSucceeded)
		{
			bStartRequestInFlight = false;

			// /scenario/start envelope 응답의 response.status를 확인한다.
			if (!bSucceeded || !IsPythonResponseOk(response))
			{
				const int32 responseCode = response.IsValid() ? response->GetResponseCode() : 0;
				const FString responseBody = response.IsValid() ? response->GetContentAsString() : FString();

				UE_LOG(
					LogDeliveryBotHttpPolicy,
					Warning,
					TEXT("Python scenario start failed. Succeeded=%s, Code=%d, Body=%s"),
					bSucceeded ? TEXT("true") : TEXT("false"),
					responseCode,
					*responseBody);

				TSharedPtr<FJsonObject> responseObject;
				if (bSucceeded && TryGetPythonResponseObject(response, responseObject) && responseObject.IsValid())
				{
					FString errorCode = TEXT("PYTHON_START_REJECTED");
					FString errorMessage = TEXT("Python scenario start was rejected.");
					bool bRetryable = false;
					TSharedPtr<FJsonObject> errorObject;
					if (TryGetJsonObjectField(*responseObject, TEXT("error"), errorObject))
					{
						errorObject->TryGetStringField(TEXT("code"), errorCode);
						errorObject->TryGetStringField(TEXT("message"), errorMessage);
						errorObject->TryGetBoolField(TEXT("retryable"), bRetryable);
					}

					bStartRequested = bRetryable;
					EmitPolicyFailureEvent(
						TEXT("/scenario/start"),
						responseObject,
						errorCode,
						errorMessage,
						bRetryable,
						!bRetryable);
				}

				return;
			}

			bScenarioStarted = true;
			bLoggedStartWaitingForPython = false;
			UE_LOG(LogDeliveryBotHttpPolicy, Log, TEXT("Python scenario started."));
		});

	if (!bRequestStarted)
	{
		bStartRequestInFlight = false;
		if (!bLoggedStartWaitingForPython)
		{
			const UDeliveryBotPythonProcessSubsystem* pythonProcessSubsystem = GetPythonProcessSubsystem();
			const FString processStatus = IsValid(pythonProcessSubsystem)
				? pythonProcessSubsystem->GetDebugStatus()
				: TEXT("PythonProcessSubsystem=<invalid>");

			UE_LOG(
				LogDeliveryBotHttpPolicy,
				Warning,
				TEXT("Python scenario start is waiting for policy server. %s"),
				*processStatus);

			bLoggedStartWaitingForPython = true;
		}
	}

	return bRequestStarted;
}


// Python 서버에 /scenario/decide 요청을 보내고 action을 차량에 적용한다.
bool UDeliveryBot_HttpPolicyComponent::RequestDecision(float deltaTime)
{
	if (!bScenarioStarted || bDecisionRequestInFlight)
		return false;

	FString payload;
	if (!BuildDecidePayload(payload))
		return false;

	bDecisionRequestInFlight = true;
	const TSharedPtr<FJsonObject> requestObjectForLog = LastDecisionRequestObject;
	const FString projectEpisodeIdForLog = ResolveProjectEpisodeId();

	const bool bRequestStarted = SendPostRequest(
		TEXT("/scenario/decide"),
		payload,
		[this, deltaTime, requestObjectForLog, projectEpisodeIdForLog](FHttpResponsePtr response, bool bSucceeded)
		{
			bDecisionRequestInFlight = false;

			FDeliveryBotMoveCommandInfo moveCommand;
			if (!bSucceeded)
			{
				StorePolicyDecisionError(TEXT("PYTHON_REQUEST_FAILED"), TEXT("Python decide HTTP request failed."));
				EmitPolicyServerFailureEvent(
					TEXT("/scenario/decide"),
					response,
					TEXT("PYTHON_REQUEST_FAILED"),
					TEXT("Python decide HTTP request failed."),
					true,
					true);
				WriteProjectActionRecord(projectEpisodeIdForLog, requestObjectForLog, response, false);

				if (ADeliveryBot* deliveryBot = Cast<ADeliveryBot>(GetOwner()))
				{
					deliveryBot->ApplyParkingStop();
				}
				return;
			}

			if (!TryParseMoveCommand(response, moveCommand))
			{
				WriteProjectActionRecord(projectEpisodeIdForLog, requestObjectForLog, response, false);

				if (ADeliveryBot* deliveryBot = Cast<ADeliveryBot>(GetOwner()))
				{
					deliveryBot->ApplyParkingStop();
				}
				return;
			}
				
			WriteProjectActionRecord(projectEpisodeIdForLog, requestObjectForLog, response, true);

			if (ADeliveryBot* deliveryBot = Cast<ADeliveryBot>(GetOwner()))
			{
				deliveryBot->ApplyMoveCommand(moveCommand, deltaTime);
			}
		});

	if (!bRequestStarted)
	{
		bDecisionRequestInFlight = false;
		StorePolicyDecisionError(TEXT("PYTHON_REQUEST_NOT_STARTED"), TEXT("Python decide HTTP request could not be started."));
		EmitPolicyServerFailureEvent(
			TEXT("/scenario/decide"),
			FHttpResponsePtr(),
			TEXT("PYTHON_REQUEST_NOT_STARTED"),
			TEXT("Python decide HTTP request could not be started."),
			true,
			true);
		WriteProjectActionRecord(projectEpisodeIdForLog, requestObjectForLog, nullptr, false);
	}

	return bRequestStarted;
}



// 현재 GameInstance에서 Python process subsystem을 가져온다.
UDeliveryBotPythonProcessSubsystem* UDeliveryBot_HttpPolicyComponent::GetPythonProcessSubsystem() const
{
	AActor* owner = GetOwner();
	if (!IsValid(owner))
		return nullptr;

	UGameInstance* gameInstance = owner->GetGameInstance();
	if (!IsValid(gameInstance))
		return nullptr;

	return gameInstance->GetSubsystem<UDeliveryBotPythonProcessSubsystem>();
}

// Python 서버에 POST 요청을 보낸다.
bool UDeliveryBot_HttpPolicyComponent::SendPostRequest(
	const FString& endpoint,
	const FString& payload,
	TFunction<void(FHttpResponsePtr, bool)> onComplete,
	float timeoutSeconds)
{
	const UDeliveryBotPythonProcessSubsystem* pythonProcessSubsystem = GetPythonProcessSubsystem();
	if (!IsValid(pythonProcessSubsystem) || !pythonProcessSubsystem->IsReady())
		return false;

	const FString normalizedEndpoint = endpoint.StartsWith(TEXT("/")) ? endpoint : TEXT("/") + endpoint;
	const FString url = pythonProcessSubsystem->GetBaseUrl() + normalizedEndpoint;

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> request = FHttpModule::Get().CreateRequest();
	request->SetURL(url);
	request->SetVerb(TEXT("POST"));
	request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	request->SetContentAsString(payload);
	if (timeoutSeconds > 0.f)
	{
		request->SetTimeout(timeoutSeconds);
	}

	request->OnProcessRequestComplete().BindWeakLambda(
		this,
		[onComplete = MoveTemp(onComplete)](
			FHttpRequestPtr,
			FHttpResponsePtr response,
			bool bWasSuccessful) mutable
		{
			if (onComplete)
			{
				onComplete(response, bWasSuccessful);
			}
		});

	return request->ProcessRequest();
}

// Project run이면 runner가 지정한 output episode id를 우선 사용한다.
FString UDeliveryBot_HttpPolicyComponent::ResolveProjectEpisodeId() const
{
	const FSimulationCommandLineParseResult commandLineResult = FSimulationCommandLine::ParseCurrent();
	if (!commandLineResult.bSuccess || !commandLineResult.Options.bProjectRun)
	{
		return FString();
	}

	if (FUserProjectEpisodeScenarioJson::IsValidEpisodeId(ProjectActionEpisodeId))
	{
		return ProjectActionEpisodeId;
	}

	UWorld* world = GetWorld();
	if (!IsValid(world))
	{
		return FString();
	}

	const UScenarioEvaluationSubsystem* evaluationSubsystem = world->GetSubsystem<UScenarioEvaluationSubsystem>();
	if (!IsValid(evaluationSubsystem))
	{
		return FString();
	}

	return evaluationSubsystem->GetCurrentResult().EpisodeId;
}

// Python decide request/response를 project actions.jsonl에 한 줄로 추가한다.
void UDeliveryBot_HttpPolicyComponent::WriteProjectActionRecord(
	const FString& projectEpisodeId,
	const TSharedPtr<FJsonObject>& requestObject,
	const FHttpResponsePtr& response,
	bool bActionSucceeded) const
{
	if (!requestObject.IsValid())
	{
		return;
	}

	const FSimulationCommandLineParseResult commandLineResult = FSimulationCommandLine::ParseCurrent();
	if (!commandLineResult.bSuccess || !commandLineResult.Options.bProjectRun)
	{
		return;
	}

	FString episodeId = projectEpisodeId.TrimStartAndEnd();
	if (!FUserProjectEpisodeScenarioJson::IsValidEpisodeId(episodeId))
	{
		episodeId = ResolveProjectEpisodeId();
	}

	if (!FUserProjectEpisodeScenarioJson::IsValidEpisodeId(episodeId))
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Project action record skipped: invalid episode id '%s'."), *episodeId);
		return;
	}

	TSharedPtr<FJsonObject> responseObject;
	TryGetPythonResponseObject(response, responseObject);

	const int32 httpStatusCode = response.IsValid() ? response->GetResponseCode() : 0;
	const FString errorMessage = bActionSucceeded
		? FString()
		: FString::Printf(
			TEXT("Python policy decide failed. ResponseValid=%s, HttpStatus=%d"),
			response.IsValid() ? TEXT("true") : TEXT("false"),
			httpStatusCode);

	TArray<FString> diagnostics;
	if (!FUserProjectRunOutputJson::AppendRobotActionRecord(
			FUserProjectRunSnapshot::BuildPaths(
				commandLineResult.Options.ProjectPath,
				commandLineResult.Options.RunId),
			episodeId,
			requestObject.ToSharedRef(),
			responseObject,
			bActionSucceeded,
			httpStatusCode,
			errorMessage,
			diagnostics))
	{
		for (const FString& diagnostic : diagnostics)
		{
			UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Project action record write failed: %s"), *diagnostic);
		}
	}
}

// request 객체를 Python message envelope으로 감싼다.
bool UDeliveryBot_HttpPolicyComponent::BuildMessagePayload(const FString& messageType, const TSharedRef<FJsonObject>& requestObject, FString& outPayload) const
{
	outPayload.Reset();

	TSharedRef<FJsonObject> rootObject = MakeShared<FJsonObject>();
	rootObject->SetStringField(TEXT("schema"), TEXT("delivery_bot_python_message"));
	rootObject->SetNumberField(TEXT("version"), 1);
	rootObject->SetStringField(TEXT("type"), messageType);
	rootObject->SetObjectField(TEXT("request"), requestObject);

	const TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&outPayload);
	return FJsonSerializer::Serialize(rootObject, writer);
}

// FVector를 Python 서버 location JSON 객체로 변환한다.
TSharedRef<FJsonObject> UDeliveryBot_HttpPolicyComponent::BuildLocationObject(const FVector& location, float yawDegree) const
{
	TSharedRef<FJsonObject> locationObject = MakeShared<FJsonObject>();

	locationObject->SetNumberField(TEXT("x"), location.X);
	locationObject->SetNumberField(TEXT("y"), location.Y);
	locationObject->SetNumberField(TEXT("z"), location.Z);
	locationObject->SetNumberField(TEXT("yawDegree"), yawDegree);

	return locationObject;
}

// GridSubsystem JSON에서 Python 서버가 받는 필드만 추려 grid 객체를 만든다.
bool UDeliveryBot_HttpPolicyComponent::BuildPythonGridObject(TSharedPtr<FJsonObject>& outGridObject) const
{
	outGridObject.Reset();

	const UWorld* world = GetWorld();
	if (!IsValid(world))
		return false;

	const UDeliveryBot_GridSubsystem* gridSubsystem = world->GetSubsystem<UDeliveryBot_GridSubsystem>();
	if (!IsValid(gridSubsystem) || !gridSubsystem->HasBuiltGrid())
		return false;

	FString gridJson;
	if (!gridSubsystem->BuildGridJson(gridJson))
		return false;

	TSharedPtr<FJsonObject> sourceGridObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(gridJson);
	if (!FJsonSerializer::Deserialize(reader, sourceGridObject) || !sourceGridObject.IsValid())
		return false;

	outGridObject = MakeShared<FJsonObject>();

	double numberValue = 0.0;

	sourceGridObject->TryGetNumberField(TEXT("gridSizeX"), numberValue);
	outGridObject->SetNumberField(TEXT("gridSizeX"), numberValue);

	sourceGridObject->TryGetNumberField(TEXT("gridSizeY"), numberValue);
	outGridObject->SetNumberField(TEXT("gridSizeY"), numberValue);

	sourceGridObject->TryGetNumberField(TEXT("cellSizeCm"), numberValue);
	outGridObject->SetNumberField(TEXT("cellSizeCm"), numberValue);

	sourceGridObject->TryGetNumberField(TEXT("cellCount"), numberValue);
	outGridObject->SetNumberField(TEXT("cellCount"), numberValue);

	TSharedPtr<FJsonObject> originObject;
	if (!TryGetJsonObjectField(*sourceGridObject, TEXT("originCm"), originObject))
		return false;

	outGridObject->SetObjectField(TEXT("originCm"), originObject);

	TArray<TSharedPtr<FJsonValue>> sourceCellValues;
	if (!TryGetJsonArrayField(*sourceGridObject, TEXT("cells"), sourceCellValues))
		return false;

	TArray<TSharedPtr<FJsonValue>> targetCellValues;
	targetCellValues.Reserve(sourceCellValues.Num());

	for (const TSharedPtr<FJsonValue>& sourceCellValue : sourceCellValues)
	{
		const TSharedPtr<FJsonObject> sourceCellObject = sourceCellValue.IsValid() ? sourceCellValue->AsObject() : nullptr;
		if (!sourceCellObject.IsValid())
			continue;

		TSharedRef<FJsonObject> targetCellObject = MakeShared<FJsonObject>();

		sourceCellObject->TryGetNumberField(TEXT("x"), numberValue);
		targetCellObject->SetNumberField(TEXT("x"), numberValue);

		sourceCellObject->TryGetNumberField(TEXT("y"), numberValue);
		targetCellObject->SetNumberField(TEXT("y"), numberValue);

		FString areaType;
		sourceCellObject->TryGetStringField(TEXT("areaType"), areaType);
		targetCellObject->SetStringField(TEXT("areaType"), areaType);

		sourceCellObject->TryGetNumberField(TEXT("cost"), numberValue);
		targetCellObject->SetNumberField(TEXT("cost"), numberValue);

		bool bBlocked = false;
		sourceCellObject->TryGetBoolField(TEXT("blocked"), bBlocked);
		targetCellObject->SetBoolField(TEXT("blocked"), bBlocked);

		FString sourceCollisionProfile;
		sourceCellObject->TryGetStringField(TEXT("sourceCollisionProfile"), sourceCollisionProfile);
		targetCellObject->SetStringField(TEXT("sourceCollisionProfile"), sourceCollisionProfile);

		targetCellValues.Add(MakeShared<FJsonValueObject>(targetCellObject));
	}

	outGridObject->SetArrayField(TEXT("cells"), targetCellValues);
	return true;
}

// setup JSON 우선, 없으면 컴포넌트 기본값으로 Point Cloud 설정을 만든다
FDeliveryBotPointCloudCaptureConfigInfo UDeliveryBot_HttpPolicyComponent::BuildEffectivePointCloudCaptureConfigInfo(const FDeliveryBotPointCloudCaptureConfigInfo& setupPointCloudConfigInfo) const
{
	if (setupPointCloudConfigInfo.bHasSetupPointCloudConfig)
		return SanitizePointCloudCaptureConfigInfo(setupPointCloudConfigInfo);

	FDeliveryBotPointCloudCaptureConfigInfo effectiveConfigInfo;
	effectiveConfigInfo.ObservationProfile = PythonObservationProfile;
	effectiveConfigInfo.bCaptureEnabled = bEnablePythonPointCloudCapture;
	effectiveConfigInfo.CaptureEveryNSensorFrames = PythonPointCloudCaptureEveryNSensorFrames;
	effectiveConfigInfo.MaxPoints = PythonPointCloudMaxPoints;
	effectiveConfigInfo.bIncludeGroundPoints = bPythonPointCloudIncludeGroundPoints;
	effectiveConfigInfo.RangeLimitM = PythonPointCloudRangeLimitM;

	return SanitizePointCloudCaptureConfigInfo(effectiveConfigInfo);
}

// Python으로 보내기 전 Point Cloud 설정값을 안전한 값으로 보정한다
FDeliveryBotPointCloudCaptureConfigInfo UDeliveryBot_HttpPolicyComponent::SanitizePointCloudCaptureConfigInfo(const FDeliveryBotPointCloudCaptureConfigInfo& pointCloudConfigInfo) const
{
	FDeliveryBotPointCloudCaptureConfigInfo sanitizedConfigInfo = pointCloudConfigInfo;
	sanitizedConfigInfo.ObservationProfile = sanitizedConfigInfo.ObservationProfile.TrimStartAndEnd();

	if (sanitizedConfigInfo.ObservationProfile.IsEmpty())
	{
		sanitizedConfigInfo.ObservationProfile = sanitizedConfigInfo.bCaptureEnabled ? TEXT("point_cloud_capture") : TEXT("basic");

		UE_LOG(
			LogDeliveryBotHttpPolicy,
			Warning,
			TEXT("Point Cloud observation profile was empty. Using profile=%s."),
			*sanitizedConfigInfo.ObservationProfile);
	}

	if (sanitizedConfigInfo.CaptureEveryNSensorFrames <= 0)
	{
		UE_LOG(
			LogDeliveryBotHttpPolicy,
			Warning,
			TEXT("Invalid Point Cloud captureEveryNSensorFrames=%d. Using default=%d."),
			sanitizedConfigInfo.CaptureEveryNSensorFrames,
			DefaultCaptureEveryNSensorFrames);

		sanitizedConfigInfo.CaptureEveryNSensorFrames = DefaultCaptureEveryNSensorFrames;
	}

	if (sanitizedConfigInfo.MaxPoints <= 0)
	{
		UE_LOG(
			LogDeliveryBotHttpPolicy,
			Warning,
			TEXT("Invalid Point Cloud maxPoints=%d. Using default=%d."),
			sanitizedConfigInfo.MaxPoints,
			DefaultMaxPoints);

		sanitizedConfigInfo.MaxPoints = DefaultMaxPoints;
	}

	if (sanitizedConfigInfo.RangeLimitM < 0.f)
	{
		UE_LOG(
			LogDeliveryBotHttpPolicy,
			Warning,
			TEXT("Invalid Point Cloud rangeLimitM=%f. Using unlimited range."),
			sanitizedConfigInfo.RangeLimitM);

		sanitizedConfigInfo.RangeLimitM = 0.f;
	}

	return sanitizedConfigInfo;
}

// Python point cloud capture 옵션 JSON을 만든다
TSharedRef<FJsonObject> UDeliveryBot_HttpPolicyComponent::BuildPointCloudOptionsObject(const FDeliveryBotPointCloudCaptureConfigInfo& pointCloudConfigInfo) const
{
	const FDeliveryBotPointCloudCaptureConfigInfo sanitizedConfigInfo = SanitizePointCloudCaptureConfigInfo(pointCloudConfigInfo);

	TSharedRef<FJsonObject> optionsObject = MakeShared<FJsonObject>();
	optionsObject->SetBoolField(TEXT("captureEnabled"), sanitizedConfigInfo.bCaptureEnabled);
	optionsObject->SetNumberField(TEXT("captureEveryNSensorFrames"), sanitizedConfigInfo.CaptureEveryNSensorFrames);
	optionsObject->SetNumberField(TEXT("maxPoints"), sanitizedConfigInfo.MaxPoints);
	optionsObject->SetBoolField(TEXT("includeGroundPoints"), sanitizedConfigInfo.bIncludeGroundPoints);

	if (sanitizedConfigInfo.RangeLimitM > 0.f)
	{
		optionsObject->SetNumberField(TEXT("rangeLimitM"), sanitizedConfigInfo.RangeLimitM);
	}
	return optionsObject;
}

// /scenario/start에 전달되는 Point Cloud 설정을 로그로 남긴다
void UDeliveryBot_HttpPolicyComponent::LogPointCloudStartConfig(const FDeliveryBotPointCloudCaptureConfigInfo& pointCloudConfigInfo) const
{
	if (!bLogPythonPointCloudStartConfig)
		return;

	UE_LOG(
		LogDeliveryBotHttpPolicy,
		Log,
		TEXT("PointCloud start config: profile=%s enabled=%s every=%d maxPoints=%d rangeLimitM=%.3f includeGround=%s runId=%s root=%s"),
		*pointCloudConfigInfo.ObservationProfile,
		pointCloudConfigInfo.bCaptureEnabled ? TEXT("true") : TEXT("false"),
		pointCloudConfigInfo.CaptureEveryNSensorFrames,
		pointCloudConfigInfo.MaxPoints,
		pointCloudConfigInfo.RangeLimitM,
		pointCloudConfigInfo.bIncludeGroundPoints ? TEXT("true") : TEXT("false"),
		*EpisodeId,
		*BuildPointCloudCaptureRootDirectory());
}

// Python capture artifact 저장 경로 JSON을 만든다.
TSharedRef<FJsonObject> UDeliveryBot_HttpPolicyComponent::BuildArtifactSpecObject() const
{
	const FString captureRoot = BuildPointCloudCaptureRootDirectory();
	const FString captureRootRelative = bProjectEpisodeOutputRequired
		? ProjectEpisodeOutputRelativeDirectory
		: FString(TEXT("captures"));

	TSharedRef<FJsonObject> artifactSpecObject = MakeShared<FJsonObject>();
	artifactSpecObject->SetStringField(TEXT("capturesRoot"), captureRoot);
	artifactSpecObject->SetStringField(TEXT("capturesRootRelative"), captureRootRelative);
	artifactSpecObject->SetBoolField(TEXT("required"), bProjectEpisodeOutputRequired);
	if (!ProjectEpisodeOutputErrorCode.IsEmpty())
	{
		artifactSpecObject->SetStringField(TEXT("configurationErrorCode"), ProjectEpisodeOutputErrorCode);
		artifactSpecObject->SetStringField(TEXT("configurationErrorMessage"), ProjectEpisodeOutputErrorMessage);
	}

	return artifactSpecObject;
}

// Point Cloud capture 저장 루트 경로를 만든다.
FString UDeliveryBot_HttpPolicyComponent::BuildPointCloudCaptureRootDirectory() const
{
	if (bProjectEpisodeOutputRequired)
	{
		return ProjectEpisodeOutputDirectory;
	}

	const FString scenarioFolderName = FString::Printf(
		TEXT("scenario_%03d"),
		FMath::Max(0, PythonPointCloudScenarioNumber));

	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("LidarPointCloudCaptures"),
		scenarioFolderName,
		EpisodeId,
		TEXT("captures"));
}

// 사람이 읽기 쉬운 point cloud capture run id를 만든다.
FString UDeliveryBot_HttpPolicyComponent::BuildPointCloudCaptureRunId(const FDeliveryBotObservationInfo& observation) const
{
	const FString timestampText = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	return FString::Printf(
		TEXT("seq_%06d_sensor_%06d_%s"),
		FMath::Max(0, observation.Sequence),
		FMath::Max(0, observation.SensorSequence),
		*timestampText);
}

// Python response.captures를 capture ref 배열로 변환한다.
TArray<FDeliveryBotPythonCaptureRefInfo> UDeliveryBot_HttpPolicyComponent::BuildPythonCaptureRefs(const TSharedPtr<FJsonObject>& responseObject) const
{
	TArray<FDeliveryBotPythonCaptureRefInfo> result;

	if (!responseObject.IsValid())
		return result;

	TArray<TSharedPtr<FJsonValue>> captureValues;
	if (!TryGetJsonArrayField(*responseObject, TEXT("captures"), captureValues))
		return result;

	for (const TSharedPtr<FJsonValue>& captureValue : captureValues)
	{
		if (!captureValue.IsValid() || captureValue->Type != EJson::Object)
			continue;

		const TSharedPtr<FJsonObject> captureObject = captureValue->AsObject();
		if (!captureObject.IsValid())
			continue;

		FDeliveryBotPythonCaptureRefInfo captureRef;
		double numberValue = 0.0;

		captureObject->TryGetStringField(TEXT("captureType"), captureRef.CaptureType);
		captureObject->TryGetStringField(TEXT("sensorId"), captureRef.SensorId);
		captureObject->TryGetStringField(TEXT("format"), captureRef.Format);
		captureObject->TryGetStringField(TEXT("path"), captureRef.Path);

		if (captureObject->TryGetNumberField(TEXT("sensorSequence"), numberValue))
			captureRef.SensorSequence = static_cast<int32>(numberValue);

		if (captureObject->TryGetNumberField(TEXT("sensorTimeSeconds"), numberValue))
			captureRef.SensorTimeSeconds = static_cast<float>(numberValue);

		if (captureObject->TryGetNumberField(TEXT("runTimeSeconds"), numberValue))
			captureRef.RunTimeSeconds = static_cast<float>(numberValue);

		if (!captureRef.CaptureType.IsEmpty() || !captureRef.Path.IsEmpty())
			result.Add(captureRef);
	}

	return result;
}

// Python response.decision을 policy decision metadata로 변환한다.
FDeliveryBotPolicyDecisionInfo UDeliveryBot_HttpPolicyComponent::BuildPythonDecisionInfo(const TSharedPtr<FJsonObject>& responseObject) const
{
	FDeliveryBotPolicyDecisionInfo decisionInfo;

	if (!responseObject.IsValid())
		return decisionInfo;

	TSharedPtr<FJsonObject> decisionObject;
	if (TryGetJsonObjectField(*responseObject, TEXT("decision"), decisionObject))
	{
		decisionObject->TryGetStringField(TEXT("selectedPolicy"), decisionInfo.SelectedPolicy);
		decisionObject->TryGetStringField(TEXT("reason"), decisionInfo.Reason);
	}

	TSharedPtr<FJsonObject> debugObject;
	if (TryGetJsonObjectField(*responseObject, TEXT("debug"), debugObject))
	{
		TryCopyStringField(debugObject, TEXT("selectedPolicy"), decisionInfo.SelectedPolicy);
		TryCopyStringField(debugObject, TEXT("reason"), decisionInfo.Reason);
	}

	return decisionInfo;
}

// Python capture refs를 로그로 남긴다.
void UDeliveryBot_HttpPolicyComponent::LogPythonCaptureRefs(const TArray<FDeliveryBotPythonCaptureRefInfo>& captureRefs) const
{
	for (const FDeliveryBotPythonCaptureRefInfo& captureRef : captureRefs)
	{
		UE_LOG(
			LogDeliveryBotHttpPolicy,
			Log,
			TEXT("Python capture ref: type=%s sensorSequence=%d path=%s"),
			*captureRef.CaptureType,
			captureRef.SensorSequence,
			*captureRef.Path);
	}
}

// 마지막 policy decision을 error 상태로 저장한다.
void UDeliveryBot_HttpPolicyComponent::StorePolicyDecisionError(const FString& errorCode, const FString& errorMessage)
{
	LastPolicyDecisionResult = FDeliveryBotPolicyDecisionResultInfo{};
	LastPolicyDecisionResult.Sequence = LastDecisionSequence;
	LastPolicyDecisionResult.RunTimeSeconds = LastDecisionRunTimeSeconds;
	LastPolicyDecisionResult.Status = EDeliveryBotPolicyDecisionStatusTypes::Error;
	LastPolicyDecisionResult.ErrorCode = errorCode;
	LastPolicyDecisionResult.ErrorMessage = errorMessage;
}

// Python policy/server event snapshot을 평가 subsystem으로 전달한다.
void UDeliveryBot_HttpPolicyComponent::EmitPolicyEventSnapshot(const FDeliveryBotPolicyEventSnapshot& snapshot) const
{
	if (snapshot.EventType == EEpisodeEvaluationEventType::None)
		return;

	UWorld* world = GetWorld();
	if (!IsValid(world))
		return;

	UScenarioEvaluationSubsystem* evaluationSubsystem = world->GetSubsystem<UScenarioEvaluationSubsystem>();
	if (!IsValid(evaluationSubsystem))
		return;

	ADeliveryBot* deliveryBot = Cast<ADeliveryBot>(GetOwner());
	if (!IsValid(deliveryBot))
		return;

	evaluationSubsystem->ReportDeliveryBotPolicyEvent(deliveryBot, snapshot);
}

void UDeliveryBot_HttpPolicyComponent::EmitPolicyServerFailureEvent(
	const FString& endpoint,
	const FHttpResponsePtr& response,
	const FString& errorCode,
	const FString& errorMessage,
	bool bRetryable,
	bool bTerminalFailure) const
{
	FDeliveryBotPolicyEventSnapshot snapshot;
	snapshot.EventType = EEpisodeEvaluationEventType::DeliveryBotPolicyServerFailure;
	snapshot.Severity = EEpisodeEvaluationEventSeverity::Failure;
	snapshot.bTerminalFailure = bTerminalFailure;
	snapshot.Sequence = LastDecisionSequence;
	snapshot.RunTimeSeconds = LastDecisionRunTimeSeconds;
	snapshot.Endpoint = endpoint;
	snapshot.EventCode = errorCode;
	snapshot.Message = errorMessage;
	snapshot.HttpStatusCode = response.IsValid() ? response->GetResponseCode() : 0;
	snapshot.ErrorCode = errorCode;
	snapshot.ErrorMessage = errorMessage;
	snapshot.bRetryable = bRetryable;
	snapshot.ResponseBodySnippet = MakeResponseBodySnippet(response);

	if (const UDeliveryBotPythonProcessSubsystem* pythonProcessSubsystem = GetPythonProcessSubsystem())
	{
		snapshot.PythonProcessStatus = pythonProcessSubsystem->GetDebugStatus();
	}

	EmitPolicyEventSnapshot(snapshot);
}

void UDeliveryBot_HttpPolicyComponent::EmitPolicyFailureEvent(
	const FString& endpoint,
	const TSharedPtr<FJsonObject>& responseObject,
	const FString& errorCode,
	const FString& errorMessage,
	bool bRetryable,
	bool bTerminalFailure) const
{
	FDeliveryBotPolicyEventSnapshot snapshot;
	snapshot.EventType = EEpisodeEvaluationEventType::DeliveryBotPolicyFailure;
	snapshot.Severity = EEpisodeEvaluationEventSeverity::Failure;
	snapshot.bTerminalFailure = bTerminalFailure;
	snapshot.Sequence = LastDecisionSequence;
	snapshot.RunTimeSeconds = LastDecisionRunTimeSeconds;
	snapshot.Endpoint = endpoint;
	snapshot.EventCode = errorCode;
	snapshot.Message = errorMessage;
	snapshot.ErrorCode = errorCode;
	snapshot.ErrorMessage = errorMessage;
	snapshot.bRetryable = bRetryable;

	if (responseObject.IsValid())
	{
		TSharedPtr<FJsonObject> debugObject;
		if (TryGetJsonObjectField(*responseObject, TEXT("debug"), debugObject))
		{
			TryCopyStringField(debugObject, TEXT("selectedPolicy"), snapshot.SelectedPolicy);
			TryCopyStringField(debugObject, TEXT("reason"), snapshot.Reason);
		}

		TSharedPtr<FJsonObject> decisionObject;
		if (TryGetJsonObjectField(*responseObject, TEXT("decision"), decisionObject))
		{
			TryCopyStringField(decisionObject, TEXT("selectedPolicy"), snapshot.SelectedPolicy);
			TryCopyStringField(decisionObject, TEXT("reason"), snapshot.Reason);
		}

		TSharedPtr<FJsonObject> errorObject;
		if (TryGetJsonObjectField(*responseObject, TEXT("error"), errorObject))
		{
			TSharedPtr<FJsonObject> detailsObject;
			if (TryGetJsonObjectField(*errorObject, TEXT("details"), detailsObject))
			{
				TryCopyStringField(detailsObject, TEXT("selectedPolicy"), snapshot.SelectedPolicy);
				TryCopyStringField(detailsObject, TEXT("reason"), snapshot.Reason);
			}
		}
	}

	EmitPolicyEventSnapshot(snapshot);
}

bool UDeliveryBot_HttpPolicyComponent::TryBuildRepathEventSnapshot(
	const TSharedPtr<FJsonObject>& sourceObject,
	const TSharedPtr<FJsonObject>& responseObject,
	FDeliveryBotPolicyEventSnapshot& outSnapshot) const
{
	if (!sourceObject.IsValid())
		return false;

	FString eventType;
	FString reason;
	sourceObject->TryGetStringField(TEXT("type"), eventType);
	sourceObject->TryGetStringField(TEXT("reason"), reason);

	const bool bExplicitRepathEvent = eventType.Equals(TEXT("repath"), ESearchCase::IgnoreCase);
	if (!bExplicitRepathEvent && !IsActualRepathReason(reason))
		return false;

	TSharedPtr<FJsonObject> debugObject;
	TSharedPtr<FJsonObject> decisionObject;
	TSharedPtr<FJsonObject> pathObject;
	if (responseObject.IsValid())
	{
		TryGetJsonObjectField(*responseObject, TEXT("debug"), debugObject);
		TryGetJsonObjectField(*responseObject, TEXT("decision"), decisionObject);
		TryGetJsonObjectField(*responseObject, TEXT("path"), pathObject);
	}

	outSnapshot = FDeliveryBotPolicyEventSnapshot{};
	outSnapshot.EventType = EEpisodeEvaluationEventType::DeliveryBotRepath;
	outSnapshot.Severity = EEpisodeEvaluationEventSeverity::Info;
	outSnapshot.Sequence = LastDecisionSequence;
	outSnapshot.RunTimeSeconds = LastDecisionRunTimeSeconds;
	outSnapshot.Endpoint = TEXT("/scenario/decide");
	outSnapshot.EventCode = TEXT("repath");

	TryCopyStringField(sourceObject, TEXT("selectedPolicy"), outSnapshot.SelectedPolicy);
	TryCopyStringField(debugObject, TEXT("selectedPolicy"), outSnapshot.SelectedPolicy);
	TryCopyStringField(decisionObject, TEXT("selectedPolicy"), outSnapshot.SelectedPolicy);

	TryCopyStringField(sourceObject, TEXT("reason"), outSnapshot.Reason);
	TryCopyStringField(debugObject, TEXT("reason"), outSnapshot.Reason);
	TryCopyStringField(decisionObject, TEXT("reason"), outSnapshot.Reason);
	if (outSnapshot.Reason.IsEmpty())
	{
		outSnapshot.Reason = TEXT("repath");
	}
	outSnapshot.Message = outSnapshot.Reason;

	TryCopyStringField(sourceObject, TEXT("pathStatus"), outSnapshot.PathStatus);
	TryCopyStringField(debugObject, TEXT("pathStatus"), outSnapshot.PathStatus);
	TryCopyStringField(pathObject, TEXT("pathStatus"), outSnapshot.PathStatus);
	TryCopyStringField(sourceObject, TEXT("lastObstacleWarningSource"), outSnapshot.LastObstacleWarningSource);
	TryCopyStringField(debugObject, TEXT("lastObstacleWarningSource"), outSnapshot.LastObstacleWarningSource);

	double numberValue = 0.0;
	bool bHasNumberValue = false;
	TryCopyNumberField(sourceObject, TEXT("pathIndex"), numberValue, bHasNumberValue);
	TryCopyNumberField(debugObject, TEXT("pathIndex"), numberValue, bHasNumberValue);
	TryCopyNumberField(pathObject, TEXT("pathIndex"), numberValue, bHasNumberValue);
	if (bHasNumberValue)
	{
		outSnapshot.PathIndex = static_cast<int32>(numberValue);
	}

	numberValue = 0.0;
	bHasNumberValue = false;
	TryCopyNumberField(sourceObject, TEXT("pathLength"), numberValue, bHasNumberValue);
	TryCopyNumberField(debugObject, TEXT("pathLength"), numberValue, bHasNumberValue);
	TryCopyNumberField(pathObject, TEXT("pathLength"), numberValue, bHasNumberValue);
	if (bHasNumberValue)
	{
		outSnapshot.PathLength = static_cast<int32>(numberValue);
	}

	numberValue = 0.0;
	bHasNumberValue = false;
	TryCopyNumberField(sourceObject, TEXT("targetPathIndex"), numberValue, bHasNumberValue);
	TryCopyNumberField(debugObject, TEXT("targetPathIndex"), numberValue, bHasNumberValue);
	TryCopyNumberField(pathObject, TEXT("targetPathIndex"), numberValue, bHasNumberValue);
	if (bHasNumberValue)
	{
		outSnapshot.TargetPathIndex = static_cast<int32>(numberValue);
	}

	numberValue = 0.0;
	bHasNumberValue = false;
	TryCopyNumberField(sourceObject, TEXT("closestPathDistanceCm"), numberValue, bHasNumberValue);
	TryCopyNumberField(debugObject, TEXT("closestPathDistanceCm"), numberValue, bHasNumberValue);
	if (bHasNumberValue)
	{
		outSnapshot.ClosestPathDistanceCm = static_cast<float>(numberValue);
	}

	numberValue = 0.0;
	bHasNumberValue = false;
	TryCopyNumberField(sourceObject, TEXT("maxPathErrorCm"), numberValue, bHasNumberValue);
	TryCopyNumberField(debugObject, TEXT("maxPathErrorCm"), numberValue, bHasNumberValue);
	if (bHasNumberValue)
	{
		outSnapshot.MaxPathErrorCm = static_cast<float>(numberValue);
	}

	numberValue = 0.0;
	bHasNumberValue = false;
	TryCopyNumberField(sourceObject, TEXT("obstacleWarningCount"), numberValue, bHasNumberValue);
	TryCopyNumberField(debugObject, TEXT("obstacleWarningCount"), numberValue, bHasNumberValue);
	if (bHasNumberValue)
	{
		outSnapshot.ObstacleWarningCount = static_cast<int32>(numberValue);
	}

	numberValue = 0.0;
	bHasNumberValue = false;
	TryCopyNumberField(sourceObject, TEXT("blockedCorridorCellCount"), numberValue, bHasNumberValue);
	TryCopyNumberField(debugObject, TEXT("blockedCorridorCellCount"), numberValue, bHasNumberValue);
	if (bHasNumberValue)
	{
		outSnapshot.BlockedCorridorCellCount = static_cast<int32>(numberValue);
	}

	numberValue = 0.0;
	bHasNumberValue = false;
	TryCopyNumberField(sourceObject, TEXT("dynamicBlockedCellCount"), numberValue, bHasNumberValue);
	TryCopyNumberField(debugObject, TEXT("dynamicBlockedCellCount"), numberValue, bHasNumberValue);
	if (bHasNumberValue)
	{
		outSnapshot.DynamicBlockedCellCount = static_cast<int32>(numberValue);
	}

	auto CopyFloatMetric = [&sourceObject, &debugObject](const FString& FieldName, float& OutValue)
	{
		double MetricValue = 0.0;
		bool bHasMetricValue = false;
		TryCopyNumberField(sourceObject, FieldName, MetricValue, bHasMetricValue);
		TryCopyNumberField(debugObject, FieldName, MetricValue, bHasMetricValue);
		if (bHasMetricValue)
		{
			OutValue = static_cast<float>(MetricValue);
		}
	};

	auto CopyIntMetric = [&sourceObject, &debugObject](const FString& FieldName, int32& OutValue)
	{
		double MetricValue = 0.0;
		bool bHasMetricValue = false;
		TryCopyNumberField(sourceObject, FieldName, MetricValue, bHasMetricValue);
		TryCopyNumberField(debugObject, FieldName, MetricValue, bHasMetricValue);
		if (bHasMetricValue)
		{
			OutValue = static_cast<int32>(MetricValue);
		}
	};

	CopyFloatMetric(TEXT("pathfindTotalMs"), outSnapshot.PathfindTotalMs);
	CopyFloatMetric(TEXT("pathfindCellLookupMs"), outSnapshot.PathfindCellLookupMs);
	CopyFloatMetric(TEXT("pathfindSoftCostMs"), outSnapshot.PathfindSoftCostMs);
	CopyFloatMetric(TEXT("pathfindSearchMs"), outSnapshot.PathfindSearchMs);
	CopyFloatMetric(TEXT("pathfindSmoothMs"), outSnapshot.PathfindSmoothMs);
	CopyIntMetric(TEXT("pathfindGridCellCount"), outSnapshot.PathfindGridCellCount);
	CopyIntMetric(TEXT("pathfindBlockedCellCount"), outSnapshot.PathfindBlockedCellCount);
	CopyIntMetric(TEXT("pathfindSoftCostCellCount"), outSnapshot.PathfindSoftCostCellCount);
	CopyIntMetric(TEXT("pathfindVisitedNodeCount"), outSnapshot.PathfindVisitedNodeCount);
	CopyIntMetric(TEXT("pathfindNeighborCheckCount"), outSnapshot.PathfindNeighborCheckCount);
	CopyIntMetric(TEXT("pathfindOpenPushCount"), outSnapshot.PathfindOpenPushCount);

	TSharedPtr<FJsonObject> targetWorldPointObject;
	if (!TryGetJsonObjectField(*sourceObject, TEXT("targetWorldPoint"), targetWorldPointObject)
		&& (!debugObject.IsValid() || !TryGetJsonObjectField(*debugObject, TEXT("targetWorldPoint"), targetWorldPointObject))
		&& (!pathObject.IsValid() || !TryGetJsonObjectField(*pathObject, TEXT("targetWorldPoint"), targetWorldPointObject)))
	{
		return true;
	}

	outSnapshot.bHasTargetWorldPoint = TryGetJsonVectorCm(targetWorldPointObject, outSnapshot.TargetWorldPointCm);
	return true;
}

void UDeliveryBot_HttpPolicyComponent::EmitPolicyEventsFromOkResponse(const TSharedPtr<FJsonObject>& responseObject) const
{
	if (!responseObject.IsValid())
		return;

	bool bEmittedRepath = false;
	TArray<TSharedPtr<FJsonValue>> eventValues;
	if (TryGetJsonArrayField(*responseObject, TEXT("events"), eventValues))
	{
		for (const TSharedPtr<FJsonValue>& eventValue : eventValues)
		{
			if (!eventValue.IsValid() || eventValue->Type != EJson::Object)
				continue;

			FDeliveryBotPolicyEventSnapshot snapshot;
			if (TryBuildRepathEventSnapshot(eventValue->AsObject(), responseObject, snapshot))
			{
				EmitPolicyEventSnapshot(snapshot);
				bEmittedRepath = true;
			}
		}
	}

	if (bEmittedRepath)
		return;

	TSharedPtr<FJsonObject> debugObject;
	if (TryGetJsonObjectField(*responseObject, TEXT("debug"), debugObject))
	{
		FDeliveryBotPolicyEventSnapshot snapshot;
		if (TryBuildRepathEventSnapshot(debugObject, responseObject, snapshot))
		{
			EmitPolicyEventSnapshot(snapshot);
			return;
		}
	}

	TSharedPtr<FJsonObject> decisionObject;
	if (TryGetJsonObjectField(*responseObject, TEXT("decision"), decisionObject))
	{
		FDeliveryBotPolicyEventSnapshot snapshot;
		if (TryBuildRepathEventSnapshot(decisionObject, responseObject, snapshot))
		{
			EmitPolicyEventSnapshot(snapshot);
		}
	}
}

bool UDeliveryBot_HttpPolicyComponent::BuildStartPayload(FString& outPayload)
{
	outPayload.Reset();

	ADeliveryBot* deliveryBot = Cast<ADeliveryBot>(GetOwner());
	if (!IsValid(deliveryBot))
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Python start payload build failed. DeliveryBot owner is invalid."));
		return false;
	}

	TSharedPtr<FJsonObject> gridObject;
	if (!BuildPythonGridObject(gridObject) || !gridObject.IsValid())
	{
		UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Python start payload build failed. Grid object is unavailable."));
		return false;
	}

	RobotInstanceId = deliveryBot->GetName();

	const FDeliveryBotSetupInfo& setupInfo = deliveryBot->GetSetupInfo();
	const FDeliveryBotObservationInfo observation = deliveryBot->BuildObservation();

	if (EpisodeId.IsEmpty())
	{
		EpisodeId = BuildPointCloudCaptureRunId(observation);
	}

	TSharedRef<FJsonObject> requestObject = MakeShared<FJsonObject>();
	requestObject->SetStringField(TEXT("robotInstanceId"), RobotInstanceId);

	requestObject->SetObjectField(
		TEXT("start"),
		BuildLocationObject(deliveryBot->GetActorLocation(), deliveryBot->GetActorRotation().Yaw));

	TSharedRef<FJsonObject> goalObject = MakeShared<FJsonObject>();
	goalObject->SetBoolField(TEXT("hasGoal"), setupInfo.LocationSetupInfo.bHasGoal);
	goalObject->SetNumberField(TEXT("x"), setupInfo.LocationSetupInfo.GoalLocationCm.X);
	goalObject->SetNumberField(TEXT("y"), setupInfo.LocationSetupInfo.GoalLocationCm.Y);
	goalObject->SetNumberField(TEXT("z"), setupInfo.LocationSetupInfo.GoalLocationCm.Z);
	requestObject->SetObjectField(TEXT("goal"), goalObject);

	requestObject->SetObjectField(TEXT("grid"), gridObject);

	const FVector robotBodySizeCm = observation.VehicleSpec.RobotBoxExtentCm * 2.0;

	TSharedRef<FJsonObject> robotSpecObject = MakeShared<FJsonObject>();
	robotSpecObject->SetNumberField(TEXT("maxSpeedKmh"), observation.VehicleSpec.MaxSpeedKmh);
	robotSpecObject->SetNumberField(TEXT("maxReverseSpeedKmh"), observation.VehicleSpec.MaxReverseSpeedKmh);
	robotSpecObject->SetNumberField(TEXT("bodyLengthCm"), robotBodySizeCm.X);
	robotSpecObject->SetNumberField(TEXT("bodyWidthCm"), robotBodySizeCm.Y);
	robotSpecObject->SetNumberField(TEXT("bodyHeightCm"), robotBodySizeCm.Z);
	robotSpecObject->SetNumberField(TEXT("wheelBaseCm"), observation.VehicleSpec.WheelBaseCm);
	robotSpecObject->SetNumberField(TEXT("turningRadiusCm"), observation.VehicleSpec.MinTurningRadiusCm);
	requestObject->SetObjectField(TEXT("robotSpec"), robotSpecObject);

	TSharedRef<FJsonObject> driveSpecObject = MakeShared<FJsonObject>();
	driveSpecObject->SetNumberField(TEXT("accelerationRateKmhPerSecond"), setupInfo.ChaosDriveConfigInfo.AccelerationRateKmhPerSecond);
	driveSpecObject->SetNumberField(TEXT("decelerationRateKmhPerSecond"), setupInfo.ChaosDriveConfigInfo.DecelerationRateKmhPerSecond);
	driveSpecObject->SetNumberField(TEXT("steeringInputRatePerSecond"), setupInfo.ChaosDriveConfigInfo.SteeringInputRatePerSecond);
	driveSpecObject->SetNumberField(TEXT("throttleInputRatePerSecond"), setupInfo.ChaosDriveConfigInfo.ThrottleInputRatePerSecond);
	driveSpecObject->SetNumberField(TEXT("brakeInputRatePerSecond"), setupInfo.ChaosDriveConfigInfo.BrakeInputRatePerSecond);
	driveSpecObject->SetNumberField(TEXT("stopBrakeInput"), setupInfo.ChaosDriveConfigInfo.StopBrakeInput);
	requestObject->SetObjectField(TEXT("driveSpec"), driveSpecObject);

	TSharedRef<FJsonObject> lidarSpecObject = MakeShared<FJsonObject>();
	const FName lidarSensorModelName =
		FDeliveryBotLidarRayPattern::GetSensorModelName(setupInfo.LidarSensorConfigInfo.LidarModeType);
	if (!lidarSensorModelName.IsNone())
	{
		lidarSpecObject->SetStringField(TEXT("sensorModel"), lidarSensorModelName.ToString());
	}
	lidarSpecObject->SetNumberField(TEXT("scanRangeM"), setupInfo.LidarSensorConfigInfo.ScanRangeM);
	lidarSpecObject->SetNumberField(TEXT("angleStepDegree"), setupInfo.LidarSensorConfigInfo.AngleStepDegree);
	lidarSpecObject->SetNumberField(TEXT("sensorHeightM"), setupInfo.LidarSensorConfigInfo.SensorHeightM);
	lidarSpecObject->SetNumberField(TEXT("frontHalfAngleDegree"), setupInfo.LidarSensorConfigInfo.FrontHalfAngleDegree);
	lidarSpecObject->SetNumberField(TEXT("stopDistanceM"), setupInfo.LidarSensorConfigInfo.StopDistanceM);
	lidarSpecObject->SetNumberField(TEXT("obstacleWarningDistanceM"), setupInfo.LidarSensorConfigInfo.ObstacleWarningDistanceM);
	lidarSpecObject->SetNumberField(TEXT("slowDownDistanceM"), setupInfo.LidarSensorConfigInfo.SlowDownDistanceM);
	lidarSpecObject->SetNumberField(TEXT("collisionStopHalfAngleDegree"), setupInfo.LidarSensorConfigInfo.CollisionStopHalfAngleDegree);
	lidarSpecObject->SetNumberField(TEXT("collisionStopDistanceM"), setupInfo.LidarSensorConfigInfo.CollisionStopDistanceM);
	lidarSpecObject->SetNumberField(TEXT("scanRateHz"), setupInfo.LidarSensorConfigInfo.ScanRateHz);
	if (FDeliveryBotLidarRayPattern::IsOusterOS1Mode(setupInfo.LidarSensorConfigInfo.LidarModeType))
	{
		lidarSpecObject->SetNumberField(
			TEXT("channelCount"),
			FDeliveryBotLidarRayPattern::GetOusterOS1ChannelCount());
		lidarSpecObject->SetNumberField(
			TEXT("horizontalColumns"),
			FDeliveryBotLidarRayPattern::CountYawSamples(setupInfo.LidarSensorConfigInfo));
		lidarSpecObject->SetNumberField(
			TEXT("verticalFovDegree"),
			FDeliveryBotLidarRayPattern::GetOusterOS1VerticalFovDegree());
	}
	const FDeliveryBotPointCloudCaptureConfigInfo pointCloudConfigInfo = BuildEffectivePointCloudCaptureConfigInfo(setupInfo.PointCloudCaptureConfigInfo);
	LogPointCloudStartConfig(pointCloudConfigInfo);
	lidarSpecObject->SetStringField(TEXT("observationProfile"), pointCloudConfigInfo.ObservationProfile);
	lidarSpecObject->SetObjectField(TEXT("pointCloudOptions"), BuildPointCloudOptionsObject(pointCloudConfigInfo));
	
	requestObject->SetObjectField(TEXT("lidarSpec"), lidarSpecObject);
	requestObject->SetObjectField(TEXT("artifactSpec"), BuildArtifactSpecObject());
	
	return BuildMessagePayload(TEXT("scenario_start"), requestObject, outPayload);
}

// /scenario/decide 요청 envelope body를 만든다.
bool UDeliveryBot_HttpPolicyComponent::BuildDecidePayload(FString& outPayload)
{
	outPayload.Reset();

	ADeliveryBot* deliveryBot = Cast<ADeliveryBot>(GetOwner());
	if (!IsValid(deliveryBot))
		return false;

	const FDeliveryBotSetupInfo& setupInfo = deliveryBot->GetSetupInfo();
	const FDeliveryBotObservationInfo observation = deliveryBot->BuildPolicyObservation();
	const EDeliveryBotLidarModeType lidarModeType = setupInfo.LidarSensorConfigInfo.LidarModeType;
	LastDecisionSequence = observation.Sequence;
	LastDecisionRunTimeSeconds = observation.WorldTimeSeconds;
	
	TSharedRef<FJsonObject> requestObject = MakeShared<FJsonObject>();
	requestObject->SetNumberField(TEXT("sequence"), observation.Sequence);
	requestObject->SetNumberField(TEXT("runTimeSeconds"), observation.WorldTimeSeconds);
	requestObject->SetNumberField(TEXT("sensorSequence"), observation.SensorSequence);
	requestObject->SetNumberField(TEXT("sensorTimeSeconds"), observation.LidarScanInfo.SimulationTimeSeconds);

	TSharedRef<FJsonObject> robotStateObject = MakeShared<FJsonObject>();
	robotStateObject->SetNumberField(TEXT("x"), observation.RobotState.LocationCm.X);
	robotStateObject->SetNumberField(TEXT("y"), observation.RobotState.LocationCm.Y);
	robotStateObject->SetNumberField(TEXT("z"), observation.RobotState.LocationCm.Z);
	robotStateObject->SetNumberField(TEXT("yawDegree"), observation.RobotState.YawDegree);
	robotStateObject->SetNumberField(TEXT("speedKmh"), observation.RobotState.SpeedKmh);
	robotStateObject->SetBoolField(TEXT("bColliding"), observation.RobotState.bColliding);
	robotStateObject->SetStringField(TEXT("collisionActorName"), observation.RobotState.CollisionActorName);
	robotStateObject->SetArrayField(TEXT("collisionActorTags"), MakeJsonStringArrayFromNames(observation.RobotState.CollisionActorTags));
	requestObject->SetObjectField(TEXT("robotState"), robotStateObject);

	TArray<TSharedPtr<FJsonValue>> legacyLidarRayValues;
	TArray<TSharedPtr<FJsonValue>> lidarRay1DValues;
	TArray<TSharedPtr<FJsonValue>> lidarRay2DValues;
	TArray<TSharedPtr<FJsonValue>> lidarRay3DValues;
	TArray<const FDeliveryBotLidarRayInfo*> lidarRay3DInfosForPayload;
	const int32 rawLidarRay1DCount = CountLidarRaysForDimension(
		observation.LidarScanInfo.RayInfos,
		EDeliveryBotLidarRayDimensionType::OneD);
	const int32 rawLidarRay2DCount = CountLidarRaysForDimension(
		observation.LidarScanInfo.RayInfos,
		EDeliveryBotLidarRayDimensionType::TwoD);
	const int32 rawLidarRay3DCount = CountLidarRaysForDimension(
		observation.LidarScanInfo.RayInfos,
		EDeliveryBotLidarRayDimensionType::ThreeD);
	const FString policyRaySelectionMode = ResolvePolicyRaySelectionMode(
		lidarModeType,
		rawLidarRay1DCount,
		rawLidarRay2DCount,
		rawLidarRay3DCount,
		rawLidarRay2DCount);
	const bool bSendOneDRaysToPolicy = bSendFullLidarRaysToPythonPolicy || policyRaySelectionMode == TEXT("1d");
	const bool bSendTwoDRaysToPolicy = bSendFullLidarRaysToPythonPolicy || policyRaySelectionMode == TEXT("2d");
	const bool bSendLegacyTwoDRaysToPolicy = bSendFullLidarRaysToPythonPolicy || policyRaySelectionMode == TEXT("legacy2d");
	const bool bSendThreeDRaysToPolicy = bSendFullLidarRaysToPythonPolicy || policyRaySelectionMode == TEXT("3d");
	if (bSendThreeDRaysToPolicy)
	{
		Build3DRayPayloadInfos(
			observation.LidarScanInfo.RayInfos,
			bSendFullLidarRaysToPythonPolicy,
			lidarRay3DInfosForPayload);
	}

	legacyLidarRayValues.Reserve(observation.LidarScanInfo.RayInfos.Num());
	lidarRay1DValues.Reserve(observation.LidarScanInfo.RayInfos.Num());
	lidarRay2DValues.Reserve(observation.LidarScanInfo.RayInfos.Num());
	lidarRay3DValues.Reserve(lidarRay3DInfosForPayload.Num());

	for (const FDeliveryBotLidarRayInfo& rayInfo : observation.LidarScanInfo.RayInfos)
	{
		switch (rayInfo.RayDimensionType)
		{
		case EDeliveryBotLidarRayDimensionType::OneD:
			if (bSendOneDRaysToPolicy)
			{
				lidarRay1DValues.Add(MakeShared<FJsonValueObject>(MakeJsonLidarRay1DObject(rayInfo)));
			}
			break;

		case EDeliveryBotLidarRayDimensionType::TwoD:
			if (bSendTwoDRaysToPolicy)
			{
				lidarRay2DValues.Add(MakeShared<FJsonValueObject>(MakeJsonLidarRay2DObject(rayInfo)));
			}
			if (bSendLegacyTwoDRaysToPolicy)
			{
				legacyLidarRayValues.Add(MakeShared<FJsonValueObject>(MakeJsonLegacyLidarRayObject(rayInfo)));
			}
			break;

		case EDeliveryBotLidarRayDimensionType::ThreeD:
			break;

		default:
			break;
		}
	}

	for (const FDeliveryBotLidarRayInfo* rayInfo : lidarRay3DInfosForPayload)
	{
		if (rayInfo != nullptr)
		{
			lidarRay3DValues.Add(MakeShared<FJsonValueObject>(MakeJsonLidarRay3DObject(*rayInfo)));
		}
	}

	requestObject->SetArrayField(TEXT("lidarRays"), legacyLidarRayValues);

	TSharedRef<FJsonObject> lidarObject = MakeShared<FJsonObject>();
	lidarObject->SetStringField(TEXT("mode"), ToJsonLidarModeString(lidarModeType));
	lidarObject->SetNumberField(TEXT("sensorSequence"), observation.SensorSequence);
	lidarObject->SetNumberField(TEXT("sensorTimeSeconds"), observation.LidarScanInfo.SimulationTimeSeconds);
	lidarObject->SetArrayField(TEXT("rays1d"), lidarRay1DValues);
	lidarObject->SetArrayField(TEXT("rays2d"), lidarRay2DValues);
	lidarObject->SetArrayField(TEXT("rays3d"), lidarRay3DValues);
	lidarObject->SetNumberField(TEXT("rawRays3dCount"), rawLidarRay3DCount);
	lidarObject->SetNumberField(TEXT("transmittedRays3dCount"), lidarRay3DValues.Num());
	lidarObject->SetBoolField(TEXT("sendFullRays"), bSendFullLidarRaysToPythonPolicy);
	lidarObject->SetStringField(
		TEXT("rayPayloadMode"),
		bSendFullLidarRaysToPythonPolicy ? TEXT("full") : TEXT("policy"));
	lidarObject->SetBoolField(
		TEXT("rays3dCompacted"),
		rawLidarRay3DCount > lidarRay3DValues.Num());
	requestObject->SetObjectField(TEXT("lidar"), lidarObject);

	TArray<TSharedPtr<FJsonValue>> observedObjectValues;
	observedObjectValues.Reserve(observation.ObservedObjects.Num());

	for (const FDeliveryBotLidarObservedObjectInfo& objectInfo : observation.ObservedObjects)
	{
		TSharedRef<FJsonObject> objectJson = MakeShared<FJsonObject>();
		objectJson->SetStringField(TEXT("actorName"), objectInfo.ActorName);
		objectJson->SetArrayField(TEXT("actorTags"), MakeJsonStringArrayFromNames(objectInfo.ActorTags));
		objectJson->SetStringField(TEXT("targetId"), objectInfo.TargetId);
		objectJson->SetArrayField(TEXT("targetTags"), MakeJsonStringArrayFromNames(objectInfo.TargetTags));
		objectJson->SetBoolField(TEXT("blocksPolicy"), objectInfo.bBlocksPolicy);
		objectJson->SetBoolField(TEXT("hasBounds"), objectInfo.bHasBounds);
		objectJson->SetObjectField(TEXT("boundsOriginCm"), MakeJsonVectorObject(objectInfo.BoundsOriginCm));
		objectJson->SetObjectField(TEXT("boundsExtentCm"), MakeJsonVectorObject(objectInfo.BoundsExtentCm));
		objectJson->SetObjectField(TEXT("closestHitLocationCm"), MakeJsonVectorObject(objectInfo.ClosestHitLocationCm));
		objectJson->SetNumberField(TEXT("closestDistanceM"), objectInfo.ClosestDistanceM);
		objectJson->SetNumberField(TEXT("closestRayYawDegree"), objectInfo.ClosestRayYawDegree);
		objectJson->SetNumberField(TEXT("totalHitRayCount"), objectInfo.TotalHitRayCount);
		objectJson->SetNumberField(TEXT("frontHitRayCount"), objectInfo.FrontHitRayCount);
		objectJson->SetBoolField(TEXT("inFront"), objectInfo.bInFront);

		observedObjectValues.Add(MakeShared<FJsonValueObject>(objectJson));
	}

	requestObject->SetArrayField(TEXT("observedObjects"), observedObjectValues);

	robotStateObject->SetStringField(TEXT("collisionTargetId"), observation.RobotState.CollisionTargetId);
	robotStateObject->SetArrayField(TEXT("collisionTargetTags"), MakeJsonStringArrayFromNames(observation.RobotState.CollisionTargetTags));
	lidarObject->SetObjectField(
		TEXT("policyRaySelection"),
		MakeJsonPolicyRaySelectionObject(
			lidarModeType,
			observation.LidarScanInfo.RayInfos,
			lidarRay1DValues.Num(),
			lidarRay2DValues.Num(),
			lidarRay3DValues.Num(),
			legacyLidarRayValues.Num()));
	const bool bPayloadBuilt = BuildMessagePayload(TEXT("scenario_decide"), requestObject, outPayload);
	LastDecisionRequestObject = requestObject;
	return bPayloadBuilt;
}

// envelope 응답에서 response 객체를 가져온다.
bool UDeliveryBot_HttpPolicyComponent::TryGetPythonResponseObject(const FHttpResponsePtr& response, TSharedPtr<FJsonObject>& outResponseObject) const
{
	outResponseObject.Reset();

	if (!response.IsValid() || response->GetResponseCode() < 200 || response->GetResponseCode() >= 300)
		return false;

	TSharedPtr<FJsonObject> rootObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(response->GetContentAsString());
	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
		return false;

	return TryGetJsonObjectField(*rootObject, TEXT("response"), outResponseObject);
}

// envelope 응답의 response.status가 ok인지 확인한다.
bool UDeliveryBot_HttpPolicyComponent::IsPythonResponseOk(const FHttpResponsePtr& response) const
{
	TSharedPtr<FJsonObject> responseObject;
	if (!TryGetPythonResponseObject(response, responseObject))
		return false;

	FString status;
	return responseObject->TryGetStringField(TEXT("status"), status)
		&& status.Equals(TEXT("ok"), ESearchCase::IgnoreCase);
}

// /scenario/decide envelope 응답의 response.action을 이동 명령으로 변환하고 마지막 policy decision 결과를 저장한다.
bool UDeliveryBot_HttpPolicyComponent::TryParseMoveCommand(
	const FHttpResponsePtr& response,
	FDeliveryBotMoveCommandInfo& outMoveCommand)
{
	outMoveCommand = FDeliveryBotMoveCommandInfo{};

	TSharedPtr<FJsonObject> responseObject;
	if (!TryGetPythonResponseObject(response, responseObject))
	{
		StorePolicyDecisionError(TEXT("PYTHON_RESPONSE_INVALID"), TEXT("Python decide response envelope is invalid."));
		EmitPolicyServerFailureEvent(
			TEXT("/scenario/decide"),
			response,
			TEXT("PYTHON_RESPONSE_INVALID"),
			TEXT("Python decide response envelope is invalid."),
			false,
			true);
		return false;
	}

	FString status;
	if (!responseObject->TryGetStringField(TEXT("status"), status) || !status.Equals(TEXT("ok"), ESearchCase::IgnoreCase))
	{
		FString errorCode = TEXT("PYTHON_DECIDE_FAILED");
		FString errorMessage = TEXT("Python decide response status is not ok.");
		bool bRetryable = false;

		TSharedPtr<FJsonObject> errorObject;
		if (TryGetJsonObjectField(*responseObject, TEXT("error"), errorObject))
		{
			errorObject->TryGetStringField(TEXT("code"), errorCode);
			errorObject->TryGetStringField(TEXT("message"), errorMessage);
			errorObject->TryGetBoolField(TEXT("retryable"), bRetryable);
		}

		StorePolicyDecisionError(errorCode, errorMessage);
		EmitPolicyFailureEvent(
			TEXT("/scenario/decide"),
			responseObject,
			errorCode,
			errorMessage,
			bRetryable,
			true);
		return false;
	}

	DrawPythonPathDebug(responseObject);

	TSharedPtr<FJsonObject> actionObject;
	if (!TryGetJsonObjectField(*responseObject, TEXT("action"), actionObject))
	{
		StorePolicyDecisionError(TEXT("PYTHON_ACTION_MISSING"), TEXT("Python decide response.action is missing."));
		EmitPolicyFailureEvent(
			TEXT("/scenario/decide"),
			responseObject,
			TEXT("PYTHON_ACTION_MISSING"),
			TEXT("Python decide response.action is missing."),
			false,
			true);
		return false;
	}

	double steering = 0.0;
	double targetSpeedKmh = 0.0;
	double brake = 0.0;

	actionObject->TryGetNumberField(TEXT("steering"), steering);
	actionObject->TryGetNumberField(TEXT("targetSpeedKmh"), targetSpeedKmh);
	actionObject->TryGetNumberField(TEXT("brake"), brake);

	outMoveCommand.Steering = FMath::Clamp(static_cast<float>(steering), -1.f, 1.f);
	outMoveCommand.TargetSpeedKmh = FMath::Max(static_cast<float>(targetSpeedKmh), 0.f);
	outMoveCommand.Brake = FMath::Clamp(static_cast<float>(brake), 0.f, 1.f);
	outMoveCommand.bBrake = outMoveCommand.Brake > KINDA_SMALL_NUMBER;

	outMoveCommand.MoveDirectionType = EDeliveryBotMoveDirectionType::Forward;

	LastPolicyDecisionResult = FDeliveryBotPolicyDecisionResultInfo{};
	LastPolicyDecisionResult.Sequence = LastDecisionSequence;
	LastPolicyDecisionResult.RunTimeSeconds = LastDecisionRunTimeSeconds;
	LastPolicyDecisionResult.Status = EDeliveryBotPolicyDecisionStatusTypes::Ok;
	LastPolicyDecisionResult.MoveCommand = outMoveCommand;
	LastPolicyDecisionResult.Decision = BuildPythonDecisionInfo(responseObject);
	LastPolicyDecisionResult.CaptureRefs = BuildPythonCaptureRefs(responseObject);

	EmitPolicyEventsFromOkResponse(responseObject);
	LogPythonCaptureRefs(LastPolicyDecisionResult.CaptureRefs);

	return true;
}

// Python response.path 좌표를 경로선, 현재 인덱스, 실제 추종 목표점으로 그린다.
void UDeliveryBot_HttpPolicyComponent::DrawPythonPathDebug(const TSharedPtr<FJsonObject>& responseObject) const
{
	if (!bDrawPythonPathDebug || !responseObject.IsValid())
		return;

	UWorld* world = GetWorld();
	if (!IsValid(world))
		return;

	TSharedPtr<FJsonObject> pathObject;
	if (!TryGetJsonObjectField(*responseObject, TEXT("path"), pathObject))
		return;

	const TArray<TSharedPtr<FJsonValue>>* pathValues = nullptr;
	if (!pathObject->TryGetArrayField(TEXT("pathWorldPoints"), pathValues) || pathValues == nullptr || pathValues->Num() < 2)
		return;

	TArray<FVector> pathPoints;
	pathPoints.Reserve(pathValues->Num());

	for (const TSharedPtr<FJsonValue>& pointValue : *pathValues)
	{
		FVector locationCm;
		if (!TryParsePythonPathDebugPoint(pointValue, locationCm))
			continue;

		locationCm.Z += PythonPathDebugHeightCm;
		pathPoints.Add(locationCm);
	}

	for (int32 index = 1; index < pathPoints.Num(); ++index)
	{
		DrawDebugLine(
			world,
			pathPoints[index - 1],
			pathPoints[index],
			FColor::Cyan,
			false,
			DecideIntervalSeconds * 2.f,
			0,
			PythonPathDebugLineThickness);
	}

	double pathIndex = 0.0;
	if (pathObject->TryGetNumberField(TEXT("pathIndex"), pathIndex))
	{
		const int32 currentIndex = FMath::Clamp(static_cast<int32>(pathIndex), 0, pathPoints.Num() - 1);
		DrawDebugSphere(
			world,
			pathPoints[currentIndex],
			18.f,
			12,
			FColor::Yellow,
			false,
			DecideIntervalSeconds * 2.f,
			0,
			2.f);
	}

	TSharedPtr<FJsonObject> targetPointObject;
	if (TryGetJsonObjectField(*pathObject, TEXT("targetWorldPoint"), targetPointObject))
	{
		double x = 0.0;
		double y = 0.0;
		double z = 0.0;

		if (targetPointObject->TryGetNumberField(TEXT("x"), x) &&
			targetPointObject->TryGetNumberField(TEXT("y"), y))
		{
			targetPointObject->TryGetNumberField(TEXT("z"), z);

			FVector targetLocationCm(
				static_cast<float>(x),
				static_cast<float>(y),
				static_cast<float>(z) + PythonPathDebugHeightCm + 20.f);

			DrawDebugSphere(
				world,
				targetLocationCm,
				24.f,
				12,
				FColor::Green,
				false,
				DecideIntervalSeconds * 2.f,
				0,
				3.f);

			if (const AActor* owner = GetOwner(); IsValid(owner))
			{
				DrawDebugLine(
					world,
					owner->GetActorLocation() + FVector(0.f, 0.f, 45.f),
					targetLocationCm,
					FColor::Green,
					false,
					DecideIntervalSeconds * 2.f,
					0,
					3.f);
			}
		}
	}
}

// path debug point JSON을 FVector로 변환한다.
bool UDeliveryBot_HttpPolicyComponent::TryParsePythonPathDebugPoint(
	const TSharedPtr<FJsonValue>& pointValue,
	FVector& outLocationCm) const
{
	outLocationCm = FVector::ZeroVector;

	if (!pointValue.IsValid() || pointValue->Type != EJson::Object)
		return false;

	const TSharedPtr<FJsonObject> pointObject = pointValue->AsObject();
	if (!pointObject.IsValid())
		return false;

	double x = 0.0;
	double y = 0.0;
	double z = 0.0;

	if (!pointObject->TryGetNumberField(TEXT("x"), x) || !pointObject->TryGetNumberField(TEXT("y"), y))
		return false;

	pointObject->TryGetNumberField(TEXT("z"), z);

	outLocationCm = FVector(x, y, z);
	return true;
}

// /scenario/end 요청 envelope body를 만든다.
bool UDeliveryBot_HttpPolicyComponent::BuildEndPayload(const FString& status, FString& outPayload) const
{
	outPayload.Reset();

	if (EpisodeId.IsEmpty() || RobotInstanceId.IsEmpty())
	{
		return false;
	}

	TSharedRef<FJsonObject> requestObject = MakeShared<FJsonObject>();

	requestObject->SetStringField(TEXT("robotInstanceId"), RobotInstanceId);
	requestObject->SetNumberField(TEXT("sequence"), LastDecisionSequence);
	requestObject->SetStringField(TEXT("status"), status);

	return BuildMessagePayload(TEXT("scenario_end"), requestObject, outPayload);
}

// scenario 진행 상태를 초기화한다.
void UDeliveryBot_HttpPolicyComponent::ResetScenarioState()
{
	EpisodeId.Reset();
	RobotInstanceId.Reset();
	LastPolicyDecisionResult = FDeliveryBotPolicyDecisionResultInfo{};
	
	LastDecisionSequence = 0;
	LastDecisionRequestObject.Reset();
	LastDecisionRunTimeSeconds = 0.f;
	StartRetryElapsedSeconds = 0.f;
	DecideElapsedSeconds = 0.f;

	bStartRequested = false;
	bScenarioStarted = false;
	bStartRequestInFlight = false;
	bDecisionRequestInFlight = false;
	bEndRequestInFlight = false;
	bLoggedStartWaitingForPython = false;
}
