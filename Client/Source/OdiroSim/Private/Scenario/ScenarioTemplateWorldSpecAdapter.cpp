#include "Scenario/ScenarioTemplateWorldSpecAdapter.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Scenario/ScenarioSampleWorldSpecAdapter.h"
#include "Scenario/ScenarioSimulationProfileAdapter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Shared/ScenarioTemplateJson.h"
#include "Shared/Struct/DeliveryBot/Setup/DeliveryBotLocationSetupInfo.h"

namespace
{
constexpr const TCHAR* ScenarioTemplateSchemaName = TEXT("scenario_template");
constexpr const TCHAR* DefaultUnsetHash = TEXT("hash:unset");
constexpr const TCHAR* DefaultSimulationProfileJsonPath = TEXT("Json/Input/ScenarioTemplates/TemplateProfileForTest.json");

FString ScenarioTemplateResolveProjectPath(const FString& JsonFilePath)
{
	if (FPaths::IsRelative(JsonFilePath))
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), JsonFilePath));
	}

	return FPaths::ConvertRelativePathToFull(JsonFilePath);
}

FString ScenarioTemplateMakeProjectRelativePath(const FString& JsonFilePath)
{
	FString FullPath = ScenarioTemplateResolveProjectPath(JsonFilePath);
	FPaths::NormalizeFilename(FullPath);

	FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeFilename(ProjectDir);

	FString RelativePath = FullPath;
	if (FPaths::MakePathRelativeTo(RelativePath, *ProjectDir))
	{
		return RelativePath;
	}

	return FullPath;
}

FString ScenarioTemplateSanitizeToken(FString Token, const FString& Fallback)
{
	Token = Token.ToLower();
	Token.ReplaceInline(TEXT("\\"), TEXT("_"));
	Token.ReplaceInline(TEXT("/"), TEXT("_"));
	Token.ReplaceInline(TEXT("."), TEXT("_"));
	Token.ReplaceInline(TEXT("-"), TEXT("_"));
	Token.ReplaceInline(TEXT(" "), TEXT("_"));
	Token.ReplaceInline(TEXT(":"), TEXT("_"));

	while (Token.Contains(TEXT("__")))
	{
		Token.ReplaceInline(TEXT("__"), TEXT("_"));
	}

	while (Token.StartsWith(TEXT("_")))
	{
		Token.RightChopInline(1);
	}

	while (Token.EndsWith(TEXT("_")))
	{
		Token.LeftChopInline(1);
	}

	return Token.IsEmpty() ? Fallback : Token;
}

FScenarioCompileDiagnostic ScenarioTemplateMakeCompileDiagnostic(
	const FScenarioSchemaDiagnostic& Diagnostic)
{
	FScenarioCompileDiagnostic CompileDiagnostic;
	CompileDiagnostic.Code = Diagnostic.Code;
	CompileDiagnostic.Message = Diagnostic.Message;
	CompileDiagnostic.Severity = EScenarioCompileDiagnosticSeverity::Info;
	if (Diagnostic.Severity == EScenarioSchemaDiagnosticSeverity::Error)
	{
		CompileDiagnostic.Severity = EScenarioCompileDiagnosticSeverity::Error;
	}
	else if (Diagnostic.Severity == EScenarioSchemaDiagnosticSeverity::Warning ||
		Diagnostic.Severity == EScenarioSchemaDiagnosticSeverity::Repair)
	{
		CompileDiagnostic.Severity = EScenarioCompileDiagnosticSeverity::Warning;
	}
	return CompileDiagnostic;
}

void ScenarioTemplateAppendSchemaDiagnosticsToCompileResult(
	const TArray<FScenarioSchemaDiagnostic>& Diagnostics,
	FScenarioCompileResult& CompileResult)
{
	for (const FScenarioSchemaDiagnostic& Diagnostic : Diagnostics)
	{
		CompileResult.Diagnostics.Add(ScenarioTemplateMakeCompileDiagnostic(Diagnostic));
	}
}

bool ScenarioTemplateTryReadSchema(const FString& JsonFilePath, FString& OutSchema)
{
	const FString ResolvedPath = ScenarioTemplateResolveProjectPath(JsonFilePath);

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *ResolvedPath))
	{
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		return false;
	}

	return RootObject->TryGetStringField(TEXT("schema"), OutSchema);
}

FString ScenarioTemplateHashFileText(const FString& JsonFilePath)
{
	const FString ResolvedPath = ScenarioTemplateResolveProjectPath(JsonFilePath);

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *ResolvedPath))
	{
		return DefaultUnsetHash;
	}

	return FString::Printf(TEXT("hash:%u"), GetTypeHash(JsonText));
}

bool ScenarioTemplateApplyProfileSetupToWorldSpec(
	FScenarioWorldSpec& WorldSpec,
	const FDeliveryBotSetupInfo& ProfileSetupInfo)
{
	bool bApplied = false;
	for (FScenarioPlaceableInstanceSpec& PlaceableSpec : WorldSpec.Placeables)
	{
		if (PlaceableSpec.Category != EScenarioActorCategory::DeliveryBot)
		{
			continue;
		}

		const FDeliveryBotLocationSetupInfo LocationSetupInfo = PlaceableSpec.DeliveryBot.SetupInfo.LocationSetupInfo;
		FDeliveryBotSetupInfo MergedSetupInfo = ProfileSetupInfo;
		MergedSetupInfo.LocationSetupInfo = LocationSetupInfo;
		PlaceableSpec.DeliveryBot.SetupInfo = MergedSetupInfo;
		bApplied = true;
	}

	return bApplied;
}
}

bool FScenarioTemplateWorldSpecAdapter::IsScenarioTemplateFile(const FString& JsonFilePath)
{
	FString Schema;
	return ScenarioTemplateTryReadSchema(JsonFilePath, Schema) && Schema == ScenarioTemplateSchemaName;
}

FScenarioTemplateSampleRequest FScenarioTemplateWorldSpecAdapter::MakeDefaultSampleRequest(
	const FString& TemplateJsonPath,
	const FString& PairId)
{
	FScenarioTemplateSampleRequest Request;

	const FString ProjectRelativeTemplatePath = ScenarioTemplateMakeProjectRelativePath(TemplateJsonPath);
	const FString TemplateHash = ScenarioTemplateHashFileText(TemplateJsonPath);
	const FString SeedSource = FString::Printf(TEXT("%s|%s|%s"), *PairId, *ProjectRelativeTemplatePath, *TemplateHash);
	const int64 StableSeed = static_cast<int64>(GetTypeHash(SeedSource));

	const FString BaseName = FPaths::GetBaseFilename(ProjectRelativeTemplatePath);
	const FString SampleToken = ScenarioTemplateSanitizeToken(
		PairId.IsEmpty() ? FString::Printf(TEXT("%s_%lld"), *BaseName, StableSeed) : PairId,
		TEXT("scenario_template_sample"));

	Request.SampleId = SampleToken;
	Request.ScenarioId = ScenarioTemplateSanitizeToken(BaseName, TEXT("scenario_template"));
	Request.Seed = StableSeed;
	Request.TemplateRef = ProjectRelativeTemplatePath;
	Request.TemplateHash = TemplateHash;
	Request.ProfileRef = ScenarioTemplateMakeProjectRelativePath(DefaultSimulationProfileJsonPath);
	Request.ProfileHash = FScenarioSimulationProfileAdapter::MakeProfileFileHash(DefaultSimulationProfileJsonPath);
	Request.SettingRef = TEXT("runtime_setting_unset");
	Request.SettingHash = DefaultUnsetHash;
	Request.GeneratorVersion = FScenarioTemplateSampler::GeneratorVersion;

	return Request;
}

FScenarioTemplateWorldSpecCompileResult FScenarioTemplateWorldSpecAdapter::CompileScenarioWorldSpecFromTemplateFile(
	const FString& JsonFilePath,
	const FScenarioTemplateSampleRequest& Request)
{
	FScenarioTemplateWorldSpecCompileResult Result;

	FScenarioTemplateParseResult ParseResult = FScenarioTemplateJson::ParseFromFile(JsonFilePath);
	Result.SamplingDiagnostics.Append(ParseResult.Diagnostics);
	ScenarioTemplateAppendSchemaDiagnosticsToCompileResult(ParseResult.Diagnostics, Result.CompileResult);

	if (!ParseResult.bSuccess)
	{
		Result.bSuccess = false;
		Result.CompileResult.bSuccess = false;
		return Result;
	}

	Result = CompileScenarioWorldSpecFromTemplateDocument(ParseResult.Document, Request);
	Result.SamplingDiagnostics.Insert(ParseResult.Diagnostics, 0);
	ScenarioTemplateAppendSchemaDiagnosticsToCompileResult(ParseResult.Diagnostics, Result.CompileResult);
	Result.bSuccess = Result.bSuccess && ParseResult.bSuccess;
	Result.CompileResult.bSuccess = Result.CompileResult.bSuccess && Result.bSuccess;
	return Result;
}

FScenarioTemplateWorldSpecCompileResult FScenarioTemplateWorldSpecAdapter::CompileScenarioWorldSpecFromTemplateDocument(
	const FScenarioTemplateDocument& TemplateDocument,
	const FScenarioTemplateSampleRequest& Request)
{
	FScenarioTemplateWorldSpecCompileResult Result;

	FScenarioTemplateSampleResult SampleResult = FScenarioTemplateSampler::GenerateSample(TemplateDocument, Request);
	Result.SampleDocument = SampleResult.Document;
	Result.SamplingDiagnostics = SampleResult.Diagnostics;

	if (!SampleResult.bSuccess)
	{
		Result.bSuccess = false;
		Result.CompileResult.bSuccess = false;
		ScenarioTemplateAppendSchemaDiagnosticsToCompileResult(SampleResult.Diagnostics, Result.CompileResult);
		return Result;
	}

	Result.CompileResult = FScenarioSampleWorldSpecAdapter::CompileScenarioWorldSpecFromSampleDocument(Result.SampleDocument);
	ScenarioTemplateAppendSchemaDiagnosticsToCompileResult(SampleResult.Diagnostics, Result.CompileResult);

	if (Result.CompileResult.bSuccess)
	{
		const FScenarioSimulationProfileCompileResult ProfileResult =
			FScenarioSimulationProfileAdapter::CompileProfileFromJsonFile(Request.ProfileRef);
		Result.CompileResult.Diagnostics.Append(ProfileResult.Diagnostics);

		if (!ProfileResult.bSuccess)
		{
			Result.bSuccess = false;
			Result.CompileResult.bSuccess = false;
			return Result;
		}

		if (!ScenarioTemplateApplyProfileSetupToWorldSpec(Result.CompileResult.WorldSpec, ProfileResult.SetupInfo))
		{
			FScenarioCompileDiagnostic Diagnostic;
			Diagnostic.Severity = EScenarioCompileDiagnosticSeverity::Error;
			Diagnostic.Code = TEXT("profile_robot_missing");
			Diagnostic.Message = TEXT("simulation_profile could not be applied because the sampled world spec has no DeliveryBot placeable.");
			Result.CompileResult.Diagnostics.Add(Diagnostic);
			Result.bSuccess = false;
			Result.CompileResult.bSuccess = false;
			return Result;
		}
	}

	Result.bSuccess = SampleResult.bSuccess && Result.CompileResult.bSuccess;
	Result.CompileResult.bSuccess = Result.bSuccess;
	return Result;
}
