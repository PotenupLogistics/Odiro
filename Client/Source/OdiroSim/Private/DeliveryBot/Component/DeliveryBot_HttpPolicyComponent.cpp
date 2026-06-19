#include "DeliveryBot/Component/DeliveryBot_HttpPolicyComponent.h"
#include "DrawDebugHelpers.h"
#include "DeliveryBot/Actor/DeliveryBot.h"
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

	void AddJsonLidarRayTargetFields(
		TArray<TSharedPtr<FJsonValue>>& rayValues,
		const TArray<FDeliveryBotLidarRayInfo>& rayInfos,
		EDeliveryBotLidarRayDimensionType rayDimensionType)
	{
		int32 valueIndex = 0;
		for (const FDeliveryBotLidarRayInfo& rayInfo : rayInfos)
		{
			if (rayInfo.RayDimensionType != rayDimensionType)
			{
				continue;
			}

			if (!rayValues.IsValidIndex(valueIndex))
			{
				break;
			}

			const TSharedPtr<FJsonValue>& rayValue = rayValues[valueIndex];
			++valueIndex;
			if (!rayValue.IsValid() || rayValue->Type != EJson::Object)
			{
				continue;
			}

			SetJsonTargetFields(rayValue->AsObject(), rayInfo.TargetId, rayInfo.TargetTags);
		}
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

	ResetScenarioState(false);

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
bool UDeliveryBot_HttpPolicyComponent::SendPostRequest(const FString& endpoint, const FString& payload, TFunction<void(FHttpResponsePtr, bool)> onComplete)
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

	request->OnProcessRequestComplete().BindWeakLambda(this, [onComplete = MoveTemp(onComplete)](FHttpRequestPtr, FHttpResponsePtr response,
				bool bWasSuccessful) mutable{onComplete(response, bWasSuccessful);});

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

	TSharedRef<FJsonObject> artifactSpecObject = MakeShared<FJsonObject>();
	artifactSpecObject->SetStringField(TEXT("capturesRoot"), captureRoot);
	artifactSpecObject->SetStringField(TEXT("capturesRootRelative"), TEXT("captures"));

	return artifactSpecObject;
}

// Point Cloud capture 저장 루트 경로를 만든다.
FString UDeliveryBot_HttpPolicyComponent::BuildPointCloudCaptureRootDirectory() const
{
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
	if (!TryGetJsonObjectField(*responseObject, TEXT("decision"), decisionObject))
		return decisionInfo;

	decisionObject->TryGetStringField(TEXT("selectedPolicy"), decisionInfo.SelectedPolicy);
	decisionObject->TryGetStringField(TEXT("reason"), decisionInfo.Reason);

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

// /scenario/start 요청 envelope body를 만든다.
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

	legacyLidarRayValues.Reserve(observation.LidarScanInfo.RayInfos.Num());
	lidarRay1DValues.Reserve(observation.LidarScanInfo.RayInfos.Num());
	lidarRay2DValues.Reserve(observation.LidarScanInfo.RayInfos.Num());
	lidarRay3DValues.Reserve(observation.LidarScanInfo.RayInfos.Num());

	for (const FDeliveryBotLidarRayInfo& rayInfo : observation.LidarScanInfo.RayInfos)
	{
		switch (rayInfo.RayDimensionType)
		{
		case EDeliveryBotLidarRayDimensionType::OneD:
			lidarRay1DValues.Add(MakeShared<FJsonValueObject>(MakeJsonLidarRay1DObject(rayInfo)));
			break;

		case EDeliveryBotLidarRayDimensionType::TwoD:
			lidarRay2DValues.Add(MakeShared<FJsonValueObject>(MakeJsonLidarRay2DObject(rayInfo)));
			legacyLidarRayValues.Add(MakeShared<FJsonValueObject>(MakeJsonLegacyLidarRayObject(rayInfo)));
			break;

		case EDeliveryBotLidarRayDimensionType::ThreeD:
			lidarRay3DValues.Add(MakeShared<FJsonValueObject>(MakeJsonLidarRay3DObject(rayInfo)));
			break;

		default:
			break;
		}
	}

	requestObject->SetArrayField(TEXT("lidarRays"), legacyLidarRayValues);

	TSharedRef<FJsonObject> lidarObject = MakeShared<FJsonObject>();
	lidarObject->SetStringField(TEXT("mode"), ToJsonLidarModeString(setupInfo.LidarSensorConfigInfo.LidarModeType));
	lidarObject->SetNumberField(TEXT("sensorSequence"), observation.SensorSequence);
	lidarObject->SetNumberField(TEXT("sensorTimeSeconds"), observation.LidarScanInfo.SimulationTimeSeconds);
	lidarObject->SetArrayField(TEXT("rays1d"), lidarRay1DValues);
	lidarObject->SetArrayField(TEXT("rays2d"), lidarRay2DValues);
	lidarObject->SetArrayField(TEXT("rays3d"), lidarRay3DValues);
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

	const bool bPayloadBuilt = BuildMessagePayload(TEXT("scenario_decide"), requestObject, outPayload);
	robotStateObject->SetStringField(TEXT("collisionTargetId"), observation.RobotState.CollisionTargetId);
	robotStateObject->SetArrayField(TEXT("collisionTargetTags"), MakeJsonStringArrayFromNames(observation.RobotState.CollisionTargetTags));
	AddJsonLidarRayTargetFields(lidarRay1DValues, observation.LidarScanInfo.RayInfos, EDeliveryBotLidarRayDimensionType::OneD);
	AddJsonLidarRayTargetFields(lidarRay2DValues, observation.LidarScanInfo.RayInfos, EDeliveryBotLidarRayDimensionType::TwoD);
	AddJsonLidarRayTargetFields(lidarRay3DValues, observation.LidarScanInfo.RayInfos, EDeliveryBotLidarRayDimensionType::ThreeD);
	AddJsonLidarRayTargetFields(legacyLidarRayValues, observation.LidarScanInfo.RayInfos, EDeliveryBotLidarRayDimensionType::TwoD);
	lidarObject->SetObjectField(
		TEXT("policyRaySelection"),
		MakeJsonPolicyRaySelectionObject(
			setupInfo.LidarSensorConfigInfo.LidarModeType,
			observation.LidarScanInfo.RayInfos,
			lidarRay1DValues.Num(),
			lidarRay2DValues.Num(),
			lidarRay3DValues.Num(),
			legacyLidarRayValues.Num()));
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
		return false;
	}

	FString status;
	if (!responseObject->TryGetStringField(TEXT("status"), status) || !status.Equals(TEXT("ok"), ESearchCase::IgnoreCase))
	{
		StorePolicyDecisionError(TEXT("PYTHON_DECIDE_FAILED"), TEXT("Python decide response status is not ok."));
		return false;
	}

	DrawPythonPathDebug(responseObject);

	TSharedPtr<FJsonObject> actionObject;
	if (!TryGetJsonObjectField(*responseObject, TEXT("action"), actionObject))
	{
		StorePolicyDecisionError(TEXT("PYTHON_ACTION_MISSING"), TEXT("Python decide response.action is missing."));
		return false;
	}

	double steering = 0.0;
	double targetSpeedKmh = 0.0;
	double brake = 0.0;
	FString direction = TEXT("Forward");

	actionObject->TryGetNumberField(TEXT("steering"), steering);
	actionObject->TryGetNumberField(TEXT("targetSpeedKmh"), targetSpeedKmh);
	actionObject->TryGetNumberField(TEXT("brake"), brake);
	actionObject->TryGetStringField(TEXT("direction"), direction);

	outMoveCommand.Steering = FMath::Clamp(static_cast<float>(steering), -1.f, 1.f);
	outMoveCommand.TargetSpeedKmh = FMath::Max(static_cast<float>(targetSpeedKmh), 0.f);
	outMoveCommand.Brake = FMath::Clamp(static_cast<float>(brake), 0.f, 1.f);
	outMoveCommand.bBrake = outMoveCommand.Brake > KINDA_SMALL_NUMBER;

	outMoveCommand.MoveDirectionType = direction.Equals(TEXT("Reverse"), ESearchCase::IgnoreCase)
		? EDeliveryBotMoveDirectionType::Reverse
		: EDeliveryBotMoveDirectionType::Forward;

	LastPolicyDecisionResult = FDeliveryBotPolicyDecisionResultInfo{};
	LastPolicyDecisionResult.Sequence = LastDecisionSequence;
	LastPolicyDecisionResult.RunTimeSeconds = LastDecisionRunTimeSeconds;
	LastPolicyDecisionResult.Status = EDeliveryBotPolicyDecisionStatusTypes::Ok;
	LastPolicyDecisionResult.MoveCommand = outMoveCommand;
	LastPolicyDecisionResult.Decision = BuildPythonDecisionInfo(responseObject);
	LastPolicyDecisionResult.CaptureRefs = BuildPythonCaptureRefs(responseObject);

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
		return false;

	TSharedRef<FJsonObject> requestObject = MakeShared<FJsonObject>();

	requestObject->SetStringField(TEXT("robotInstanceId"), RobotInstanceId);
	requestObject->SetNumberField(TEXT("sequence"), LastDecisionSequence);
	requestObject->SetStringField(TEXT("status"), status);

	TSharedRef<FJsonObject> metricsObject = MakeShared<FJsonObject>();
	requestObject->SetObjectField(TEXT("metrics"), metricsObject);

	TSharedRef<FJsonObject> debugObject = MakeShared<FJsonObject>();
	debugObject->SetStringField(TEXT("endSource"), TEXT("UScenarioEvaluationSubsystem"));
	requestObject->SetObjectField(TEXT("debug"), debugObject);

	return BuildMessagePayload(TEXT("scenario_end"), requestObject, outPayload);
}

// scenario 진행 상태를 초기화한다.
void UDeliveryBot_HttpPolicyComponent::ResetScenarioState(bool bKeepLastResult)
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

	if (!bKeepLastResult)
	{
		LastScenarioResultJson.Reset();
	}
}

// 목표 도착 시 Python 서버에 /scenario/end 요청을 보내고 결과 JSON을 저장한다.
void UDeliveryBot_HttpPolicyComponent::EndScenario(const FString& status)
{
	if (!bScenarioStarted || bEndRequestInFlight)
		return;

	FString payload;
	if (!BuildEndPayload(status, payload))
		return;

	bEndRequestInFlight = true;

	// /scenario/end 응답을 저장하고 scenario 상태를 종료 상태로 초기화한다.
	const bool bRequestStarted = SendPostRequest(
		TEXT("/scenario/end"),
		payload,
		[this](FHttpResponsePtr response, bool bSucceeded)
		{
			bEndRequestInFlight = false;

			LastScenarioResultJson = response.IsValid() ? response->GetContentAsString() : FString();

			ResetScenarioState(true);

			// /scenario/end envelope 응답의 response.status를 확인한다.
			if (!bSucceeded || !IsPythonResponseOk(response))
			{
				UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Python scenario end failed."));
				return;
			}

			UE_LOG(LogDeliveryBotHttpPolicy, Log, TEXT("Python scenario result saved. Length=%d"), LastScenarioResultJson.Len());
		});

	if (!bRequestStarted)
	{
		bEndRequestInFlight = false;
	}
}
