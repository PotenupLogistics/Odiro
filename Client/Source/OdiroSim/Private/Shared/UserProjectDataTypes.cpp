#include "Shared/UserProjectDataTypes.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Scenario/ScenarioSampler.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Shared/ScenarioDocumentJson.h"
#include "Shared/ScenarioSampleJson.h"

namespace
{
	const TCHAR* UserProjectScenarioSampleSchema = TEXT("scenario_sample");
	const int32 UserProjectJsonVersion = 1;

	void AddUserProjectDiagnostic(
		TArray<FScenarioCompileDiagnostic>& diagnostics,
		EScenarioCompileDiagnosticSeverity severity,
		const FString& code,
		const FString& message)
	{
		FScenarioCompileDiagnostic diagnostic;
		diagnostic.Severity = severity;
		diagnostic.Code = code;
		diagnostic.Message = message;
		diagnostics.Add(diagnostic);
	}

	bool HasUserProjectErrors(const TArray<FScenarioCompileDiagnostic>& diagnostics)
	{
		for (const FScenarioCompileDiagnostic& diagnostic : diagnostics)
		{
			if (diagnostic.Severity == EScenarioCompileDiagnosticSeverity::Error)
			{
				return true;
			}
		}

		return false;
	}

	EScenarioCompileDiagnosticSeverity ToUserProjectCompileSeverity(EScenarioSchemaDiagnosticSeverity severity)
	{
		switch (severity)
		{
		case EScenarioSchemaDiagnosticSeverity::Info:
			return EScenarioCompileDiagnosticSeverity::Info;
		case EScenarioSchemaDiagnosticSeverity::Warning:
		case EScenarioSchemaDiagnosticSeverity::Repair:
			return EScenarioCompileDiagnosticSeverity::Warning;
		case EScenarioSchemaDiagnosticSeverity::Error:
			return EScenarioCompileDiagnosticSeverity::Error;
		default:
			return EScenarioCompileDiagnosticSeverity::Error;
		}
	}

	void AppendScenarioSchemaDiagnostics(
		const TArray<FScenarioSchemaDiagnostic>& schemaDiagnostics,
		TArray<FScenarioCompileDiagnostic>& diagnostics)
	{
		for (const FScenarioSchemaDiagnostic& schemaDiagnostic : schemaDiagnostics)
		{
			FString message = schemaDiagnostic.Message;
			if (!schemaDiagnostic.Path.IsEmpty())
			{
				message = FString::Printf(TEXT("%s | Path: %s"), *message, *schemaDiagnostic.Path);
			}

			AddUserProjectDiagnostic(
				diagnostics,
				ToUserProjectCompileSeverity(schemaDiagnostic.Severity),
				schemaDiagnostic.Code,
				message);
		}
	}

	bool TryLoadJsonStringFromFile(
		const FString& jsonFilePath,
		FString& outJsonString,
		TArray<FScenarioCompileDiagnostic>& diagnostics)
	{
		outJsonString.Reset();
		if (jsonFilePath.TrimStartAndEnd().IsEmpty())
		{
			AddUserProjectDiagnostic(
				diagnostics,
				EScenarioCompileDiagnosticSeverity::Error,
				TEXT("empty_json_path"),
				TEXT("JSON file path must not be empty."));
			return false;
		}

		if (!FFileHelper::LoadFileToString(outJsonString, *jsonFilePath))
		{
			AddUserProjectDiagnostic(
				diagnostics,
				EScenarioCompileDiagnosticSeverity::Error,
				TEXT("json_file_read_failed"),
				FString::Printf(TEXT("JSON file read failed: %s"), *jsonFilePath));
			return false;
		}

		return true;
	}

	bool TryParseJsonObject(
		const FString& jsonString,
		TSharedPtr<FJsonObject>& outRootObject,
		TArray<FScenarioCompileDiagnostic>& diagnostics)
	{
		outRootObject.Reset();
		const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(jsonString);
		if (!FJsonSerializer::Deserialize(reader, outRootObject) || !outRootObject.IsValid())
		{
			AddUserProjectDiagnostic(
				diagnostics,
				EScenarioCompileDiagnosticSeverity::Error,
				TEXT("invalid_json_root"),
				TEXT("JSON must be a root object."));
			return false;
		}

		return true;
	}

	bool TryReadStringField(
		const FJsonObject& jsonObject,
		const FString& fieldName,
		const FString& path,
		TArray<FScenarioCompileDiagnostic>& diagnostics,
		FString& outValue)
	{
		const TSharedPtr<FJsonValue> jsonValue = jsonObject.TryGetField(fieldName);
		if (!jsonValue.IsValid())
		{
			AddUserProjectDiagnostic(
				diagnostics,
				EScenarioCompileDiagnosticSeverity::Error,
				FString::Printf(TEXT("missing_%s"), *fieldName),
				FString::Printf(TEXT("%s.%s field is required."), *path, *fieldName));
			return false;
		}

		if (jsonValue->Type != EJson::String)
		{
			AddUserProjectDiagnostic(
				diagnostics,
				EScenarioCompileDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *fieldName),
				FString::Printf(TEXT("%s.%s must be a string."), *path, *fieldName));
			return false;
		}

		outValue = jsonValue->AsString();
		if (outValue.IsEmpty())
		{
			AddUserProjectDiagnostic(
				diagnostics,
				EScenarioCompileDiagnosticSeverity::Error,
				FString::Printf(TEXT("empty_%s"), *fieldName),
				FString::Printf(TEXT("%s.%s must not be empty."), *path, *fieldName));
			return false;
		}

		return true;
	}

	bool TryReadInt32Field(
		const FJsonObject& jsonObject,
		const FString& fieldName,
		const FString& path,
		TArray<FScenarioCompileDiagnostic>& diagnostics,
		int32& outValue)
	{
		const TSharedPtr<FJsonValue> jsonValue = jsonObject.TryGetField(fieldName);
		if (!jsonValue.IsValid() || jsonValue->Type != EJson::Number)
		{
			AddUserProjectDiagnostic(
				diagnostics,
				EScenarioCompileDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *fieldName),
				FString::Printf(TEXT("%s.%s must be a number."), *path, *fieldName));
			return false;
		}

		outValue = FMath::RoundToInt(jsonValue->AsNumber());
		return true;
	}

	FString MakeContentHash(const FString& content)
	{
		return FString::Printf(TEXT("crc32:%08x"), FCrc::StrCrc32(*content));
	}

	FString MakePolicySnapshotHash(const FUserProjectRunSnapshotPaths& paths)
	{
		FString policyEntrypointJson;
		TArray<FScenarioCompileDiagnostic> diagnostics;
		return TryLoadJsonStringFromFile(paths.PolicyEntrypointPath, policyEntrypointJson, diagnostics)
			? MakeContentHash(policyEntrypointJson)
			: FString(TEXT("unknown"));
	}

	FString MakeRunRelativePath(const FUserProjectRunSnapshotPaths& paths, FString filePath)
	{
		filePath.ReplaceInline(TEXT("\\"), TEXT("/"));
		FString runPath = paths.RunPath;
		runPath.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (FPaths::MakePathRelativeTo(filePath, *runPath))
		{
			filePath.ReplaceInline(TEXT("\\"), TEXT("/"));
		}

		return filePath;
	}

	TSharedPtr<FJsonValue> CloneJsonValue(const TSharedPtr<FJsonValue>& value)
	{
		if (!value.IsValid())
		{
			return MakeShared<FJsonValueNull>();
		}

		switch (value->Type)
		{
		case EJson::None:
		case EJson::Null:
			return MakeShared<FJsonValueNull>();
		case EJson::String:
			return MakeShared<FJsonValueString>(value->AsString());
		case EJson::Number:
			return MakeShared<FJsonValueNumber>(value->AsNumber());
		case EJson::Boolean:
			return MakeShared<FJsonValueBoolean>(value->AsBool());
		case EJson::Array:
		{
			TArray<TSharedPtr<FJsonValue>> clonedArray;
			for (const TSharedPtr<FJsonValue>& item : value->AsArray())
			{
				clonedArray.Add(CloneJsonValue(item));
			}
			return MakeShared<FJsonValueArray>(clonedArray);
		}
		case EJson::Object:
		{
			const TSharedPtr<FJsonObject> clonedObject = MakeShared<FJsonObject>();
			for (const TPair<FString, TSharedPtr<FJsonValue>>& pair : value->AsObject()->Values)
			{
				clonedObject->SetField(pair.Key, CloneJsonValue(pair.Value));
			}
			return MakeShared<FJsonValueObject>(clonedObject);
		}
		default:
			return MakeShared<FJsonValueNull>();
		}
	}

	bool TrySerializeJsonObject(
		const TSharedRef<FJsonObject>& rootObject,
		FString& outJson,
		TArray<FScenarioCompileDiagnostic>& diagnostics)
	{
		outJson.Reset();
		const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&outJson);
		if (!FJsonSerializer::Serialize(rootObject, writer))
		{
			AddUserProjectDiagnostic(
				diagnostics,
				EScenarioCompileDiagnosticSeverity::Error,
				TEXT("json_serialize_failed"),
				TEXT("JSON serialization failed."));
			return false;
		}

		return true;
	}

	bool TryWriteTextFile(
		const FString& filePath,
		const FString& content,
		TArray<FScenarioCompileDiagnostic>& diagnostics)
	{
		const FString outputDirectory = FPaths::GetPath(filePath);
		if (!IFileManager::Get().MakeDirectory(*outputDirectory, true))
		{
			AddUserProjectDiagnostic(
				diagnostics,
				EScenarioCompileDiagnosticSeverity::Error,
				TEXT("directory_create_failed"),
				FString::Printf(TEXT("Directory create failed: %s"), *outputDirectory));
			return false;
		}

		if (!FFileHelper::SaveStringToFile(content, *filePath))
		{
			AddUserProjectDiagnostic(
				diagnostics,
				EScenarioCompileDiagnosticSeverity::Error,
				TEXT("json_file_write_failed"),
				FString::Printf(TEXT("JSON file write failed: %s"), *filePath));
			return false;
		}

		return true;
	}

	FScenarioSampleParamValue MakeScenarioSampleFloatParam(double value)
	{
		FScenarioSampleParamValue paramValue;
		paramValue.Type = EScenarioSampleParamValueType::Float;
		paramValue.FloatValue = value;
		return paramValue;
	}
}

FUserProjectJsonParseResult FUserProjectDataJson::ValidateRootJsonString(
	const FString& jsonString,
	const FString& expectedSchema)
{
	FUserProjectJsonParseResult result;

	TSharedPtr<FJsonObject> rootObject;
	if (!TryParseJsonObject(jsonString, rootObject, result.Diagnostics) || !rootObject.IsValid())
	{
		result.bSuccess = false;
		return result;
	}

	TryReadStringField(*rootObject, TEXT("schema"), TEXT("$"), result.Diagnostics, result.Schema);
	if (!result.Schema.Equals(expectedSchema, ESearchCase::CaseSensitive))
	{
		AddUserProjectDiagnostic(
			result.Diagnostics,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("invalid_schema"),
			FString::Printf(TEXT("$.schema must be '%s'."), *expectedSchema));
	}

	TryReadInt32Field(*rootObject, TEXT("version"), TEXT("$"), result.Diagnostics, result.Version);
	if (result.Version != UserProjectJsonVersion)
	{
		AddUserProjectDiagnostic(
			result.Diagnostics,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("unsupported_version"),
			FString::Printf(TEXT("$.version must be %d."), UserProjectJsonVersion));
	}

	result.bSuccess = !HasUserProjectErrors(result.Diagnostics);
	return result;
}

FUserProjectJsonParseResult FUserProjectDataJson::ValidateRootJsonFile(
	const FString& jsonFilePath,
	const FString& expectedSchema)
{
	FUserProjectJsonParseResult result;
	FString jsonString;
	if (!TryLoadJsonStringFromFile(jsonFilePath, jsonString, result.Diagnostics))
	{
		result.bSuccess = false;
		return result;
	}

	result = ValidateRootJsonString(jsonString, expectedSchema);
	return result;
}

bool FUserProjectDataJson::SaveRootJsonFile(
	const FString& jsonFilePath,
	const FString& jsonString,
	const FString& expectedSchema,
	TArray<FScenarioCompileDiagnostic>& outDiagnostics)
{
	outDiagnostics.Reset();
	const FUserProjectJsonParseResult parseResult = ValidateRootJsonString(jsonString, expectedSchema);
	outDiagnostics.Append(parseResult.Diagnostics);
	if (!parseResult.bSuccess)
	{
		return false;
	}

	return TryWriteTextFile(jsonFilePath, jsonString, outDiagnostics);
}

FString FUserProjectEpisodeScenarioJson::BuildEpisodeId(int32 episodeIndex)
{
	return FString::Printf(TEXT("%06d"), episodeIndex + 1);
}

bool FUserProjectEpisodeScenarioJson::IsValidEpisodeId(const FString& episodeId)
{
	if (episodeId.Len() != 6)
	{
		return false;
	}

	for (int32 index = 0; index < episodeId.Len(); ++index)
	{
		if (!FChar::IsDigit(episodeId[index]))
		{
			return false;
		}
	}

	return true;
}

FUserProjectEpisodeScenarioWriteResult FUserProjectEpisodeScenarioJson::WriteEpisodeScenario(
	const FUserProjectRunSnapshotPaths& paths,
	const FUserProjectRunSetting& setting,
	int32 episodeIndex)
{
	FUserProjectEpisodeScenarioWriteResult result;
	result.EpisodeId = BuildEpisodeId(episodeIndex);
	result.Seed = setting.BaseSeed + episodeIndex;
	result.ScenarioPath = FPaths::Combine(paths.EpisodesPath, result.EpisodeId, TEXT("scenario.json"));

	FString scenarioJson;
	FString profileJson;
	FString settingJson;
	TArray<FScenarioCompileDiagnostic>& diagnostics = result.Diagnostics;
	if (!TryLoadJsonStringFromFile(paths.ScenarioPath, scenarioJson, diagnostics)
		|| !TryLoadJsonStringFromFile(paths.ProfilePath, profileJson, diagnostics)
		|| !TryLoadJsonStringFromFile(paths.SettingPath, settingJson, diagnostics))
	{
		result.bSuccess = false;
		return result;
	}

	const FScenarioDocumentParseResult scenarioParseResult =
		FScenarioDocumentJson::ParseProjectScenarioFromString(scenarioJson);
	AppendScenarioSchemaDiagnostics(scenarioParseResult.Diagnostics, diagnostics);
	if (!scenarioParseResult.bSuccess)
	{
		result.bSuccess = false;
		return result;
	}

	FScenarioSamplerRequest sampleRequest;
	sampleRequest.SampleId = result.EpisodeId;
	sampleRequest.Seed = result.Seed;
	sampleRequest.SourceScenarioRef = MakeRunRelativePath(paths, paths.ScenarioPath);
	sampleRequest.SourceScenarioHash = MakeContentHash(scenarioJson);
	sampleRequest.ProfileRef = MakeRunRelativePath(paths, paths.ProfilePath);
	sampleRequest.ProfileHash = MakeContentHash(profileJson);
	sampleRequest.SettingRef = MakeRunRelativePath(paths, paths.SettingPath);
	sampleRequest.SettingHash = MakeContentHash(settingJson);
	sampleRequest.GeneratorVersion = setting.GeneratorVersion.IsEmpty()
		? FString(FScenarioSampler::GeneratorVersion)
		: setting.GeneratorVersion;

	const FScenarioSamplerResult sampleResult =
		FScenarioSampler::GenerateSample(scenarioParseResult.Document, sampleRequest);
	AppendScenarioSchemaDiagnostics(sampleResult.Diagnostics, diagnostics);
	if (!sampleResult.bSuccess || HasUserProjectErrors(diagnostics))
	{
		result.bSuccess = false;
		return result;
	}

	FScenarioSampleDocument sampleDocument = sampleResult.Document;
	sampleDocument.Scenario.Params.Add(TEXT("time_limit_s"), MakeScenarioSampleFloatParam(setting.MaxDurationSeconds));

	FString outputJson;
	TArray<FScenarioSchemaDiagnostic> sampleWriteDiagnostics;
	const bool bSampleJsonWritten =
		FScenarioSampleJson::TryWriteJson(sampleDocument, outputJson, sampleWriteDiagnostics);
	AppendScenarioSchemaDiagnostics(sampleWriteDiagnostics, diagnostics);
	if (!bSampleJsonWritten
		|| !TryWriteTextFile(result.ScenarioPath, outputJson, diagnostics))
	{
		result.bSuccess = false;
		return result;
	}

	result.ScenarioHash = MakeContentHash(outputJson);
	result.bSuccess = true;
	return result;
}

bool FUserProjectEpisodeScenarioJson::WriteAllEpisodeScenarios(
	const FUserProjectRunSnapshotPaths& paths,
	const FUserProjectRunSetting& setting,
	TArray<FUserProjectEpisodeScenarioWriteResult>& outResults,
	TArray<FScenarioCompileDiagnostic>& outDiagnostics)
{
	outResults.Reset();
	outDiagnostics.Reset();
	if (setting.EpisodeCount <= 0)
	{
		AddUserProjectDiagnostic(
			outDiagnostics,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("invalid_episode_count"),
			TEXT("setting.sampling.episode_count must be > 0."));
		return false;
	}

	for (int32 episodeIndex = 0; episodeIndex < setting.EpisodeCount; ++episodeIndex)
	{
		FUserProjectEpisodeScenarioWriteResult result = WriteEpisodeScenario(paths, setting, episodeIndex);
		outDiagnostics.Append(result.Diagnostics);
		outResults.Add(result);
		if (!result.bSuccess)
		{
			return false;
		}
	}

	return !HasUserProjectErrors(outDiagnostics);
}

FUserProjectEpisodeScenarioParseResult FUserProjectEpisodeScenarioJson::ParseFromString(const FString& jsonString)
{
	FUserProjectEpisodeScenarioParseResult result;

	TSharedPtr<FJsonObject> rootObject;
	if (!TryParseJsonObject(jsonString, rootObject, result.Diagnostics) || !rootObject.IsValid())
	{
		result.bSuccess = false;
		return result;
	}

	FString schema;
	TryReadStringField(*rootObject, TEXT("schema"), TEXT("$"), result.Diagnostics, schema);
	if (!schema.Equals(UserProjectScenarioSampleSchema, ESearchCase::CaseSensitive))
	{
		AddUserProjectDiagnostic(
			result.Diagnostics,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("invalid_schema"),
			FString::Printf(TEXT("$.schema must be '%s'."), UserProjectScenarioSampleSchema));
		result.bSuccess = false;
		return result;
	}

	const FScenarioSampleParseResult sampleParseResult = FScenarioSampleJson::ParseFromString(jsonString);
	AppendScenarioSchemaDiagnostics(sampleParseResult.Diagnostics, result.Diagnostics);
	result.EpisodeId = sampleParseResult.Document.Sample.SampleId;
	result.Seed = sampleParseResult.Document.Sample.Source.Seed;
	if (!IsValidEpisodeId(result.EpisodeId))
	{
		AddUserProjectDiagnostic(
			result.Diagnostics,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("invalid_episode_id"),
			TEXT("$.sample.sample_id must be a 6-digit decimal string for user project episodes."));
	}
	result.bSuccess = sampleParseResult.bSuccess && !HasUserProjectErrors(result.Diagnostics);
	return result;
}

FUserProjectEpisodeScenarioParseResult FUserProjectEpisodeScenarioJson::ParseFromFile(const FString& jsonFilePath)
{
	FUserProjectEpisodeScenarioParseResult result;
	FString jsonString;
	if (!TryLoadJsonStringFromFile(jsonFilePath, jsonString, result.Diagnostics))
	{
		result.bSuccess = false;
		return result;
	}

	result = ParseFromString(jsonString);
	return result;
}

namespace
{
	template <typename TEnum>
	FString ToUserProjectEnumString(TEnum value)
	{
		if (const UEnum* enumValue = StaticEnum<TEnum>())
		{
			return enumValue->GetNameStringByValue(static_cast<int64>(value));
		}

		return TEXT("Unknown");
	}

	// events.jsonl 매핑은 런타임 detector가 JSON 이름을 알지 않도록 typed snapshot 필드만 읽는다.
	FString ReadEventStringProperty(const FEpisodeEvaluationEvent& event, const FString& key)
	{
		if (const FScenarioParamValue* value = event.Properties.Find(key))
		{
			if (value->Type == EScenarioParamValueType::String)
			{
				return value->StringValue.TrimStartAndEnd();
			}
		}

		return FString();
	}

	// 정책 pathfinding 실패는 policy/runtime 계층에서 여러 error code 표기로 들어온다.
	bool IsPathfindFailureCode(const FString& value)
	{
		const FString normalizedValue = value.TrimStartAndEnd();
		return normalizedValue.Equals(TEXT("PathfindFail"), ESearchCase::IgnoreCase)
			|| normalizedValue.Equals(TEXT("PathFindingFailed"), ESearchCase::IgnoreCase)
			|| normalizedValue.Equals(TEXT("PATH_NOT_FOUND"), ESearchCase::IgnoreCase)
			|| normalizedValue.Equals(TEXT("path_not_found"), ESearchCase::IgnoreCase)
			|| normalizedValue.Equals(TEXT("start_cell_blocked"), ESearchCase::IgnoreCase)
			|| normalizedValue.Equals(TEXT("goal_cell_blocked"), ESearchCase::IgnoreCase);
	}

	// DeliveryBotPolicyFailure는 외부 계약의 PathfindFail과 PolicyDecisionError로 나뉜다.
	bool IsPolicyPathfindFailureEvent(const FEpisodeEvaluationEvent& event)
	{
		return IsPathfindFailureCode(ReadEventStringProperty(event, TEXT("error_code")))
			|| IsPathfindFailureCode(ReadEventStringProperty(event, TEXT("policy_event_code")))
			|| IsPathfindFailureCode(ReadEventStringProperty(event, TEXT("policy_reason")));
	}

	// Stuck은 generic DeliveryBotSimulationFailure snapshot의 외부 event_type 세분화다.
	bool IsStuckSimulationFailureEvent(const FEpisodeEvaluationEvent& event)
	{
		return ReadEventStringProperty(event, TEXT("delivery_bot_failure_type")).Equals(TEXT("Stuck"), ESearchCase::IgnoreCase)
			|| ReadEventStringProperty(event, TEXT("failure_type")).Equals(TEXT("Stuck"), ESearchCase::IgnoreCase);
	}

	// 내부 evaluation enum을 문서화된 외부 events.jsonl event_type 값으로 변환한다.
	FString ResolveUserProjectEventType(const FEpisodeEvaluationEvent& event)
	{
		switch (event.EventType)
		{
		case EEpisodeEvaluationEventType::DeliveryBotRepath:
			return TEXT("Repath");
		case EEpisodeEvaluationEventType::DeliveryBotPolicyServerFailure:
			return TEXT("PolicyDecisionError");
		case EEpisodeEvaluationEventType::DeliveryBotPolicyFailure:
			return IsPolicyPathfindFailureEvent(event)
				? TEXT("PathfindFail")
				: TEXT("PolicyDecisionError");
		case EEpisodeEvaluationEventType::DeliveryBotSimulationFailure:
			return IsStuckSimulationFailureEvent(event)
				? TEXT("Stuck")
				: TEXT("DeliveryBotSimulationFailure");
		default:
			return ToUserProjectEnumString(event.EventType);
		}
	}

	// source 이름은 downstream 분석에서 책임 subsystem 경계를 설명한다.
	FString ResolveUserProjectEventSource(const FEpisodeEvaluationEvent& event)
	{
		switch (event.EventType)
		{
		case EEpisodeEvaluationEventType::DeliveryBotRepath:
			return TEXT("PythonPolicy");
		case EEpisodeEvaluationEventType::DeliveryBotPolicyFailure:
			return IsPolicyPathfindFailureEvent(event)
				? TEXT("PythonPolicy")
				: TEXT("PolicyRuntime");
		case EEpisodeEvaluationEventType::DeliveryBotPolicyServerFailure:
			return TEXT("PolicyRuntime");
		default:
			return TEXT("EvaluationSubsystem");
		}
	}

	// reason은 event snapshot에서 얻을 수 있는 가장 구체적인 detector 또는 policy code를 유지한다.
	FString ResolveUserProjectEventReason(const FEpisodeEvaluationEvent& event)
	{
		if (event.EventType == EEpisodeEvaluationEventType::DeliveryBotRepath)
		{
			const FString policyReason = ReadEventStringProperty(event, TEXT("policy_reason"));
			if (!policyReason.IsEmpty()) return policyReason;

			const FString policyEventCode = ReadEventStringProperty(event, TEXT("policy_event_code"));
			if (!policyEventCode.IsEmpty()) return policyEventCode;
		}

		if (event.EventType == EEpisodeEvaluationEventType::DeliveryBotPolicyFailure
			|| event.EventType == EEpisodeEvaluationEventType::DeliveryBotPolicyServerFailure)
		{
			const FString errorCode = ReadEventStringProperty(event, TEXT("error_code"));
			if (!errorCode.IsEmpty()) return errorCode;

			const FString policyReason = ReadEventStringProperty(event, TEXT("policy_reason"));
			if (!policyReason.IsEmpty()) return policyReason;

			const FString policyEventCode = ReadEventStringProperty(event, TEXT("policy_event_code"));
			if (!policyEventCode.IsEmpty()) return policyEventCode;
		}

		if (event.EventType == EEpisodeEvaluationEventType::DeliveryBotSimulationFailure)
		{
			const FString failureType = ReadEventStringProperty(event, TEXT("failure_type"));
			if (!failureType.IsEmpty()) return failureType;

			const FString deliveryBotFailureType = ReadEventStringProperty(event, TEXT("delivery_bot_failure_type"));
			if (!deliveryBotFailureType.IsEmpty()) return deliveryBotFailureType;
		}

		return ResolveUserProjectEventType(event);
	}

	// 계약에 대응되는 terminal reason은 Terminal 대신 같은 event_type 이름으로 기록한다.
	FString ResolveUserProjectTerminalEventType(EEpisodeEvaluationTerminalReason terminalReason)
	{
		switch (terminalReason)
		{
		case EEpisodeEvaluationTerminalReason::GoalReached:
			return TEXT("GoalReached");
		case EEpisodeEvaluationTerminalReason::Timeout:
			return TEXT("Timeout");
		case EEpisodeEvaluationTerminalReason::RobotTipOver:
			return TEXT("RobotTipOver");
		case EEpisodeEvaluationTerminalReason::DeliveryBotSimulationFailed:
			return TEXT("DeliveryBotSimulationFailure");
		default:
			return TEXT("Terminal");
		}
	}

	TSharedPtr<FJsonValue> MakeUserProjectParamJsonValue(const FScenarioParamValue& paramValue)
	{
		switch (paramValue.Type)
		{
		case EScenarioParamValueType::Bool:
			return MakeShared<FJsonValueBoolean>(paramValue.BoolValue);
		case EScenarioParamValueType::Integer:
			return MakeShared<FJsonValueNumber>(paramValue.IntegerValue);
		case EScenarioParamValueType::Float:
			return MakeShared<FJsonValueNumber>(paramValue.FloatValue);
		case EScenarioParamValueType::String:
			return MakeShared<FJsonValueString>(paramValue.StringValue);
		case EScenarioParamValueType::Vector:
		{
			TArray<TSharedPtr<FJsonValue>> vectorValues;
			vectorValues.Add(MakeShared<FJsonValueNumber>(paramValue.VectorValue.X));
			vectorValues.Add(MakeShared<FJsonValueNumber>(paramValue.VectorValue.Y));
			vectorValues.Add(MakeShared<FJsonValueNumber>(paramValue.VectorValue.Z));
			return MakeShared<FJsonValueArray>(vectorValues);
		}
		case EScenarioParamValueType::None:
		default:
			return MakeShared<FJsonValueNull>();
		}
	}

	TSharedRef<FJsonObject> MakeUserProjectParamObject(const TMap<FString, FScenarioParamValue>& params)
	{
		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		TArray<FString> keys;
		params.GetKeys(keys);
		keys.Sort();
		for (const FString& key : keys)
		{
			if (const FScenarioParamValue* paramValue = params.Find(key))
			{
				object->SetField(key, MakeUserProjectParamJsonValue(*paramValue));
			}
		}
		return object;
	}

	bool TrySerializeCondensedJsonLine(const TSharedRef<FJsonObject>& object, FString& outLine)
	{
		outLine.Reset();
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&outLine);
		return FJsonSerializer::Serialize(object, writer);
	}

	bool TryWriteUtf8File(const FString& filePath, const FString& contents, TArray<FString>& outDiagnostics)
	{
		const FString outputDirectory = FPaths::GetPath(filePath);
		if (!IFileManager::Get().MakeDirectory(*outputDirectory, true))
		{
			outDiagnostics.Add(FString::Printf(TEXT("Directory create failed: %s"), *outputDirectory));
			return false;
		}

		if (!FFileHelper::SaveStringToFile(contents, *filePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			outDiagnostics.Add(FString::Printf(TEXT("File write failed: %s"), *filePath));
			return false;
		}

		return true;
	}

	bool TryAppendUtf8File(const FString& filePath, const FString& contents, TArray<FString>& outDiagnostics)
	{
		const FString outputDirectory = FPaths::GetPath(filePath);
		if (!IFileManager::Get().MakeDirectory(*outputDirectory, true))
		{
			outDiagnostics.Add(FString::Printf(TEXT("Directory create failed: %s"), *outputDirectory));
			return false;
		}

		if (!FFileHelper::SaveStringToFile(
				contents,
				*filePath,
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
				&IFileManager::Get(),
				FILEWRITE_Append))
		{
			outDiagnostics.Add(FString::Printf(TEXT("File append failed: %s"), *filePath));
			return false;
		}

		return true;
	}

	bool EnsureJsonlFileExists(const FString& filePath, TArray<FString>& outDiagnostics)
	{
		if (FPaths::FileExists(filePath))
		{
			return true;
		}

		return TryWriteUtf8File(filePath, FString(), outDiagnostics);
	}

	TSharedPtr<FJsonObject> LoadJsonObjectOrEmpty(const FString& filePath)
	{
		FString jsonString;
		TArray<FScenarioCompileDiagnostic> diagnostics;
		TSharedPtr<FJsonObject> object;
		if (TryLoadJsonStringFromFile(filePath, jsonString, diagnostics))
		{
			TryParseJsonObject(jsonString, object, diagnostics);
		}
		return object.IsValid() ? object : MakeShared<FJsonObject>();
	}

	TSharedPtr<FJsonObject> TryGetObjectFieldOrNull(
		const TSharedPtr<FJsonObject>& rootObject,
		const FString& fieldName)
	{
		if (!rootObject.IsValid())
		{
			return nullptr;
		}

		const TSharedPtr<FJsonValue> fieldValue = rootObject->TryGetField(fieldName);
		if (!fieldValue.IsValid() || fieldValue->Type != EJson::Object)
		{
			return nullptr;
		}

		return fieldValue->AsObject();
	}

	FString ReadStringOrDefault(const FJsonObject& object, const FString& fieldName, const FString& defaultValue = FString())
	{
		FString value;
		return object.TryGetStringField(fieldName, value) ? value : defaultValue;
	}

	double ReadNumberOrDefault(const FJsonObject& object, const FString& fieldName, double defaultValue)
	{
		double value = defaultValue;
		return object.TryGetNumberField(fieldName, value) ? value : defaultValue;
	}

	bool ReadBoolOrDefault(const FJsonObject& object, const FString& fieldName, bool defaultValue)
	{
		bool value = defaultValue;
		return object.TryGetBoolField(fieldName, value) ? value : defaultValue;
	}

	FString GetEpisodeIdForOutput(const FEpisodeRunRecord& runRecord)
	{
		if (FUserProjectEpisodeScenarioJson::IsValidEpisodeId(runRecord.EpisodeId))
		{
			return runRecord.EpisodeId;
		}
		if (FUserProjectEpisodeScenarioJson::IsValidEpisodeId(runRecord.PairId))
		{
			return runRecord.PairId;
		}
		return FUserProjectEpisodeScenarioJson::BuildEpisodeId(FMath::Max(0, runRecord.RunIndex));
	}

	TSharedRef<FJsonObject> MakeEpisodeSourceObject(
		const FEpisodeRunRecord& runRecord,
		const TSharedPtr<FJsonObject>& episodeScenarioObject,
		FString& outScenarioId,
		FString& outScenarioHash,
		FString& outScenarioSourceHash,
		FString& outProfileHash,
		FString& outSettingHash,
		int64& outSeed)
	{
		outScenarioId.Reset();
		outScenarioHash = runRecord.EpisodeSetupHash;
		outScenarioSourceHash.Reset();
		outProfileHash = runRecord.DeliveryBotSetupHash;
		outSettingHash.Reset();
		outSeed = 0;

		if (episodeScenarioObject.IsValid())
		{
			TSharedPtr<FJsonObject> sampleObject = TryGetObjectFieldOrNull(episodeScenarioObject, TEXT("sample"));
			if (sampleObject.IsValid())
			{
				outScenarioId = ReadStringOrDefault(*sampleObject, TEXT("scenario_id"));
				TSharedPtr<FJsonObject> sampleSourceObject = TryGetObjectFieldOrNull(sampleObject, TEXT("source"));
				if (sampleSourceObject.IsValid())
				{
					outSeed = static_cast<int64>(ReadNumberOrDefault(*sampleSourceObject, TEXT("seed"), 0.0));
					outScenarioSourceHash = ReadStringOrDefault(*sampleSourceObject, TEXT("template_hash"));
					outProfileHash = ReadStringOrDefault(*sampleSourceObject, TEXT("profile_hash"), outProfileHash);
					outSettingHash = ReadStringOrDefault(*sampleSourceObject, TEXT("setting_hash"));
				}
			}
		}

		if (outScenarioId.IsEmpty())
		{
			outScenarioId = runRecord.EpisodeId;
		}

		FString scenarioJson;
		TArray<FScenarioCompileDiagnostic> diagnostics;
		if (TryLoadJsonStringFromFile(runRecord.EpisodeScenarioJsonPath, scenarioJson, diagnostics))
		{
			outScenarioHash = MakeContentHash(scenarioJson);
		}

		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		object->SetStringField(TEXT("scenario_id"), outScenarioId);
		object->SetStringField(TEXT("scenario_hash"), outScenarioHash);
		object->SetStringField(TEXT("scenario_source_hash"), outScenarioSourceHash);
		object->SetStringField(TEXT("profile_hash"), outProfileHash);
		object->SetStringField(TEXT("setting_hash"), outSettingHash);
		object->SetNumberField(TEXT("seed"), static_cast<double>(outSeed));
		return object;
	}

	bool IsUserProjectRecordUsableForTuning(const FEpisodeRunRecord& runRecord)
	{
		return runRecord.bCompileSucceeded
			&& runRecord.bEpisodeSetupCompileSucceeded
			&& runRecord.bDeliveryBotSetupCompileSucceeded
			&& runRecord.bSetupSucceeded
			&& runRecord.bEvaluationCompleted
			&& runRecord.Outcome != EEpisodeEvaluationOutcome::Cancelled;
	}

	TSharedRef<FJsonObject> MakeEpisodeObject(const FString& episodeId, const TSharedRef<FJsonObject>& sourceObject)
	{
		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		object->SetStringField(TEXT("episode_id"), episodeId);
		object->SetStringField(TEXT("scenario_hash"), sourceObject->GetStringField(TEXT("scenario_hash")));
		object->SetStringField(TEXT("scenario_source_hash"), sourceObject->GetStringField(TEXT("scenario_source_hash")));
		object->SetStringField(TEXT("profile_hash"), sourceObject->GetStringField(TEXT("profile_hash")));
		object->SetStringField(TEXT("setting_hash"), sourceObject->GetStringField(TEXT("setting_hash")));
		object->SetNumberField(TEXT("seed"), sourceObject->GetNumberField(TEXT("seed")));
		return object;
	}

	TSharedRef<FJsonObject> MakeRunObject(const FUserProjectRunSnapshotPaths& paths, const FEpisodeRunRecord& runRecord)
	{
		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		object->SetStringField(TEXT("run_id"), paths.RunId.IsEmpty() ? runRecord.RunId : paths.RunId);
		object->SetStringField(TEXT("policy_snapshot_hash"), MakePolicySnapshotHash(paths));
		return object;
	}

	TSharedRef<FJsonObject> MakeEpisodeSummaryObject(const FEpisodeRunRecord& runRecord)
	{
		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		object->SetBoolField(TEXT("success"), runRecord.bSuccess);
		object->SetStringField(TEXT("outcome"), ToUserProjectEnumString(runRecord.Outcome));
		object->SetStringField(TEXT("terminal_reason"), ToUserProjectEnumString(runRecord.TerminalReason));
		object->SetNumberField(TEXT("duration_s"), runRecord.DurationSeconds);
		object->SetBoolField(TEXT("evaluation_completed"), runRecord.bEvaluationCompleted);
		return object;
	}

	TSharedRef<FJsonObject> MakeEventSummaryObject(const FEpisodeRunRecord& runRecord)
	{
		TMap<FString, int32> eventCounts;
		for (const FEpisodeEvaluationEvent& event : runRecord.EvaluationResult.Events)
		{
			const FString eventType = ResolveUserProjectEventType(event);
			eventCounts.FindOrAdd(eventType) += 1;
		}
		eventCounts.FindOrAdd(ResolveUserProjectTerminalEventType(runRecord.TerminalReason)) += 1;

		TSharedRef<FJsonObject> countsObject = MakeShared<FJsonObject>();
		TArray<FString> eventTypes;
		eventCounts.GetKeys(eventTypes);
		eventTypes.Sort();
		for (const FString& eventType : eventTypes)
		{
			if (const int32* eventCount = eventCounts.Find(eventType))
			{
				countsObject->SetNumberField(eventType, *eventCount);
			}
		}

		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		object->SetNumberField(TEXT("event_count"), runRecord.EvaluationResult.Events.Num() + 1);
		object->SetObjectField(TEXT("by_type"), countsObject);
		return object;
	}

	TSharedRef<FJsonObject> MakeEventLineObject(const FEpisodeEvaluationEvent& event)
	{
		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		object->SetStringField(TEXT("schema"), TEXT("episode_event"));
		object->SetNumberField(TEXT("version"), 1);
		object->SetNumberField(TEXT("event_index"), event.EventIndex);
		object->SetNumberField(TEXT("run_time_seconds"), event.ElapsedTimeSeconds);
		object->SetStringField(TEXT("source"), ResolveUserProjectEventSource(event));
		object->SetStringField(TEXT("event_type"), ResolveUserProjectEventType(event));
		object->SetStringField(TEXT("reason"), ResolveUserProjectEventReason(event));
		object->SetStringField(TEXT("message"), event.Message);
		object->SetField(TEXT("action_sequence"), MakeShared<FJsonValueNull>());
		object->SetObjectField(TEXT("properties"), MakeUserProjectParamObject(event.Properties));
		return object;
	}

	// Terminal event properties reuse evaluation metrics so the final line carries the same snapshot context.
	void SetMetricPropertyIfPresent(
		const TSharedRef<FJsonObject>& object,
		const TMap<FString, FScenarioParamValue>& metrics,
		const FString& metricKey,
		const FString& propertyKey)
	{
		if (const FScenarioParamValue* metricValue = metrics.Find(metricKey))
		{
			object->SetField(propertyKey, MakeUserProjectParamJsonValue(*metricValue));
		}
	}

	// Keeps terminal event details within the documented scalar events.jsonl property names.
	TSharedRef<FJsonObject> MakeTerminalEventPropertiesObject(const FEpisodeRunRecord& runRecord)
	{
		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		const TMap<FString, FScenarioParamValue>& metrics = runRecord.EvaluationResult.Metrics;

		object->SetNumberField(TEXT("duration_s"), runRecord.DurationSeconds);
		SetMetricPropertyIfPresent(object, metrics, TEXT("distance_to_goal_m"), TEXT("distance_to_goal_m"));
		if (!object->HasField(TEXT("distance_to_goal_m")))
		{
			SetMetricPropertyIfPresent(object, metrics, TEXT("goal_distance_m"), TEXT("distance_to_goal_m"));
		}
		SetMetricPropertyIfPresent(object, metrics, TEXT("goal_threshold_m"), TEXT("goal_threshold_m"));
		SetMetricPropertyIfPresent(object, metrics, TEXT("max_duration_s"), TEXT("max_duration_s"));
		SetMetricPropertyIfPresent(object, metrics, TEXT("failure_type"), TEXT("failure_type"));
		SetMetricPropertyIfPresent(object, metrics, TEXT("speed_kmh"), TEXT("speed_kmh"));
		SetMetricPropertyIfPresent(object, metrics, TEXT("roll_degree"), TEXT("roll_degree"));
		SetMetricPropertyIfPresent(object, metrics, TEXT("pitch_degree"), TEXT("pitch_degree"));
		SetMetricPropertyIfPresent(object, metrics, TEXT("threshold_degree"), TEXT("threshold_degree"));
		return object;
	}

	TSharedRef<FJsonObject> MakeTerminalEventLineObject(const FEpisodeRunRecord& runRecord, int32 eventIndex)
	{
		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		object->SetStringField(TEXT("schema"), TEXT("episode_event"));
		object->SetNumberField(TEXT("version"), 1);
		object->SetNumberField(TEXT("event_index"), eventIndex);
		object->SetNumberField(TEXT("run_time_seconds"), runRecord.DurationSeconds);
		object->SetStringField(TEXT("source"), TEXT("EvaluationSubsystem"));
		object->SetStringField(TEXT("event_type"), ResolveUserProjectTerminalEventType(runRecord.TerminalReason));
		object->SetStringField(TEXT("reason"), ToUserProjectEnumString(runRecord.TerminalReason));
		object->SetStringField(TEXT("message"), FString::Printf(TEXT("Episode finished: %s"), *ToUserProjectEnumString(runRecord.TerminalReason)));
		object->SetField(TEXT("action_sequence"), MakeShared<FJsonValueNull>());
		object->SetObjectField(TEXT("properties"), MakeTerminalEventPropertiesObject(runRecord));
		return object;
	}

	bool WriteEpisodeEventsJsonl(
		const FString& eventsPath,
		const FEpisodeRunRecord& runRecord,
		TArray<FString>& outDiagnostics)
	{
		TArray<FString> lines;
		for (const FEpisodeEvaluationEvent& event : runRecord.EvaluationResult.Events)
		{
			FString line;
			if (!TrySerializeCondensedJsonLine(MakeEventLineObject(event), line))
			{
				outDiagnostics.Add(TEXT("Episode event JSONL serialization failed."));
				return false;
			}
			lines.Add(line);
		}

		FString terminalLine;
		if (!TrySerializeCondensedJsonLine(
			MakeTerminalEventLineObject(runRecord, runRecord.EvaluationResult.Events.Num()),
			terminalLine))
		{
			outDiagnostics.Add(TEXT("Terminal episode event JSONL serialization failed."));
			return false;
		}
		lines.Add(terminalLine);

		return TryWriteUtf8File(eventsPath, FString::Join(lines, TEXT("\n")) + TEXT("\n"), outDiagnostics);
	}

	TSharedRef<FJsonObject> CloneObjectFieldOrEmpty(
		const TSharedPtr<FJsonObject>& rootObject,
		const FString& fieldName)
	{
		if (const TSharedPtr<FJsonObject> object = TryGetObjectFieldOrNull(rootObject, fieldName))
		{
			const TSharedPtr<FJsonValue> clonedValue = CloneJsonValue(MakeShared<FJsonValueObject>(object));
			if (clonedValue.IsValid() && clonedValue->Type == EJson::Object)
			{
				return clonedValue->AsObject().ToSharedRef();
			}
		}

		return MakeShared<FJsonObject>();
	}

	TSharedPtr<FJsonValue> CloneFieldOrNull(const FJsonObject& rootObject, const FString& fieldName)
	{
		return CloneJsonValue(rootObject.TryGetField(fieldName));
	}

	TSharedPtr<FJsonObject> TryGetObjectFieldOrNull(
		const FJsonObject& rootObject,
		const FString& fieldName)
	{
		const TSharedPtr<FJsonValue> fieldValue = rootObject.TryGetField(fieldName);
		if (!fieldValue.IsValid() || fieldValue->Type != EJson::Object)
		{
			return nullptr;
		}

		return fieldValue->AsObject();
	}

	const TArray<TSharedPtr<FJsonValue>>* TryGetArrayFieldByName(
		const FJsonObject& rootObject,
		const FString& fieldName)
	{
		const TArray<TSharedPtr<FJsonValue>>* values = nullptr;
		return rootObject.TryGetArrayField(fieldName, values) ? values : nullptr;
	}

	const TArray<TSharedPtr<FJsonValue>>* TryGetArrayFieldByNames(
		const FJsonObject& rootObject,
		const FString& firstFieldName,
		const FString& secondFieldName)
	{
		if (const TArray<TSharedPtr<FJsonValue>>* values = TryGetArrayFieldByName(rootObject, firstFieldName))
		{
			return values;
		}

		return TryGetArrayFieldByName(rootObject, secondFieldName);
	}

	TArray<TSharedPtr<FJsonValue>> CloneArrayFieldOrEmpty(
		const FJsonObject& rootObject,
		const FString& firstFieldName,
		const FString& secondFieldName = FString())
	{
		const TArray<TSharedPtr<FJsonValue>>* values = secondFieldName.IsEmpty()
			? TryGetArrayFieldByName(rootObject, firstFieldName)
			: TryGetArrayFieldByNames(rootObject, firstFieldName, secondFieldName);
		if (!values)
		{
			return {};
		}

		TArray<TSharedPtr<FJsonValue>> clonedValues;
		clonedValues.Reserve(values->Num());
		for (const TSharedPtr<FJsonValue>& value : *values)
		{
			clonedValues.Add(CloneJsonValue(value));
		}
		return clonedValues;
	}

	TSharedRef<FJsonObject> MakePointObjectFromCm(const TSharedPtr<FJsonObject>& pointObject)
	{
		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		if (!pointObject.IsValid())
		{
			return object;
		}

		object->SetNumberField(TEXT("x"), ReadNumberOrDefault(*pointObject, TEXT("x"), 0.0) / 100.0);
		object->SetNumberField(TEXT("y"), ReadNumberOrDefault(*pointObject, TEXT("y"), 0.0) / 100.0);
		object->SetNumberField(TEXT("z"), ReadNumberOrDefault(*pointObject, TEXT("z"), 0.0) / 100.0);
		return object;
	}

	TSharedPtr<FJsonValue> MakePointFieldValueFromCmOrNull(
		const FJsonObject& rootObject,
		const FString& fieldName)
	{
		if (const TSharedPtr<FJsonObject> pointObject = TryGetObjectFieldOrNull(rootObject, fieldName))
		{
			return MakeShared<FJsonValueObject>(MakePointObjectFromCm(pointObject));
		}

		return MakeShared<FJsonValueNull>();
	}

	TArray<TSharedPtr<FJsonValue>> MakePointArrayFromCm(
		const FJsonObject& rootObject,
		const FString& fieldName)
	{
		const TArray<TSharedPtr<FJsonValue>>* sourceValues = TryGetArrayFieldByName(rootObject, fieldName);
		if (!sourceValues)
		{
			return {};
		}

		TArray<TSharedPtr<FJsonValue>> result;
		result.Reserve(sourceValues->Num());
		for (const TSharedPtr<FJsonValue>& sourceValue : *sourceValues)
		{
			if (!sourceValue.IsValid() || sourceValue->Type != EJson::Object)
			{
				continue;
			}

			result.Add(MakeShared<FJsonValueObject>(MakePointObjectFromCm(sourceValue->AsObject())));
		}
		return result;
	}

	void SetTargetIdField(TSharedRef<FJsonObject> targetObject, const FJsonObject& sourceObject, const FString& fieldName)
	{
		const FString targetId = ReadStringOrDefault(sourceObject, fieldName).TrimStartAndEnd();
		if (targetId.IsEmpty())
		{
			targetObject->SetField(TEXT("target_id"), MakeShared<FJsonValueNull>());
			return;
		}

		targetObject->SetStringField(TEXT("target_id"), targetId);
	}

	void SetTargetIdField(TSharedRef<FJsonObject> targetObject, const FJsonObject& sourceObject)
	{
		SetTargetIdField(targetObject, sourceObject, TEXT("targetId"));
	}

	void SetTargetTagsField(TSharedRef<FJsonObject> targetObject, const FJsonObject& sourceObject)
	{
		TArray<TSharedPtr<FJsonValue>> tags = CloneArrayFieldOrEmpty(sourceObject, TEXT("targetTags"), TEXT("actorTags"));
		targetObject->SetArrayField(TEXT("target_tags"), tags);
	}

	TSharedRef<FJsonObject> MakeRobotActionRayObject(
		const FJsonObject& sourceObject,
		bool bIncludeYaw,
		bool bIncludePitch,
		bool bIncludeHitLocation)
	{
		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		object->SetBoolField(TEXT("hit"), ReadBoolOrDefault(sourceObject, TEXT("hit"), false));
		object->SetNumberField(TEXT("distance_m"), ReadNumberOrDefault(sourceObject, TEXT("distanceM"), 0.0));

		double rayIndex = 0.0;
		if (sourceObject.TryGetNumberField(TEXT("rayIndex"), rayIndex))
		{
			object->SetNumberField(TEXT("ray_index"), rayIndex);
		}
		else
		{
			object->SetField(TEXT("ray_index"), MakeShared<FJsonValueNull>());
		}

		if (bIncludeYaw)
		{
			object->SetNumberField(TEXT("yaw_degree"), ReadNumberOrDefault(sourceObject, TEXT("yawDegree"), 0.0));
		}
		if (bIncludePitch)
		{
			object->SetNumberField(TEXT("pitch_degree"), ReadNumberOrDefault(sourceObject, TEXT("pitchDegree"), 0.0));
		}
		if (bIncludeHitLocation)
		{
			object->SetField(TEXT("hit_location_cm"), CloneFieldOrNull(sourceObject, TEXT("hitLocationCm")));
		}

		SetTargetIdField(object, sourceObject);
		SetTargetTagsField(object, sourceObject);
		return object;
	}

	TArray<TSharedPtr<FJsonValue>> MakeRobotActionRayArray(
		const FJsonObject& lidarObject,
		const FString& camelCaseFieldName,
		const FString& snakeCaseFieldName,
		bool bIncludeYaw,
		bool bIncludePitch,
		bool bIncludeHitLocation)
	{
		const TArray<TSharedPtr<FJsonValue>>* sourceValues = TryGetArrayFieldByNames(lidarObject, camelCaseFieldName, snakeCaseFieldName);
		if (!sourceValues)
		{
			return {};
		}

		TArray<TSharedPtr<FJsonValue>> result;
		result.Reserve(sourceValues->Num());
		for (const TSharedPtr<FJsonValue>& sourceValue : *sourceValues)
		{
			if (!sourceValue.IsValid() || sourceValue->Type != EJson::Object)
			{
				continue;
			}

			result.Add(MakeShared<FJsonValueObject>(MakeRobotActionRayObject(
				*sourceValue->AsObject(),
				bIncludeYaw,
				bIncludePitch,
				bIncludeHitLocation)));
		}
		return result;
	}

	FString NormalizePolicyRaySelectionMode(const FString& mode)
	{
		const FString normalizedMode = mode.TrimStartAndEnd().ToLower();
		if (normalizedMode == TEXT("oned") || normalizedMode == TEXT("1d"))
		{
			return TEXT("1d");
		}
		if (normalizedMode == TEXT("twod") || normalizedMode == TEXT("2d"))
		{
			return TEXT("2d");
		}
		if (normalizedMode == TEXT("threed") || normalizedMode == TEXT("3d"))
		{
			return TEXT("3d");
		}
		if (normalizedMode == TEXT("legacy2d"))
		{
			return TEXT("legacy2d");
		}
		if (normalizedMode == TEXT("none"))
		{
			return TEXT("none");
		}
		return normalizedMode;
	}

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

	FString NormalizePolicyRaySelectionSource(const FString& source)
	{
		const FString trimmedSource = source.TrimStartAndEnd();
		if (trimmedSource == TEXT("lidar.rays1d"))
		{
			return TEXT("lidar.rays_1d");
		}
		if (trimmedSource == TEXT("lidar.rays2d"))
		{
			return TEXT("lidar.rays_2d");
		}
		if (trimmedSource == TEXT("lidar.rays3d.nearest_vertical_by_yaw"))
		{
			return TEXT("lidar.rays_3d.nearest_vertical_by_yaw");
		}
		return trimmedSource;
	}

	FString ResolvePolicyRaySelectionModeFromLidar(
		const TSharedPtr<FJsonObject>& lidarObject,
		int32 ray1DCount,
		int32 ray2DCount,
		int32 ray3DCount,
		int32 legacyRayCount)
	{
		const FString lidarMode = lidarObject.IsValid()
			? ReadStringOrDefault(*lidarObject, TEXT("mode"))
			: FString();
		const FString normalizedLidarMode = lidarMode.TrimStartAndEnd().ToLower();

		if (normalizedLidarMode == TEXT("oned") || normalizedLidarMode == TEXT("1d"))
		{
			return TEXT("1d");
		}
		if (normalizedLidarMode == TEXT("twod")
			|| normalizedLidarMode == TEXT("2d")
			|| normalizedLidarMode == TEXT("onedandtwod")
			|| normalizedLidarMode == TEXT("twodandthreed"))
		{
			return TEXT("2d");
		}
		if (normalizedLidarMode == TEXT("threed") || normalizedLidarMode == TEXT("3d"))
		{
			return TEXT("3d");
		}
		if (normalizedLidarMode == TEXT("all"))
		{
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

	int32 CountProjected3DPolicyRays(const TArray<TSharedPtr<FJsonValue>>& ray3DValues)
	{
		TSet<int32> yawKeys;
		for (const TSharedPtr<FJsonValue>& rayValue : ray3DValues)
		{
			if (!rayValue.IsValid() || rayValue->Type != EJson::Object)
			{
				continue;
			}

			double yawDegree = 0.0;
			if (rayValue->AsObject()->TryGetNumberField(TEXT("yaw_degree"), yawDegree))
			{
				yawKeys.Add(FMath::RoundToInt(yawDegree * 100.0));
			}
		}
		return yawKeys.Num();
	}

	bool TryCalculateHorizontalPitchDegree(const TArray<TSharedPtr<FJsonValue>>& ray3DValues, double& outPitchDegree)
	{
		outPitchDegree = 0.0;
		bool bFoundRay = false;
		double bestAbsPitchDegree = TNumericLimits<double>::Max();

		for (const TSharedPtr<FJsonValue>& rayValue : ray3DValues)
		{
			if (!rayValue.IsValid() || rayValue->Type != EJson::Object)
			{
				continue;
			}

			double pitchDegree = 0.0;
			if (!rayValue->AsObject()->TryGetNumberField(TEXT("pitch_degree"), pitchDegree))
			{
				continue;
			}

			const double absPitchDegree = FMath::Abs(pitchDegree);
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

	TSharedRef<FJsonObject> MakePolicyRaySelectionObject(
		const FJsonObject& requestObject,
		const TSharedPtr<FJsonObject>& lidarObject,
		int32 ray1DCount,
		int32 ray2DCount,
		const TArray<TSharedPtr<FJsonValue>>& ray3DValues)
	{
		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();

		const TArray<TSharedPtr<FJsonValue>>* legacyPolicyRays = TryGetArrayFieldByName(requestObject, TEXT("lidarRays"));
		const int32 legacyRayCount = legacyPolicyRays ? legacyPolicyRays->Num() : 0;
		const int32 ray3DCount = ray3DValues.Num();

		TSharedPtr<FJsonObject> selectionObject;
		if (lidarObject.IsValid())
		{
			selectionObject = TryGetObjectFieldOrNull(*lidarObject, TEXT("policyRaySelection"));
			if (!selectionObject.IsValid())
			{
				selectionObject = TryGetObjectFieldOrNull(*lidarObject, TEXT("policy_ray_selection"));
			}
		}

		if (selectionObject.IsValid())
		{
			const FString mode = NormalizePolicyRaySelectionMode(ReadStringOrDefault(
				*selectionObject,
				TEXT("mode"),
				ReadStringOrDefault(*selectionObject, TEXT("family"), TEXT("none"))));
			const FString source = NormalizePolicyRaySelectionSource(ReadStringOrDefault(
				*selectionObject,
				TEXT("source"),
				ResolvePolicyRaySelectionSource(mode)));
			object->SetStringField(TEXT("mode"), mode.IsEmpty() ? TEXT("none") : mode);
			object->SetStringField(TEXT("source"), source.IsEmpty() ? ResolvePolicyRaySelectionSource(mode) : source);
			object->SetNumberField(TEXT("ray_count"), ReadNumberOrDefault(*selectionObject, TEXT("rayCount"), ReadNumberOrDefault(*selectionObject, TEXT("ray_count"), 0.0)));
			if (selectionObject->HasField(TEXT("horizontalPitchDegree")) || selectionObject->HasField(TEXT("horizontal_pitch_degree")))
			{
				object->SetNumberField(TEXT("horizontal_pitch_degree"), ReadNumberOrDefault(*selectionObject, TEXT("horizontalPitchDegree"), ReadNumberOrDefault(*selectionObject, TEXT("horizontal_pitch_degree"), 0.0)));
			}
			else
			{
				object->SetField(TEXT("horizontal_pitch_degree"), MakeShared<FJsonValueNull>());
			}
			return object;
		}

		const FString mode = ResolvePolicyRaySelectionModeFromLidar(
			lidarObject,
			ray1DCount,
			ray2DCount,
			ray3DCount,
			legacyRayCount);
		object->SetStringField(TEXT("mode"), mode);
		object->SetStringField(TEXT("source"), ResolvePolicyRaySelectionSource(mode));

		if (mode == TEXT("2d"))
		{
			object->SetNumberField(TEXT("ray_count"), ray2DCount);
		}
		else if (mode == TEXT("3d"))
		{
			object->SetNumberField(TEXT("ray_count"), CountProjected3DPolicyRays(ray3DValues));
		}
		else if (mode == TEXT("1d"))
		{
			object->SetNumberField(TEXT("ray_count"), ray1DCount);
		}
		else if (mode == TEXT("legacy2d"))
		{
			object->SetNumberField(TEXT("ray_count"), legacyRayCount);
		}
		else
		{
			object->SetNumberField(TEXT("ray_count"), 0);
		}

		double horizontalPitchDegree = 0.0;
		if (mode == TEXT("3d") && TryCalculateHorizontalPitchDegree(ray3DValues, horizontalPitchDegree))
		{
			object->SetNumberField(TEXT("horizontal_pitch_degree"), horizontalPitchDegree);
		}
		else
		{
			object->SetField(TEXT("horizontal_pitch_degree"), MakeShared<FJsonValueNull>());
		}
		return object;
	}

	TSharedRef<FJsonObject> MakeRobotActionLidarObject(const FJsonObject& requestObject)
	{
		const TSharedPtr<FJsonObject> lidarObject = TryGetObjectFieldOrNull(requestObject, TEXT("lidar"));
		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();

		object->SetStringField(TEXT("mode"), lidarObject.IsValid() ? ReadStringOrDefault(*lidarObject, TEXT("mode")) : FString());

		TArray<TSharedPtr<FJsonValue>> ray1DValues;
		TArray<TSharedPtr<FJsonValue>> ray2DValues;
		TArray<TSharedPtr<FJsonValue>> ray3DValues;
		if (lidarObject.IsValid())
		{
			ray1DValues = MakeRobotActionRayArray(*lidarObject, TEXT("rays1d"), TEXT("rays_1d"), false, false, false);
			ray2DValues = MakeRobotActionRayArray(*lidarObject, TEXT("rays2d"), TEXT("rays_2d"), true, false, false);
			ray3DValues = MakeRobotActionRayArray(*lidarObject, TEXT("rays3d"), TEXT("rays_3d"), true, true, true);
		}

		object->SetArrayField(TEXT("rays_1d"), ray1DValues);
		object->SetArrayField(TEXT("rays_2d"), ray2DValues);
		object->SetArrayField(TEXT("rays_3d"), ray3DValues);
		object->SetObjectField(
			TEXT("policy_ray_selection"),
			MakePolicyRaySelectionObject(
				requestObject,
				lidarObject,
				ray1DValues.Num(),
				ray2DValues.Num(),
				ray3DValues));
		return object;
	}

	TSharedRef<FJsonObject> MakeRobotActionObservedObject(const FJsonObject& sourceObject)
	{
		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		SetTargetIdField(object, sourceObject);
		SetTargetTagsField(object, sourceObject);
		object->SetBoolField(TEXT("has_bounds"), ReadBoolOrDefault(sourceObject, TEXT("hasBounds"), false));
		object->SetField(TEXT("bounds_origin_cm"), CloneFieldOrNull(sourceObject, TEXT("boundsOriginCm")));
		object->SetField(TEXT("bounds_extent_cm"), CloneFieldOrNull(sourceObject, TEXT("boundsExtentCm")));
		object->SetField(TEXT("closest_hit_location_cm"), CloneFieldOrNull(sourceObject, TEXT("closestHitLocationCm")));
		object->SetNumberField(TEXT("closest_distance_m"), ReadNumberOrDefault(sourceObject, TEXT("closestDistanceM"), 0.0));
		object->SetNumberField(TEXT("closest_ray_yaw_degree"), ReadNumberOrDefault(sourceObject, TEXT("closestRayYawDegree"), 0.0));
		object->SetNumberField(TEXT("total_hit_ray_count"), ReadNumberOrDefault(sourceObject, TEXT("totalHitRayCount"), 0.0));
		object->SetNumberField(TEXT("front_hit_ray_count"), ReadNumberOrDefault(sourceObject, TEXT("frontHitRayCount"), 0.0));
		object->SetBoolField(TEXT("in_front"), ReadBoolOrDefault(sourceObject, TEXT("inFront"), false));
		return object;
	}

	TArray<TSharedPtr<FJsonValue>> MakeRobotActionObservedObjects(const FJsonObject& requestObject)
	{
		const TArray<TSharedPtr<FJsonValue>>* sourceValues = TryGetArrayFieldByName(requestObject, TEXT("observedObjects"));
		if (!sourceValues)
		{
			return {};
		}

		TArray<TSharedPtr<FJsonValue>> result;
		result.Reserve(sourceValues->Num());
		for (const TSharedPtr<FJsonValue>& sourceValue : *sourceValues)
		{
			if (!sourceValue.IsValid() || sourceValue->Type != EJson::Object)
			{
				continue;
			}

			result.Add(MakeShared<FJsonValueObject>(MakeRobotActionObservedObject(*sourceValue->AsObject())));
		}
		return result;
	}

	TSharedRef<FJsonObject> MakeRobotActionRobotStateObject(const FJsonObject& requestObject)
	{
		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		const TSharedPtr<FJsonObject> robotStateObject = TryGetObjectFieldOrNull(requestObject, TEXT("robotState"));
		if (!robotStateObject.IsValid())
		{
			object->SetNumberField(TEXT("x"), 0.0);
			object->SetNumberField(TEXT("y"), 0.0);
			object->SetNumberField(TEXT("z"), 0.0);
			object->SetNumberField(TEXT("yaw_degree"), 0.0);
			object->SetNumberField(TEXT("speed_kmh"), 0.0);
			object->SetBoolField(TEXT("colliding"), false);
			object->SetField(TEXT("collision_target_id"), MakeShared<FJsonValueNull>());
			object->SetArrayField(TEXT("collision_target_tags"), {});
			return object;
		}

		object->SetNumberField(TEXT("x"), ReadNumberOrDefault(*robotStateObject, TEXT("x"), 0.0) / 100.0);
		object->SetNumberField(TEXT("y"), ReadNumberOrDefault(*robotStateObject, TEXT("y"), 0.0) / 100.0);
		object->SetNumberField(TEXT("z"), ReadNumberOrDefault(*robotStateObject, TEXT("z"), 0.0) / 100.0);
		object->SetNumberField(TEXT("yaw_degree"), ReadNumberOrDefault(*robotStateObject, TEXT("yawDegree"), 0.0));
		object->SetNumberField(TEXT("speed_kmh"), ReadNumberOrDefault(*robotStateObject, TEXT("speedKmh"), 0.0));
		object->SetBoolField(TEXT("colliding"), ReadBoolOrDefault(*robotStateObject, TEXT("colliding"), ReadBoolOrDefault(*robotStateObject, TEXT("bColliding"), false)));

		const FString collisionTargetId = ReadStringOrDefault(*robotStateObject, TEXT("collisionTargetId")).TrimStartAndEnd();
		if (collisionTargetId.IsEmpty())
		{
			object->SetField(TEXT("collision_target_id"), MakeShared<FJsonValueNull>());
		}
		else
		{
			object->SetStringField(TEXT("collision_target_id"), collisionTargetId);
		}
		object->SetArrayField(TEXT("collision_target_tags"), CloneArrayFieldOrEmpty(*robotStateObject, TEXT("collisionTargetTags"), TEXT("collisionActorTags")));
		return object;
	}

	TSharedPtr<FJsonValue> MakeRobotActionActionValue(const TSharedPtr<FJsonObject>& responseObject)
	{
		const TSharedPtr<FJsonObject> actionObject = TryGetObjectFieldOrNull(responseObject, TEXT("action"));
		if (!actionObject.IsValid())
		{
			return MakeShared<FJsonValueNull>();
		}

		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		object->SetNumberField(TEXT("steering"), ReadNumberOrDefault(*actionObject, TEXT("steering"), 0.0));
		object->SetNumberField(TEXT("target_speed_kmh"), ReadNumberOrDefault(*actionObject, TEXT("targetSpeedKmh"), ReadNumberOrDefault(*actionObject, TEXT("target_speed_kmh"), 0.0)));
		object->SetNumberField(TEXT("brake"), ReadNumberOrDefault(*actionObject, TEXT("brake"), 0.0));
		object->SetStringField(TEXT("direction"), ReadStringOrDefault(*actionObject, TEXT("direction"), TEXT("Forward")));
		return MakeShared<FJsonValueObject>(object);
	}

	TSharedPtr<FJsonValue> MakeRobotActionDecisionValue(const TSharedPtr<FJsonObject>& responseObject)
	{
		if (!responseObject.IsValid())
		{
			return MakeShared<FJsonValueNull>();
		}

		TSharedPtr<FJsonObject> decisionObject = TryGetObjectFieldOrNull(*responseObject, TEXT("decision"));
		if (!decisionObject.IsValid())
		{
			decisionObject = TryGetObjectFieldOrNull(*responseObject, TEXT("debug"));
		}

		if (!decisionObject.IsValid())
		{
			return MakeShared<FJsonValueNull>();
		}

		const FString selectedPolicy = ReadStringOrDefault(*decisionObject, TEXT("selectedPolicy"), ReadStringOrDefault(*decisionObject, TEXT("selected_policy"))).TrimStartAndEnd();
		const FString reason = ReadStringOrDefault(*decisionObject, TEXT("reason")).TrimStartAndEnd();
		if (selectedPolicy.IsEmpty() && reason.IsEmpty())
		{
			return MakeShared<FJsonValueNull>();
		}

		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		object->SetStringField(TEXT("selected_policy"), selectedPolicy);
		object->SetStringField(TEXT("reason"), reason);
		return MakeShared<FJsonValueObject>(object);
	}

	TSharedRef<FJsonObject> MakeRobotActionPathObject(const TSharedPtr<FJsonObject>& responseObject)
	{
		TSharedPtr<FJsonObject> pathObject;
		if (responseObject.IsValid())
		{
			pathObject = TryGetObjectFieldOrNull(*responseObject, TEXT("path"));
			if (!pathObject.IsValid())
			{
				pathObject = TryGetObjectFieldOrNull(*responseObject, TEXT("debug"));
			}
		}

		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		if (!pathObject.IsValid())
		{
			object->SetStringField(TEXT("path_status"), TEXT("empty"));
			object->SetNumberField(TEXT("path_index"), 0);
			object->SetNumberField(TEXT("path_length"), 0);
			object->SetNumberField(TEXT("target_path_index"), 0);
			object->SetField(TEXT("target_world_point"), MakeShared<FJsonValueNull>());
			object->SetArrayField(TEXT("path_world_points"), {});
			return object;
		}

		object->SetStringField(TEXT("path_status"), ReadStringOrDefault(*pathObject, TEXT("pathStatus"), ReadStringOrDefault(*pathObject, TEXT("path_status"), TEXT("empty"))));
		object->SetNumberField(TEXT("path_index"), ReadNumberOrDefault(*pathObject, TEXT("pathIndex"), ReadNumberOrDefault(*pathObject, TEXT("path_index"), 0.0)));
		object->SetNumberField(TEXT("path_length"), ReadNumberOrDefault(*pathObject, TEXT("pathLength"), ReadNumberOrDefault(*pathObject, TEXT("path_length"), 0.0)));
		object->SetNumberField(TEXT("target_path_index"), ReadNumberOrDefault(*pathObject, TEXT("targetPathIndex"), ReadNumberOrDefault(*pathObject, TEXT("target_path_index"), 0.0)));
		object->SetField(TEXT("target_world_point"), MakePointFieldValueFromCmOrNull(*pathObject, TEXT("targetWorldPoint")));
		object->SetArrayField(TEXT("path_world_points"), MakePointArrayFromCm(*pathObject, TEXT("pathWorldPoints")));
		return object;
	}

	double CalculateFrontHalfAngleDegree(const FJsonObject& requestObject)
	{
		double frontHalfAngleDegree = 0.0;
		if (requestObject.TryGetNumberField(TEXT("frontHalfAngleDegree"), frontHalfAngleDegree))
		{
			return frontHalfAngleDegree;
		}

		const TArray<TSharedPtr<FJsonValue>>* lidarRayValues = nullptr;
		if (!requestObject.TryGetArrayField(TEXT("lidarRays"), lidarRayValues) || !lidarRayValues)
		{
			return 0.0;
		}

		double halfAngleDegree = 0.0;
		for (const TSharedPtr<FJsonValue>& rayValue : *lidarRayValues)
		{
			if (!rayValue.IsValid() || rayValue->Type != EJson::Object)
			{
				continue;
			}

			double rayYawDegree = 0.0;
			if (rayValue->AsObject()->TryGetNumberField(TEXT("rayYawDegree"), rayYawDegree))
			{
				halfAngleDegree = FMath::Max(halfAngleDegree, FMath::Abs(rayYawDegree));
			}
		}

		return halfAngleDegree;
	}

	bool TryReadResponseStatus(const TSharedPtr<FJsonObject>& responseObject, FString& outStatus)
	{
		outStatus.Reset();
		return responseObject.IsValid() && responseObject->TryGetStringField(TEXT("status"), outStatus);
	}

	TSharedRef<FJsonObject> MakeRobotActionErrorObject(
		const TSharedPtr<FJsonObject>& responseObject,
		int32 httpStatusCode,
		const FString& errorMessage)
	{
		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		object->SetNumberField(TEXT("http_status"), httpStatusCode);
		object->SetStringField(TEXT("message"), errorMessage.IsEmpty() ? TEXT("policy action failed") : errorMessage);

		FString responseStatus;
		if (TryReadResponseStatus(responseObject, responseStatus))
		{
			object->SetStringField(TEXT("response_status"), responseStatus);
		}

		return object;
	}

	TSharedRef<FJsonObject> MakeRobotActionLineObject(
		const FJsonObject& requestObject,
		const TSharedPtr<FJsonObject>& responseObject,
		bool bActionSucceeded,
		int32 httpStatusCode,
		const FString& errorMessage)
	{
		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		object->SetStringField(TEXT("schema"), TEXT("robot_action"));
		object->SetNumberField(TEXT("version"), 1);
		object->SetNumberField(TEXT("sequence"), ReadNumberOrDefault(requestObject, TEXT("sequence"), 0.0));
		object->SetNumberField(TEXT("run_time_seconds"), ReadNumberOrDefault(requestObject, TEXT("runTimeSeconds"), 0.0));
		object->SetNumberField(TEXT("sensor_sequence"), ReadNumberOrDefault(requestObject, TEXT("sensorSequence"), 0.0));
		object->SetNumberField(TEXT("sensor_time_seconds"), ReadNumberOrDefault(requestObject, TEXT("sensorTimeSeconds"), 0.0));
		object->SetStringField(TEXT("status"), bActionSucceeded ? TEXT("ok") : TEXT("error"));
		object->SetNumberField(TEXT("front_half_angle_degree"), CalculateFrontHalfAngleDegree(requestObject));
		object->SetObjectField(TEXT("lidar"), MakeRobotActionLidarObject(requestObject));
		object->SetArrayField(TEXT("observed_objects"), MakeRobotActionObservedObjects(requestObject));
		object->SetObjectField(TEXT("robot_state"), MakeRobotActionRobotStateObject(requestObject));

		if (responseObject.IsValid())
		{
			object->SetField(TEXT("action"), MakeRobotActionActionValue(responseObject));
			object->SetField(TEXT("decision"), MakeRobotActionDecisionValue(responseObject));
			object->SetObjectField(TEXT("path"), MakeRobotActionPathObject(responseObject));
		}
		else
		{
			object->SetField(TEXT("action"), MakeShared<FJsonValueNull>());
			object->SetField(TEXT("decision"), MakeShared<FJsonValueNull>());
			object->SetObjectField(TEXT("path"), MakeRobotActionPathObject(nullptr));
		}

		if (!bActionSucceeded)
		{
			object->SetObjectField(TEXT("error"), MakeRobotActionErrorObject(responseObject, httpStatusCode, errorMessage));
		}

		return object;
	}

	TArray<TSharedPtr<FJsonValue>> MakeTraceVectorArray(const FVector& vector)
	{
		TArray<TSharedPtr<FJsonValue>> values;
		values.Reserve(3);
		values.Add(MakeShared<FJsonValueNumber>(vector.X));
		values.Add(MakeShared<FJsonValueNumber>(vector.Y));
		values.Add(MakeShared<FJsonValueNumber>(vector.Z));
		return values;
	}

	TArray<TSharedPtr<FJsonValue>> MakeTraceQuatArray(const TArray<double>& rotationQuatXyzw)
	{
		TArray<TSharedPtr<FJsonValue>> values;
		values.Reserve(4);
		for (int32 index = 0; index < 4; ++index)
		{
			const double value = rotationQuatXyzw.IsValidIndex(index)
				? rotationQuatXyzw[index]
				: (index == 3 ? 1.0 : 0.0);
			values.Add(MakeShared<FJsonValueNumber>(value));
		}
		return values;
	}

	TSharedRef<FJsonObject> MakeTraceActorStateObject(
		const FEpisodeMeasurementLogActorState& actorState,
		bool bIncludeActorIndex)
	{
		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		if (bIncludeActorIndex)
		{
			object->SetNumberField(TEXT("actor_index"), actorState.ActorIndex);
		}
		object->SetArrayField(TEXT("position_cm"), MakeTraceVectorArray(actorState.PositionCm));
		object->SetArrayField(TEXT("rotation_quat_xyzw"), MakeTraceQuatArray(actorState.RotationQuatXyzw));
		object->SetArrayField(TEXT("velocity_cm_per_s"), MakeTraceVectorArray(actorState.VelocityCmPerSecond));
		return object;
	}

	TSharedRef<FJsonObject> MakeEpisodeTraceLineObject(
		const FEpisodeMeasurementLogTickRecord& tickRecord,
		int32 sampleIndex)
	{
		TArray<TSharedPtr<FJsonValue>> actorValues;
		actorValues.Reserve(tickRecord.MovingActors.Num());
		for (const FEpisodeMeasurementLogActorState& actorState : tickRecord.MovingActors)
		{
			actorValues.Add(MakeShared<FJsonValueObject>(MakeTraceActorStateObject(actorState, true)));
		}

		TSharedRef<FJsonObject> robotObject = MakeTraceActorStateObject(tickRecord.Robot.Truth, false);
		robotObject->SetStringField(TEXT("id"), tickRecord.Robot.Id);

		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		object->SetStringField(TEXT("schema"), TEXT("episode_trace"));
		object->SetNumberField(TEXT("version"), 1);
		object->SetNumberField(TEXT("sample_index"), sampleIndex);
		object->SetNumberField(TEXT("run_time_seconds"), tickRecord.WorldTimeSeconds);
		object->SetNumberField(TEXT("delta_seconds"), tickRecord.DeltaSeconds);
		object->SetObjectField(TEXT("robot"), robotObject);
		object->SetArrayField(TEXT("actors"), actorValues);
		return object;
	}

	TSharedRef<FJsonObject> MakeSummaryRowObject(
		const FUserProjectRunSnapshotPaths& paths,
		const FEpisodeRunRecord& runRecord)
	{
		const FString episodeId = GetEpisodeIdForOutput(runRecord);
		const TSharedPtr<FJsonObject> episodeScenarioObject = LoadJsonObjectOrEmpty(runRecord.EpisodeScenarioJsonPath);
		FString scenarioId;
		FString scenarioHash;
		FString scenarioSourceHash;
		FString profileHash;
		FString settingHash;
		int64 seed = 0;
		const TSharedRef<FJsonObject> sourceObject = MakeEpisodeSourceObject(
			runRecord,
			episodeScenarioObject,
			scenarioId,
			scenarioHash,
			scenarioSourceHash,
			profileHash,
			settingHash,
			seed);

		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		object->SetStringField(TEXT("episode_id"), episodeId);
		object->SetStringField(TEXT("scenario_id"), scenarioId);
		object->SetStringField(TEXT("scenario_hash"), scenarioHash);
		object->SetStringField(TEXT("scenario_source_hash"), scenarioSourceHash);
		object->SetStringField(TEXT("profile_hash"), profileHash);
		object->SetStringField(TEXT("setting_hash"), settingHash);
		object->SetNumberField(TEXT("seed"), static_cast<double>(seed));
		object->SetStringField(TEXT("outcome"), ToUserProjectEnumString(runRecord.Outcome));
		object->SetStringField(TEXT("terminal_reason"), ToUserProjectEnumString(runRecord.TerminalReason));
		object->SetNumberField(TEXT("duration_s"), runRecord.DurationSeconds);
		object->SetBoolField(TEXT("usable_for_llm_tuning"), IsUserProjectRecordUsableForTuning(runRecord));
		object->SetObjectField(TEXT("metrics"), MakeUserProjectParamObject(runRecord.EvaluationResult.Metrics));
		const TSharedPtr<FJsonObject> scenarioPayloadObject = TryGetObjectFieldOrNull(episodeScenarioObject, TEXT("scenario"));
		object->SetObjectField(TEXT("scenario_params"), CloneObjectFieldOrEmpty(scenarioPayloadObject, TEXT("params")));
		object->SetObjectField(TEXT("scenario_semantic"), CloneObjectFieldOrEmpty(scenarioPayloadObject, TEXT("semantic")));
		(void)paths;
		(void)sourceObject;
		return object;
	}
}

FString FUserProjectRunOutputJson::BuildEpisodeDirectory(
	const FUserProjectRunSnapshotPaths& paths,
	const FString& episodeId)
{
	return FPaths::Combine(paths.EpisodesPath, episodeId);
}

bool FUserProjectRunOutputJson::SaveEpisodeArtifacts(
	const FUserProjectRunSnapshotPaths& paths,
	const FEpisodeRunRecord& runRecord,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	const FString episodeId = GetEpisodeIdForOutput(runRecord);
	const FString episodeDirectory = BuildEpisodeDirectory(paths, episodeId);
	const FString resultPath = FPaths::Combine(episodeDirectory, TEXT("result.json"));
	const FString actionsPath = FPaths::Combine(episodeDirectory, TEXT("actions.jsonl"));
	const FString eventsPath = FPaths::Combine(episodeDirectory, TEXT("events.jsonl"));
	const FString tracePath = FPaths::Combine(episodeDirectory, TEXT("trace.jsonl"));

	const TSharedPtr<FJsonObject> episodeScenarioObject = LoadJsonObjectOrEmpty(runRecord.EpisodeScenarioJsonPath);
	FString scenarioId;
	FString scenarioHash;
	FString scenarioSourceHash;
	FString profileHash;
	FString settingHash;
	int64 seed = 0;
	const TSharedRef<FJsonObject> sourceObject = MakeEpisodeSourceObject(
		runRecord,
		episodeScenarioObject,
		scenarioId,
		scenarioHash,
		scenarioSourceHash,
		profileHash,
		settingHash,
		seed);

	TSharedRef<FJsonObject> rootObject = MakeShared<FJsonObject>();
	rootObject->SetStringField(TEXT("schema"), TEXT("episode_result"));
	rootObject->SetNumberField(TEXT("version"), 1);
	rootObject->SetObjectField(TEXT("episode"), MakeEpisodeObject(episodeId, sourceObject));
	rootObject->SetObjectField(TEXT("run"), MakeRunObject(paths, runRecord));
	rootObject->SetObjectField(TEXT("summary"), MakeEpisodeSummaryObject(runRecord));
	rootObject->SetObjectField(TEXT("metrics"), MakeUserProjectParamObject(runRecord.EvaluationResult.Metrics));
	rootObject->SetObjectField(TEXT("event_summary"), MakeEventSummaryObject(runRecord));

	FString resultJson;
	TArray<FScenarioCompileDiagnostic> serializationDiagnostics;
	if (!TrySerializeJsonObject(rootObject, resultJson, serializationDiagnostics))
	{
		for (const FScenarioCompileDiagnostic& diagnostic : serializationDiagnostics)
		{
			outDiagnostics.Add(diagnostic.Message);
		}
		return false;
	}

	bool bSuccess = TryWriteUtf8File(resultPath, resultJson, outDiagnostics);
	bSuccess = WriteEpisodeEventsJsonl(eventsPath, runRecord, outDiagnostics) && bSuccess;
	bSuccess = EnsureJsonlFileExists(actionsPath, outDiagnostics) && bSuccess;
	bSuccess = EnsureJsonlFileExists(tracePath, outDiagnostics) && bSuccess;
	return bSuccess;
}

bool FUserProjectRunOutputJson::SaveRunSummary(
	const FUserProjectRunSnapshotPaths& paths,
	const TArray<FEpisodeRunRecord>& runRecords,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();

	TArray<TSharedPtr<FJsonValue>> rows;
	rows.Reserve(runRecords.Num());
	for (const FEpisodeRunRecord& runRecord : runRecords)
	{
		rows.Add(MakeShared<FJsonValueObject>(MakeSummaryRowObject(paths, runRecord)));
	}

	TSharedRef<FJsonObject> runObject = MakeShared<FJsonObject>();
	runObject->SetStringField(TEXT("run_id"), paths.RunId);
	runObject->SetStringField(TEXT("project_id"), FPaths::GetCleanFilename(paths.ProjectPath));
	runObject->SetStringField(TEXT("started_at"), FDateTime::UtcNow().ToString(TEXT("%Y-%m-%dT%H:%M:%SZ")));
	runObject->SetStringField(TEXT("ended_at"), FDateTime::UtcNow().ToString(TEXT("%Y-%m-%dT%H:%M:%SZ")));

	runObject->SetStringField(TEXT("policy_snapshot_hash"), MakePolicySnapshotHash(paths));

	TSharedRef<FJsonObject> rootObject = MakeShared<FJsonObject>();
	rootObject->SetStringField(TEXT("schema"), TEXT("run_summary"));
	rootObject->SetNumberField(TEXT("version"), 1);
	rootObject->SetObjectField(TEXT("run"), runObject);
	rootObject->SetArrayField(TEXT("rows"), rows);

	FString summaryJson;
	TArray<FScenarioCompileDiagnostic> serializationDiagnostics;
	if (!TrySerializeJsonObject(rootObject, summaryJson, serializationDiagnostics))
	{
		for (const FScenarioCompileDiagnostic& diagnostic : serializationDiagnostics)
		{
			outDiagnostics.Add(diagnostic.Message);
		}
		return false;
	}

	return TryWriteUtf8File(paths.SummaryPath, summaryJson, outDiagnostics);
}

bool FUserProjectRunOutputJson::AppendRobotActionRecord(
	const FUserProjectRunSnapshotPaths& paths,
	const FString& episodeId,
	const TSharedRef<FJsonObject>& requestObject,
	const TSharedPtr<FJsonObject>& responseObject,
	bool bActionSucceeded,
	int32 httpStatusCode,
	const FString& errorMessage,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();

	if (!FUserProjectEpisodeScenarioJson::IsValidEpisodeId(episodeId))
	{
		outDiagnostics.Add(FString::Printf(TEXT("Invalid episode id for action record: %s"), *episodeId));
		return false;
	}

	const FString actionPath = FPaths::Combine(BuildEpisodeDirectory(paths, episodeId), TEXT("actions.jsonl"));

	FString actionLine;
	if (!TrySerializeCondensedJsonLine(
			MakeRobotActionLineObject(requestObject.Get(), responseObject, bActionSucceeded, httpStatusCode, errorMessage),
			actionLine))
	{
		outDiagnostics.Add(TEXT("Robot action JSONL serialization failed."));
		return false;
	}

	return TryAppendUtf8File(actionPath, actionLine + TEXT("\n"), outDiagnostics);
}

bool FUserProjectRunOutputJson::AppendEpisodeTraceRecord(
	const FUserProjectRunSnapshotPaths& paths,
	const FString& episodeId,
	const FEpisodeMeasurementLogTickRecord& tickRecord,
	int32 sampleIndex,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();

	if (!FUserProjectEpisodeScenarioJson::IsValidEpisodeId(episodeId))
	{
		outDiagnostics.Add(FString::Printf(TEXT("Invalid episode id for trace record: %s"), *episodeId));
		return false;
	}

	const FString tracePath = FPaths::Combine(BuildEpisodeDirectory(paths, episodeId), TEXT("trace.jsonl"));
	return AppendEpisodeTraceRecordToFile(tracePath, tickRecord, sampleIndex, outDiagnostics);
}

bool FUserProjectRunOutputJson::AppendEpisodeTraceRecordToFile(
	const FString& traceJsonlPath,
	const FEpisodeMeasurementLogTickRecord& tickRecord,
	int32 sampleIndex,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	if (traceJsonlPath.TrimStartAndEnd().IsEmpty())
	{
		outDiagnostics.Add(TEXT("Trace path must not be empty."));
		return false;
	}

	FString traceLine;
	if (!TrySerializeCondensedJsonLine(MakeEpisodeTraceLineObject(tickRecord, sampleIndex), traceLine))
	{
		outDiagnostics.Add(TEXT("Episode trace JSONL serialization failed."));
		return false;
	}

	return TryAppendUtf8File(traceJsonlPath, traceLine + TEXT("\n"), outDiagnostics);
}
